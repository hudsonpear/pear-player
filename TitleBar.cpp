#include "TitleBar.h"

#include "IconButton.h"

#include <QCoreApplication>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QWindow>

#include <functional>

namespace {

constexpr int kBarHeight = 34;
constexpr int kLogoSize = 20;
constexpr QSize kButtonSize(40, 26);
constexpr QSize kGlyphSize(16, 16);
constexpr int kIconCanvas = 24;

// Windows' own close-button red, so the one destructive button in the row is
// the one that reads as destructive.
const QColor kCloseHover(232, 17, 35);

// Same convention as the control bar: the English tooltip is kept in a
// property, in the MainWindow context, so MainWindow::retranslateControls()
// finds and re-translates these buttons along with all the others.
void setTranslatableToolTip(QWidget *widget, const char *source)
{
    widget->setProperty("sourceToolTip", QString::fromUtf8(source));
    widget->setToolTip(QCoreApplication::translate("MainWindow", source));
}

// Same hand-drawn approach as the transport icons in MainWindow.cpp: a white
// shape on a transparent canvas, always crisp and always the right colour
// against the dark palette.
QIcon captionIcon(const std::function<void(QPainter &)> &draw)
{
    QPixmap pixmap(kIconCanvas, kIconCanvas);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(Qt::white, 1.4));
    painter.setBrush(Qt::NoBrush);
    draw(painter);
    return QIcon(pixmap);
}

QIcon minimizeIcon()
{
    return captionIcon([](QPainter &p) {
        p.drawLine(QPointF(6, 12), QPointF(18, 12));
    });
}

QIcon maximizeIcon()
{
    return captionIcon([](QPainter &p) {
        p.drawRoundedRect(QRectF(6.5, 6.5, 11, 11), 1.5, 1.5);
    });
}

QIcon restoreIcon()
{
    return captionIcon([](QPainter &p) {
        // The window in front, and the one it was restored out of behind it.
        p.drawRoundedRect(QRectF(5.5, 8.5, 10, 9), 1.5, 1.5);
        p.drawPolyline(QPolygonF({QPointF(8.5, 6.5), QPointF(18.5, 6.5), QPointF(18.5, 15)}));
    });
}

QIcon closeIcon()
{
    return captionIcon([](QPainter &p) {
        p.drawLine(QPointF(7, 7), QPointF(17, 17));
        p.drawLine(QPointF(17, 7), QPointF(7, 17));
    });
}

IconButton *makeCaptionButton(const QIcon &icon, const char *tooltip)
{
    auto *button = new IconButton(icon, QString());
    button->setIconSize(kGlyphSize);
    button->setFixedSize(kButtonSize);
    button->setFocusPolicy(Qt::NoFocus);
    setTranslatableToolTip(button, tooltip);
    return button;
}

} // namespace

