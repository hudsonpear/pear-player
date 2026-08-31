#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QMap>
#include <QSize>

#include "SubtitleStyle.h"

#include <mpv/client.h>
#include <mpv/render_gl.h>

/// Thin RAII wrapper around libmpv (client API + OpenGL render API).
///
/// MpvPlayer owns the mpv core and the mpv render context. It never touches
/// any Qt widget directly: playback state is surfaced exclusively through
/// Qt signals, and rendering is exposed through initRender()/render() so a
/// QOpenGLWidget (VideoWidget) can drive it from its own GL context.
class MpvPlayer : public QObject
{
    Q_OBJECT

public:
    explicit MpvPlayer(QObject *parent = nullptr);
    ~MpvPlayer() override;

    MpvPlayer(const MpvPlayer &) = delete;
    MpvPlayer &operator=(const MpvPlayer &) = delete;

    /// One subtitle or audio track as reported by mpv's track-list,
    /// embedded (from the current file) or external (added via
    /// loadExternalSubtitle()).
    struct TrackInfo {
        int id = 0;
        QString title;
        QString language;
        bool selected = false;
        bool external = false;
    };

    /// Creates the mpv core, applies baseline options and starts observing
    /// the properties this class cares about. Must be called once before
    /// any other method. hwDecEnabled selects hardware- vs software-decoded
    /// playback; hwdec is an mpv init-time option, so this only takes effect
    /// here, not via a later setter.
    void initialize(bool hwDecEnabled = false);

    /// Creates the mpv render context bound to the *currently current*
    /// OpenGL context. Must be called from QOpenGLWidget::initializeGL().
    void initRender();

    /// Renders one frame into the given framebuffer. Must be called from
    /// QOpenGLWidget::paintGL() with that widget's GL context current.
    void render(int fbo, int width, int height);

    /// Frees the mpv render context. Must be called with the owning
    /// QOpenGLWidget's GL context current (mpv releases GL objects it
    /// allocated) and BEFORE that GL context itself is destroyed. Safe to
    /// call more than once.
    void releaseRender();

    /// True once initRender() has succeeded.
    [[nodiscard]] bool isRenderInitialized() const noexcept { return renderCtx_ != nullptr; }

    // --- playback commands -------------------------------------------------
    void loadFile(const QString &path);

    /// Closes the current file and empties mpv's playlist, releasing the
    /// handle on disk. stop() only pauses and rewinds, which leaves the file
    /// open and so cannot be used before moving or deleting it.
    void unload();
    void play();
    void pause();
    void togglePause();
    void stop();

    void seekAbsolute(double seconds);
    void seekRelative(double secondsDelta);
    void frameStep();
    void frameBackStep();

    void playlistNext();
    void playlistPrevious();

    void setSpeed(double speed);
    void setLoop(bool loop);
    void setVolume(int volume0to200);
    void setMute(bool muted);

    /// Changes the top of the volume scale (100-1000%), re-clamping the
    /// current volume down if it now exceeds the new maximum.
    void setMaxVolume(int maxVolume);

    // --- subtitles ----------------------------------------------------
    /// Snapshot of every subtitle track mpv currently knows about, embedded
    /// and external alike. Queried live (not cached) since it only changes
    /// on file load or loadExternalSubtitle(), both infrequent.
    [[nodiscard]] QVector<TrackInfo> subtitleTracks() const;

    /// Selects a subtitle track by id, or disables subtitles for 0.
    void setSubtitleTrack(int trackId);

    /// Adds and selects an external subtitle file for the current media.
    void loadExternalSubtitle(const QString &path);

    // --- subtitle appearance and timing ---------------------------------
    /// Pushes a whole SubtitleStyle to mpv. When style.useDefaults is set,
    /// mpv's own defaults are restored instead of the struct's values.
    void applySubtitleStyle(const SubtitleStyle &style);

    /// Whether subtitles are drawn at all; independent of which track is
    /// selected, so toggling visibility does not lose the chosen track.
    void setSubtitleVisible(bool visible);
    [[nodiscard]] bool isSubtitleVisible() const;

    /// Positive delay shows subtitles later, negative shows them earlier.
    void setSubtitleDelay(double seconds);
    [[nodiscard]] double subtitleDelay() const;

    void setSubtitleBold(bool bold);
    [[nodiscard]] bool isSubtitleBold() const;

