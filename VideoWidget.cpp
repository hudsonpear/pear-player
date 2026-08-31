#include "VideoWidget.h"
#include "MpvPlayer.h"

#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QContextMenuEvent>
#include <QResizeEvent>
#include <QLabel>
#include <QTimer>
#include <QFont>

#include <algorithm>

VideoWidget::VideoWidget(QWidget *parent)
    : QOpenGLWidget(parent)
{
    // We draw the whole surface every frame (video), so avoid Qt clearing
    // it for us and avoid partial-update paths that can cause flicker.
    setUpdateBehavior(QOpenGLWidget::NoPartialUpdate);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);

    // Placeholder for audio-only media. A plain child QLabel layered on top
    // -- Qt's QOpenGLWidget docs explicitly support this -- rather than
    // drawing it inside paintGL(): mixing a QPainter into the same GL
    // context mpv renders with corrupts GL state (blend/shader/viewport)
    // that mpv doesn't reset on the next frame, causing visible corruption.
    audioOnlyIndicator_ = new QLabel(QString::fromUtf8("\xF0\x9F\x8E\xB5"), this); // musical note
    audioOnlyIndicator_->setAlignment(Qt::AlignCenter);
    audioOnlyIndicator_->setStyleSheet(QStringLiteral("color: rgba(255, 255, 255, 160); background: transparent;"));
    audioOnlyIndicator_->setAttribute(Qt::WA_TransparentForMouseEvents);
    audioOnlyIndicator_->hide();

    // Same layered-QLabel approach for the OSD. mpv can draw its own, but only
    // while its renderer runs -- which it does not for audio-only files, where
    // paintGL() clears the surface instead. Drawing it here means one OSD that
    // behaves the same for video, audio and stills.
    osdLabel_ = new QLabel(this);
    osdLabel_->setStyleSheet(QStringLiteral(
        "color: white;"
        "background: rgba(0, 0, 0, 160);"
        "border-radius: 4px;"
        "padding: 6px 10px;"
        "font-size: 15px;"));
    osdLabel_->setAttribute(Qt::WA_TransparentForMouseEvents);
    osdLabel_->hide();

    osdTimer_ = new QTimer(this);
    osdTimer_->setSingleShot(true);
    connect(osdTimer_, &QTimer::timeout, osdLabel_, &QLabel::hide);
}

void VideoWidget::showOsd(const QString &text, int durationMs)
{
    osdLabel_->setText(text);
    osdLabel_->adjustSize();
    // Top left, clear of the window edge, matching where mpv puts its own.
    osdLabel_->move(16, 16);
    // Above the audio-only note, which covers the whole widget.
    osdLabel_->raise();
    osdLabel_->show();
    // Restarted rather than accumulated, so holding a key down keeps one
    // message on screen instead of leaving the last one to expire early.
    osdTimer_->start(durationMs);
}

VideoWidget::~VideoWidget()
{
    releasePlayerRender();
}

void VideoWidget::setPlayer(MpvPlayer *player)
{
    player_ = player;
    if (player_) {
        connect(player_, &MpvPlayer::frameReady, this, qOverload<>(&QOpenGLWidget::update));
        connect(player_, &MpvPlayer::hasVideoChanged, this, [this](bool hasVideo) { setAudioOnly(!hasVideo); });
        connect(player_, &MpvPlayer::osdMessageRequested, this, &VideoWidget::showOsd);
        connect(player_, &MpvPlayer::endOfFileChanged, this, &VideoWidget::setBlankScreen);
    }
}

void VideoWidget::setClickToPauseEnabled(bool enabled)
{
    clickToPauseEnabled_ = enabled;
}

void VideoWidget::setDoubleClickAction(DoubleClickAction action)
{
    doubleClickAction_ = action;
}

void VideoWidget::setBlankScreen(bool blank)
{
    if (blankScreen_ == blank) {
        return;
    }
    blankScreen_ = blank;
    update();
}

void VideoWidget::setAudioOnly(bool audioOnly)
{
    audioOnly_ = audioOnly;
    audioOnlyIndicator_->setVisible(audioOnly);
    update(); // repaint immediately so a stale video frame doesn't linger behind the note
}

void VideoWidget::releasePlayerRender()
{
    if (player_ && player_->isRenderInitialized()) {
        makeCurrent();
        player_->releaseRender();
        doneCurrent();
    }
}

void VideoWidget::initializeGL()
{
    initializeOpenGLFunctions();
    if (player_) {
        player_->initRender();
    }
}