TitleBar::TitleBar(QMainWindow *window, QMenuBar *menuBar)
    : QWidget(window)
    , window_(window)
{
    setFixedHeight(kBarHeight);
    // Painted rather than left transparent: this row sits where the native
    // caption used to, with nothing of its own behind it.
    setAutoFillBackground(true);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 0, 0, 0);
    layout->setSpacing(0);

    // Kept for its actions and their application-wide shortcuts, never shown:
    // the entries reach the user through the button below instead. Reparented
    // here so it is not left as a stray unmanaged child of the window.
    menuBar_ = menuBar;
    menuBar_->setParent(this);
    menuBar_->hide();

    menuButton_ = new IconButton(
        QIcon(QStringLiteral(":/app/resources/icons/pearicon-%1.png").arg(kLogoSize)),
        QStringLiteral("Pear Player"), this);
    menuButton_->setIconSize(QSize(kLogoSize, kLogoSize));
    menuButton_->setFixedHeight(kButtonSize.height());
    menuButton_->setFocusPolicy(Qt::NoFocus);
    // Only so it can be shown held down while its popup is open.
    menuButton_->setCheckable(true);
    connect(menuButton_, &IconButton::clicked, this, &TitleBar::showMainMenu);
    layout->addWidget(menuButton_);

    titleLabel_ = new QLabel(this);
    titleLabel_->setAlignment(Qt::AlignCenter);
    // Dimmer than menu text: it names the window, it is not something to
    // click, and the native caption it replaces was dimmer too.
    QPalette titlePalette = titleLabel_->palette();
    titlePalette.setColor(QPalette::WindowText, QColor(190, 190, 190));
    titleLabel_->setPalette(titlePalette);
    // Ignored horizontally so a long file name is elided to fit whatever room
    // there is, rather than widening the window's own minimum size.
    titleLabel_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    // Re-elided from the label's own resize rather than this widget's: the
    // layout has not handed it its new width yet while the bar is resizing.
    titleLabel_->installEventFilter(this);

    // The label itself takes all the room between the button and the window
    // controls, and centres the text in it. Stretch on the label, not spacers
    // either side of it -- an Ignored-policy widget reports no width of its
    // own, so spacers with a stretch factor would take every pixel and leave
    // it nothing to draw in.
    layout->addWidget(titleLabel_, /*stretch=*/1);

    auto *minimizeButton = makeCaptionButton(minimizeIcon(), QT_TR_NOOP("Minimize"));
    connect(minimizeButton, &IconButton::clicked, window_, &QWidget::showMinimized);
    layout->addWidget(minimizeButton);

    maximizeButton_ = makeCaptionButton(maximizeIcon(), QT_TR_NOOP("Maximize"));
    connect(maximizeButton_, &IconButton::clicked, this, [this] {
        if (window_->isMaximized()) {
            window_->showNormal();
        } else {
            window_->showMaximized();
        }
    });
    layout->addWidget(maximizeButton_);

    auto *closeButton = makeCaptionButton(closeIcon(), QT_TR_NOOP("Close"));
    closeButton->setHoverColor(kCloseHover);
    connect(closeButton, &IconButton::clicked, window_, &QWidget::close);
    layout->addWidget(closeButton);

}

void TitleBar::showMainMenu()
{
    QMenu menu(this);
    // The real File, Playback, Subtitles, Audio, Video, Settings and Help
    // entries, submenus and ticks and all -- not copies of them, so everything
    // the menu bar would have shown behaves identically here.
    menu.addActions(menuBar_->actions());

    // Held down for as long as the popup is up, the way a menu title stays
    // highlighted while its menu is open.
    menuButton_->setChecked(true);
    menu.exec(mapToGlobal(QPoint(menuButton_->x(), height())));
    menuButton_->setChecked(false);
}

void TitleBar::syncWindowState()
{
    const bool maximized = window_->isMaximized();
    maximizeButton_->setIcon(maximized ? restoreIcon() : maximizeIcon());
    setTranslatableToolTip(maximizeButton_, maximized ? QT_TR_NOOP("Restore") : QT_TR_NOOP("Maximize"));
}

void TitleBar::mousePressEvent(QMouseEvent *event)
{
    QWindow *handle = window_->windowHandle();
    if (event->button() != Qt::LeftButton || !handle) {
        QWidget::mousePressEvent(event);
        return;
    }

    const QPoint pos = event->position().toPoint();
    // The window's top edge is this widget's top edge, so the two top corners
    // and the strip between them are grabbed from here; the central widget
    // owns the other three sides.
    if (!window_->isMaximized() && !window_->isFullScreen() && pos.y() <= kResizeBorder) {
        Qt::Edges edges = Qt::TopEdge;
        if (pos.x() <= kResizeBorder) {
            edges |= Qt::LeftEdge;
        } else if (pos.x() >= width() - kResizeBorder) {
            edges |= Qt::RightEdge;
        }
        handle->startSystemResize(edges);
        return;
    }

    // Windows restores a maximised window and carries on with the drag by
    // itself, so that needs no special case here.
    handle->startSystemMove();
}

void TitleBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mouseDoubleClickEvent(event);
        return;
    }
    if (window_->isMaximized()) {
        window_->showNormal();
    } else {
        window_->showMaximized();
    }
}

bool TitleBar::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == titleLabel_ && event->type() == QEvent::Resize) {
        updateElidedTitle();
    }
    return QWidget::eventFilter(watched, event);
}

void TitleBar::setMediaTitle(const QString &title)
{
    title_ = title;
    updateElidedTitle();
}

void TitleBar::updateElidedTitle()
{
    titleLabel_->setText(titleLabel_->fontMetrics().elidedText(
        title_, Qt::ElideMiddle, titleLabel_->width()));
}