    void setSubtitleFontSize(double points);
    [[nodiscard]] double subtitleFontSize() const;

    /// mpv sub-pos: 0 is the top of the frame, 100 the default bottom line.
    void setSubtitleVerticalPosition(double position);
    [[nodiscard]] double subtitleVerticalPosition() const;

    /// mpv sub-align-x: "left", "center" or "right".
    void setSubtitleAlignX(const QString &align);

    /// mpv sub-justify: how multiple lines line up with each other, which is
    /// separate from where the block as a whole sits.
    void setSubtitleJustify(const QString &justify);

    // --- video ------------------------------------------------------------
    /// Picture adjustments; each takes -100..100 with 0 meaning untouched.
    void setVideoAdjust(int brightness, int contrast, int saturation, int hue);

    /// mpv video-rotate: 0, 90, 180 or 270 degrees clockwise.
    void setVideoRotate(int degrees);
    [[nodiscard]] int videoRotate() const;

    /// Flip (vertical) and mirror (horizontal) share mpv's video filter chain,
    /// so both are set together rather than fighting over the "vf" property.
    void setFlipAndMirror(bool flip, bool mirror);

    /// Geometry preset: a vertical stretch factor and a log2 zoom, matching
    /// mpv's video-scale-y and video-zoom.
    void setVideoGeometry(double scaleY, double zoom);

    /// Shifts the picture inside the window. Both are fractions of the video
    /// size: positive x moves right, positive y moves down.
    void setVideoPan(double panX, double panY);

    /// How the picture fills the window. keepAspect false stretches it to the
    /// window regardless of shape; panscan runs 0 (fit entirely inside) to 1
    /// (fill completely, cropping the overflow).
    void setVideoFrame(bool keepAspect, double panscan);

    /// mpv video-aspect-override: -1 keeps the ratio the file declares, 0
    /// ignores it and assumes square pixels, anything else forces that ratio.
    void setAspectOverride(double ratio);

    /// Decoded video size in pixels, 0 when unknown (audio-only, or nothing
    /// loaded). Used to size the window to a multiple of the video.
    [[nodiscard]] int videoWidth() const;
    [[nodiscard]] int videoHeight() const;

    /// Size the picture is displayed at, which unlike videoWidth()/Height()
    /// accounts for the aspect ratio and rotation. Empty when nothing with a
    /// video track is loaded.
    [[nodiscard]] QSize videoDisplaySize() const;

    /// mpv gamma, -100..100 with 0 meaning untouched. Separate from the other
    /// four picture controls because it was added later and is stored on its
    /// own key.
    void setGamma(int gamma);

    // --- audio ------------------------------------------------------------
    /// Positive delay plays the audio later than the picture.
    void setAudioDelay(double seconds);
    [[nodiscard]] double audioDelay() const;

    /// One entry per output mpv can use; the first is always "auto".
    struct AudioDevice {
        QString name;        ///< mpv's device id, passed back to setAudioDevice
        QString description; ///< human-readable name for the menu
    };
    [[nodiscard]] QVector<AudioDevice> audioDevices() const;
    void setAudioDevice(const QString &name);
    [[nodiscard]] QString audioDevice() const;

    /// mpv audio-channels: "auto-safe", "stereo" or "mono".
    void setAudioChannels(const QString &layout);
    [[nodiscard]] QString audioChannels() const;

    // --- looping ----------------------------------------------------------
    /// Repeats the current file for ever.
    void setLoopFile(bool loop);
    [[nodiscard]] bool isLoopingFile() const;

    /// Repeats the whole playlist for ever.
    void setLoopPlaylist(bool loop);
    [[nodiscard]] bool isLoopingPlaylist() const;

    // --- chapters ---------------------------------------------------------
    struct Chapter {
        double time = 0.0; ///< start, in seconds
        QString title;
    };
    [[nodiscard]] QVector<Chapter> chapters() const;
    [[nodiscard]] int currentChapter() const;
    void setChapter(int index);

    // --- info -------------------------------------------------------------
    /// Container/stream metadata (title, artist, album...) as reported by mpv.
    [[nodiscard]] QMap<QString, QString> metadata() const;

    /// A one-line summary of a property, empty when it is unset. Used by the
    /// Media Info dialog to read things like video-format or audio-codec.
    [[nodiscard]] QString propertyText(const QString &name) const;

