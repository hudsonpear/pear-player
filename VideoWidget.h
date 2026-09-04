#pragma once

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QString>
#include <QStringList>

class MpvPlayer;
class QLabel;
class QTimer;

/// Native widget mpv renders video into via the libmpv OpenGL render API.
///
/// VideoWidget owns no playback state: it forwards user intent (clicks,
/// key presses, wheel scrolls) as signals and lets MainWindow decide what
/// they mean. This keeps playback logic out of the UI layer.
class VideoWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    enum class DoubleClickAction { Fullscreen, PlayPause };

    explicit VideoWidget(QWidget *parent = nullptr);
    ~VideoWidget() override;

    /// Associates the widget with the player that will render into it.
    /// Does not take ownership.
    void setPlayer(MpvPlayer *player);

    /// Whether a single click toggles play/pause. Disabling it leaves
    /// double-click (per setDoubleClickAction()) as the only click gesture.
    void setClickToPauseEnabled(bool enabled);

    /// What a double-click on the video surface does.
    void setDoubleClickAction(DoubleClickAction action);

    /// Shows a message over the video for durationMs, replacing any message
    /// already up. Drawn by Qt rather than by mpv's own OSD, so it appears for
    /// audio-only files too -- those never reach mpv's renderer.
    void showOsd(const QString &text, int durationMs);

    /// Paints black instead of the video. Used at the end of a file, where
    /// mpv's keep-open would otherwise leave the last frame on screen.
    void setBlankScreen(bool blank);

    /// Shows an audio file's tags in the right half of the surface, beside the
    /// cover art MainWindow has penned into the left half (see
    /// MpvPlayer::setVideoRightMargin). An empty title and no details hides the
    /// panel again and gives the whole surface back.
    void setAudioInfo(const QString &title, const QStringList &details);

    /// Frees the player's mpv render context while this widget's GL context
    /// is still current. Must be called by the owner (MainWindow) before
    /// the MpvPlayer itself is destroyed, since mpv needs a live, current
    /// GL context to release the GL objects it allocated. Safe to call more
    /// than once; also invoked defensively from the destructor.
    void releasePlayerRender();

signals:
    void playPauseToggleRequested();
    void fullscreenToggleRequested();
    void exitFullscreenRequested();
    void muteToggleRequested();
    void seekStepRequested(int direction);
    void seekToStartRequested();
    void seekToEndRequested();
    void speedStepRequested(double delta);
    void speedResetRequested();
    void screenshotRequested();

    /// Pointer moved over the video: its y within the surface, and the
    /// surface's height.
    void mouseMoved(int y, int height);
    void volumeStepRequested(int delta);
    void contextMenuRequested(const QPoint &globalPos);

private slots:
    /// Shows/hides the big music-note placeholder for audio-only media,
    /// driven by MpvPlayer::hasVideoChanged.
    void setAudioOnly(bool audioOnly);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeEvent(QResizeEvent *event) override;

    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    /// Places the music note and the track-details panel, and scales their
    /// text to the surface. Called on every resize and whenever the details
    /// change, since both depend on how much room there is.
    void layoutOverlays();

    MpvPlayer *player_ = nullptr;
    QLabel *audioOnlyIndicator_ = nullptr;
    QLabel *audioInfoLabel_ = nullptr;
    QString audioInfoTitle_;
    QStringList audioInfoDetails_;
    QLabel *osdLabel_ = nullptr;
    QTimer *osdTimer_ = nullptr;
    bool audioOnly_ = false;
    bool blankScreen_ = false;
    bool clickToPauseEnabled_ = true;
    DoubleClickAction doubleClickAction_ = DoubleClickAction::Fullscreen;
};