void VideoWidget::paintGL()
{
    if (audioOnly_ || blankScreen_ || !player_ || !player_->isRenderInitialized()) {
        // Plain GL clear, no mpv render call: for audio-only media mpv has
        // no new frame to draw, so calling render() here would just keep
        // re-blitting whatever video frame was on screen before switching
        // to this file. The same clear covers a finished file, where mpv's
        // keep-open holds the last frame ready to redraw for ever.
        // Note: no QPainter here -- that's what corrupted GL state for mpv's
        // own renderer last time.
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        return;
    }
    const qreal pixelRatio = devicePixelRatioF();
    player_->render(static_cast<int>(defaultFramebufferObject()),
                     static_cast<int>(width() * pixelRatio),
                     static_cast<int>(height() * pixelRatio));
}

void VideoWidget::resizeEvent(QResizeEvent *event)
{
    QOpenGLWidget::resizeEvent(event);

    audioOnlyIndicator_->setGeometry(rect());

    const int pointSize = std::clamp(std::min(width(), height()) / 4, 24, 200);
    QFont noteFont = audioOnlyIndicator_->font();
    noteFont.setPointSize(pointSize);
    audioOnlyIndicator_->setFont(noteFont);
}

void VideoWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && clickToPauseEnabled_) {
        emit playPauseToggleRequested();
    }
    QOpenGLWidget::mousePressEvent(event);
}

void VideoWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (doubleClickAction_ == DoubleClickAction::Fullscreen) {
            emit fullscreenToggleRequested();
        } else {
            emit playPauseToggleRequested();
        }
    }
    QOpenGLWidget::mouseDoubleClickEvent(event);
}

void VideoWidget::mouseMoveEvent(QMouseEvent *event)
{
    // Mouse tracking is on, so this arrives without a button held. Reported as
    // a position within the video surface, which is what decides whether the
    // pointer is in the strip along the bottom that summons the controls.
    emit mouseMoved(int(event->position().y()), height());
    QOpenGLWidget::mouseMoveEvent(event);
}

void VideoWidget::wheelEvent(QWheelEvent *event)
{
    const int steps = event->angleDelta().y() / 120;
    if (steps != 0) {
        emit volumeStepRequested(steps * 5);
    }
    event->accept();
}

void VideoWidget::keyPressEvent(QKeyEvent *event)
{
    // Fixed bindings, unmodified presses only, so the menu bar's own shortcuts
    // (Ctrl+O, the Alt+... subtitle keys, the keypad panning ones) still reach
    // the base class.
    //
    // The numpad Enter key is the one exception: it always carries
    // KeypadModifier, so that flag is discounted for it alone. Not for keys in
    // general -- with NumLock off the numpad arrows report Key_Left and friends
    // with the same flag, and those must keep falling through to the Move
    // Picture shortcuts rather than seeking.
    const Qt::KeyboardModifiers modifiers = event->key() == Qt::Key_Enter
        ? event->modifiers() & ~Qt::KeypadModifier
        : event->modifiers();
    if (modifiers != Qt::NoModifier) {
        QOpenGLWidget::keyPressEvent(event);
        return;
    }

    switch (event->key()) {
    case Qt::Key_Space:
        emit playPauseToggleRequested();
        break;
    // Key_Return is the main Enter key; Key_Enter is the numpad one. Qt keeps
    // them apart, but nobody looking at their keyboard does.
    case Qt::Key_F:
    case Qt::Key_Return:
    case Qt::Key_Enter:
        emit fullscreenToggleRequested();
        break;
    case Qt::Key_Escape:
        emit exitFullscreenRequested();
        break;
    case Qt::Key_Up:
        emit volumeStepRequested(5);
        break;
    case Qt::Key_Down:
        emit volumeStepRequested(-5);
        break;
    case Qt::Key_Right:
        emit seekStepRequested(1);
        break;
    case Qt::Key_Left:
        emit seekStepRequested(-1);
        break;
    case Qt::Key_Home:
        emit seekToStartRequested();
        break;
    case Qt::Key_End:
        emit seekToEndRequested();
        break;
    case Qt::Key_M:
        emit muteToggleRequested();
        break;
    // Z / X / C sit together on the keyboard: reset, slower, faster.
    case Qt::Key_S:
        emit screenshotRequested();
        break;
    case Qt::Key_Z:
        emit speedResetRequested();
        break;
    case Qt::Key_X:
        emit speedStepRequested(-0.1);
        break;
    case Qt::Key_C:
        emit speedStepRequested(0.1);
        break;
    default:
        QOpenGLWidget::keyPressEvent(event);
        return;
    }
    event->accept();
}

void VideoWidget::contextMenuEvent(QContextMenuEvent *event)
{
    emit contextMenuRequested(event->globalPos());
    event->accept();
}