    // --- equalizer -------------------------------------------------------
    /// Applies a ten-band graphic equaliser as one biquad filter per band.
    /// An all-zero set removes the filter chain entirely rather than running
    /// ten no-op filters over every sample.
    void setEqualizerGains(const QVector<int> &gainsDb);

    /// Briefly overlays text on the video, the way mpv itself reports state
    /// changes. Used for feedback that would otherwise need a status bar this
    /// window does not have.
    void showOsdMessage(const QString &text, int durationMs = 2000);

    /// Writes the current frame, with subtitles as displayed, to filePath.
    /// False when mpv could not write it -- nothing loaded, or the folder is
    /// not writable.
    bool takeScreenshot(const QString &filePath);

    // --- audio tracks ---------------------------------------------------
    /// Snapshot of every audio track mpv currently knows about (e.g. dual
    /// audio releases). Queried live, same rationale as subtitleTracks().
    [[nodiscard]] QVector<TrackInfo> audioTracks() const;

    /// Selects an audio track by id, or disables audio for 0.
    void setAudioTrack(int trackId);

    // --- cached state --------------------------------------------------
    [[nodiscard]] double position() const noexcept { return position_; }
    [[nodiscard]] double duration() const noexcept { return duration_; }
    [[nodiscard]] bool isPaused() const noexcept { return paused_; }
    [[nodiscard]] int volume() const noexcept { return volume_; }
    [[nodiscard]] bool isMuted() const noexcept { return muted_; }
    [[nodiscard]] double speed() const noexcept { return speed_; }

signals:
    void positionChanged(double seconds);
    void durationChanged(double seconds);
    void pauseChanged(bool paused);
    void volumeChanged(int volume0to200);
    void muteChanged(bool muted);
    void speedChanged(double speed);
    void fileLoaded(QString filename);
    void mediaTitleChanged(QString title);
    void playbackFinished();
    void frameReady();
    void errorOccurred(QString message);

    /// False for audio-only media (no active video track) so the UI can
    /// show a placeholder instead of a blank frame.
    void hasVideoChanged(bool hasVideo);

    /// The displayed picture size changed: a new file, or a rotation or
    /// aspect change on the current one.
    void videoDisplaySizeChanged(const QSize &size);

    /// A message to put on screen for durationMs. Emitted by
    /// showOsdMessage(); the video widget draws it.
    void osdMessageRequested(const QString &text, int durationMs);

    /// True when the file has played to its end, false again on the next load
    /// or a seek back into it. keep-open leaves the last frame on screen, so
    /// this is what tells the widget to stop showing it.
    void endOfFileChanged(bool atEnd);

    /// The current chapter index, whether reached by playing into it or by
    /// picking it. -1 when the file has no chapters.
    void chapterChanged(int index);

private slots:
    // Runs on the Qt thread (queued from the mpv wakeup callback) and
    // drains the mpv event queue.
    void processMpvEvents();

private:
    static void onMpvWakeup(void *ctx);
    static void onMpvRenderUpdate(void *ctx);
    static void *getProcAddress(void *ctx, const char *name);

    void handleEvent(mpv_event *event);
    void handlePropertyChange(mpv_event_property *prop);
    void observeProperties();

    mpv_handle *mpv_ = nullptr;
    mpv_render_context *renderCtx_ = nullptr;

    // First error-level line mpv logged for the current load. mpv_event_end_file
    // only carries a generic "loading failed" code, so this supplies the actual
    // reason (HTTP status, missing codec, unreachable host...).
    //
    // First rather than last on purpose: when a load fails mpv keeps going
    // through its fallbacks, and the later lines describe those failing too
    // ("youtube-dl failed: unexpected error occurred") rather than the root
    // cause. Cleared on every loadFile() so a stale line is never reported.
    QString firstLogError_;

    // Cached playback state (kept in sync via property observation).
    double position_ = 0.0;
    double duration_ = 0.0;
    bool paused_ = true;
    /// Last seen value of mpv's eof-reached, so playbackFinished() is emitted
    /// once per end rather than on every report of the flag.
    bool eofReached_ = false;
    int volume_ = 100;
    int maxVolume_ = 200;
    bool muted_ = false;
    double speed_ = 1.0;
};
