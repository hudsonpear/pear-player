#pragma once

#include "VideoSettings.h"

#include <QMainWindow>
#include <QElapsedTimer>
#include <QPointer>
#include <functional>
#include <memory>

struct SubtitleStyle;
struct FileMemory;

class MpvPlayer;
class VideoWidget;
class TimelineSlider;
class VolumeSlider;
class IconButton;
class QLabel;
class QAction;
class QMenu;
class QVBoxLayout;
class QTimer;
class QShortcut;
class QDockWidget;
class PlaylistWidget;
class QListWidgetItem;
class QFileInfo;
class QUrl;
class QPushButton;
class QMenuBar;
class SettingsDialog;
class TaskbarProgress;
class TitleBar;

/// Top-level window: owns the player and every widget, wires user actions
/// to MpvPlayer commands, and reflects MpvPlayer's signals back into the UI.
/// All playback logic lives in MpvPlayer; this class only translates intent.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    /// Loads and starts playing the given file. Used for both the Open
    /// dialog and an optional startup file passed on the command line.
    void loadFile(const QString &path);

    /// Handles a file handed over by a second launch: brings this window to
    /// the front and plays it, so opening a file while the player is already
    /// running replaces what is playing instead of opening another window.
    /// Message is one path, or several separated by newlines.
    void openFromAnotherInstance(const QString &message);

    /// Plays the first path and queues the rest, showing the playlist when
    /// there is more than one. Used by the Open dialog, drag and drop, the
    /// command line and a second launch.
    void openFiles(const QStringList &paths);

    /// Rebuilds the menu bar and refreshes the rest of the persistent UI in
    /// the current language, for use after Translation::applyLanguage().
    /// Dialogs need no help: each is constructed when it opens, so it comes up
    /// translated on its own.
    void retranslateUi();

protected:
    void closeEvent(QCloseEvent *event) override;

    /// Keeps the title bar's maximise glyph and the frameless resize border in
    /// step with the window state, however it changed -- the caption buttons,
    /// a double click, Win+Arrow or the taskbar.
    void changeEvent(QEvent *event) override;

    /// Keeps the fullscreen control bar on the bar itself while the pointer is
    /// over it, rather than letting the hide timer pull it away mid-click.
    bool eventFilter(QObject *watched, QEvent *event) override;

    /// Accepting drops on the window covers the video area too: QOpenGLWidget
    /// does not accept drops itself, so the event propagates up to here.
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void openFile();
    void openFolder();
    void openUrl();
    void copyFileTo();

    /// Shows the file playing now in Explorer, selected in its own folder.
    void openContainingFolder();
    void clearRecentFiles();
    void refreshRecentFilesMenu();
    void downloadFromUrl();
    void downloadCurrentMedia();
    void openSubtitleFile();
    void onPlaylistFilesDropped(const QStringList &paths);
    void showVideoContextMenu(const QPoint &globalPos);
    void openSettingsDialog();
    void openSubtitleSettings();
    void openEqualizerDialog();
    void openAboutDialog();
    void openMediaInfoDialog();

    // Subtitles menu
    void toggleSubtitleVisibility();
    void subtitlePositionReset();
    void subtitleMoveUp();
    void subtitleMoveDown();
    // Playlist buttons
    void playlistAddFiles();
    void playlistAddFolder();
    void playlistRemoveSelected();
    void playlistClear();
    void playlistShuffle();
    void playlistPlayNext();
    void playlistPlayPrevious();
    void playlistToggleRepeat();
    void playlistSave();
    void playlistLoad();

    // Playback
    void frameStepForward();
    void frameStepBackward();
    void refreshChaptersMenu();

    // Audio menu
    void audioDelayEarlier();
    void audioDelayLater();
    void audioDelayReset();
    void refreshAudioDeviceMenu();

    // Video menu
    void toggleVideoFlip();
    void toggleVideoMirror();
    void cycleVideoFrameMode();
    void panVideoUp();
    void panVideoDown();
    void panVideoLeft();
    void panVideoRight();
    void panVideoCenter();

    void toggleSubtitleBold();
    void subtitleFontSizeUp();
    void subtitleFontSizeDown();
    void subtitleSyncEarlier();
    void subtitleSyncLater();
    void subtitleSyncReset();
    void subtitleSyncSave();
    void toggleFullscreen();
    void exitFullscreenIfActive();

    // MpvPlayer -> UI
    void onPositionChanged(double seconds);
    void onDurationChanged(double seconds);
    void onPauseChanged(bool paused);
    void onVolumeChanged(int volume0to200);
    void onMuteChanged(bool muted);
    void onSpeedChanged(double speed);
    void onMediaTitleChanged(const QString &title);
    void onFileLoaded(const QString &filename);
    void onErrorOccurred(const QString &message);
    void onVideoDisplaySizeChanged(const QSize &size);

    /// Resizes the window so the video surface is exactly videoSize, scaling
    /// down if that would not fit on the screen. No-op in fullscreen or while
    /// maximized.
    void fitWindowToVideoSize(const QSize &videoSize);

    /// Shrinks and nudges the window until it fits on its screen. Runs after
    /// a fit, once the window manager has answered the resize.
    void clampWindowToScreen();

    // UI -> MpvPlayer
    void onVolumeSliderMoved(int value);
    void onVolumeStepRequested(int delta);

    /// X and C: nudges playback speed, clamped and rounded to one decimal.
    void onSpeedStepRequested(double delta);

    /// S: writes the current frame to the screenshot folder.
    void takeScreenshot();

    /// Reports a seek on the OSD. deltaSeconds is the jump just requested, or
    /// 0 for an absolute move (Home/End), which shows the position alone.
    void showSeekOsd(double deltaSeconds);

    /// Names a frame step and the timestamp it landed on.
    void showFrameStepOsd(const QString &label);

    /// "14 files added", for every route that appends to the playlist.
    void showFilesAddedOsd(int count);

    /// Tooltips, the playlist dock title and the window title.
    void retranslateControls();

    /// Puts a menu action on the window for an application-wide shortcut, and
    /// records it so retranslateUi() can take it off again.
    void registerMenuShortcut(QAction *action);

    /// Sits the floating control bar along the bottom of the video. Only
    /// meaningful in fullscreen, where the bar is a child of the video widget.
    void layoutFullscreenControls();

    /// Shows or hides that bar as the pointer enters or leaves the strip along
    /// the bottom of the video.
    void onVideoMouseMoved(int y, int height);

private:
    /// Single place the name of what is playing is set: it goes to the middle
    /// of the title bar on its own, and to the window title with the
    /// application name after it, which is what the task bar and the Alt-Tab
    /// switcher show. Empty for "nothing is playing".
    void setMediaTitle(const QString &title);

    void createMenus();

    /// Opens up the grab band around the central widget, or closes it again
    /// when the window is maximised or fullscreen and there is nothing left
    /// to drag.
    void updateResizeBorder();
    void createControlsBar();
    void createPlaylistPanel();
    void createConnections();
    void loadSettings();
    void saveSettings();
    void applyPreferences();
    void refreshTimeLabel();
    double computeJumpSeconds() const;
    void addPlaylistItem(const QFileInfo &info);

    /// Appends paths to the playlist without disturbing what is playing.
    void queueFiles(const QStringList &paths);
    void updateTaskbarProgress();
    void stopPlayback();
    void populateAudioMenu(QMenu *menu);

    /// Builds the persistent Subtitles menu once. It has to outlive any single
    /// popup because its entries carry application-wide shortcuts, which only
    /// keep working while the actions themselves stay alive.
    void createSubtitlesMenu();

    /// Refills only the track entries of the "Show Subtitle" submenu; the Off
    /// and Add File entries are permanent members.
    void refreshShowSubtitleMenu();

    /// Creates a Subtitles-menu action whose shortcut also fires in fullscreen,
    /// where the menu bar is hidden.
    QAction *addSubtitleAction(QMenu *menu, const QString &text, const QKeySequence &shortcut,
                                void (MainWindow::*slot)());

    /// Applies one edit to the stored subtitle style, so the Settings dialog
    /// opens showing what is actually on screen instead of the values from
    /// the last time it was used.
    ///
    /// Takes a mutation rather than saving every menu-reachable value: mpv
    /// reports its own defaults while no file is loaded, so writing them all
    /// back would quietly overwrite settings the menu never touched.
    void updateStoredSubtitleStyle(const std::function<void(SubtitleStyle &)> &mutate);

    void createVideoMenu();
    void createAudioMenu();

    /// Loop-off / loop-file / loop-playlist are mutually exclusive, so they are
    /// set through one enum rather than three independent toggles.
    enum class LoopMode { Off, File, Playlist };
    void setLoopMode(LoopMode mode);

    /// Pushes flip/mirror/rotation/preset state onto the player. Called after
    /// any of them changes and after a file loads, since mpv clears the video
    /// filter chain per file.
    void applyVideoState();

    /// Sets the rotation, updates the menu ticks and remembers it for the
    /// file playing now.
    void setVideoRotation(int degrees);

    /// Applies one Video Frame entry: how the picture fills the window, and
    /// for the size entries the window dimensions too.
    void setVideoFrameMode(int index);

    /// Forces a display aspect ratio, or restores the file's own.
    void setVideoAspectRatio(double ratio);

    /// Scales the picture: 100 leaves it alone, 50 halves it, 200 doubles it.
    /// Independent of the Video Frame modes, and combined with them.
    void setVideoZoomPercent(int percent);


    /// Applies one edit to what is remembered about the current file. Does
    /// nothing when nothing is loaded, so a stray edit never creates an entry
    /// keyed on an empty path.
    void updateFileMemory(const std::function<void(FileMemory &)> &mutate);

    /// Pushes the current file's chapter starts onto the timeline, or clears
    /// them when the setting is off or the file has none.
    void refreshChapterMarkers();

    /// Stores where the current file was left, if resuming is enabled and the
    /// position is far enough from both ends to be worth keeping.
    void savePlaybackPosition();

    /// Offers to send the file playing now to the Recycle Bin, and stops
    /// playback first so the file is not still open when it moves.
    void deleteCurrentFile();

    /// Records a newly opened file at the top of the recent list, moving it up
    /// rather than duplicating it if it is already there.
    void addToRecentFiles(const QString &path);

    /// Moves through the playlist by delta entries, wrapping at both ends.
    /// Works on the list widget rather than mpv's own playlist, which only
    /// ever holds the one file the player was told to load.
    void playlistPlayRelative(int delta);

    /// Restores subtitle file, sync and the whole video block for the file
    /// just loaded, falling back to the app-wide defaults when this file has
    /// no remembered video state.
    void restoreFileMemory();

    /// Writes the current flip/mirror/rotation/picture/preset onto the file
    /// playing now. Called after any of them changes, so the same video comes
    /// back looking the way it was left.
    void saveVideoStateToFile();

    /// Runs the Settings dialog with live preview: picture and subtitle edits
    /// take effect as they are made, are kept on OK and undone on Cancel.
    /// Shared by all three entry points so they behave identically.
    void runSettingsDialog(SettingsDialog &dialog);

    /// Shared by the Open and Download entries. Returns an invalid QUrl when
    /// the user cancels or types something unusable (which it reports itself).
    QUrl promptForUrl(const QString &title, const QString &label);

    /// Asks where to save, then downloads url there with a progress dialog.
    void startDownload(const QUrl &url);

    /// True when what is loaded came from the network, so downloading it makes
    /// sense; false for local files and when nothing is loaded.
    [[nodiscard]] bool currentMediaIsNetworkUrl() const;

    static QString formatTime(double seconds);

    std::unique_ptr<MpvPlayer> player_;
    std::unique_ptr<TaskbarProgress> taskbarProgress_;

    VideoWidget *videoWidget_ = nullptr;
    TimelineSlider *timelineSlider_ = nullptr;
    VolumeSlider *volumeSlider_ = nullptr;

    /// The window is frameless, so its caption is a widget of ours sitting in
    /// the menu-bar slot, with the menu bar itself inside it.
    TitleBar *titleBar_ = nullptr;
    /// Held rather than reached for through QMainWindow::menuBar(), which
    /// builds and installs a *new* bar whenever the menu-bar slot holds
    /// something that is not a QMenuBar -- here, the title bar -- and so would
    /// throw the caption out of the window on the next call.
    QMenuBar *menuBar_ = nullptr;
    /// Owns the resize band along the left, right and bottom edges.
    QWidget *central_ = nullptr;

    QWidget *controlsBar_ = nullptr;
    /// One button for both: shows Play while paused and Pause while playing,
    /// so its icon is the action the next click performs.
    IconButton *playPauseButton_ = nullptr;
    IconButton *stopButton_ = nullptr;
    IconButton *prevButton_ = nullptr;
    IconButton *nextButton_ = nullptr;
    IconButton *muteButton_ = nullptr;
    IconButton *fullscreenButton_ = nullptr;
    IconButton *playlistButton_ = nullptr;
    QLabel *timeLabel_ = nullptr;
    /// Playback > Speed, parallel to kSpeedChoices.
    QList<QAction *> speedActions_;

    QDockWidget *playlistDock_ = nullptr;
    PlaylistWidget *playlistWidget_ = nullptr;

    QAction *openAction_ = nullptr;
    QAction *openFolderAction_ = nullptr;
    QAction *openUrlAction_ = nullptr;
    QMenu *recentFilesMenu_ = nullptr;
    QAction *copyFileAction_ = nullptr;
    QAction *openFolderOfFileAction_ = nullptr;
    QAction *downloadUrlAction_ = nullptr;
    QAction *downloadCurrentAction_ = nullptr;
    QAction *exitAction_ = nullptr;
    QAction *loopOffAction_ = nullptr;
    QAction *loopFileAction_ = nullptr;
    QAction *loopPlaylistAction_ = nullptr;
    QMenu *chaptersMenu_ = nullptr;

    QPushButton *playlistRepeatButton_ = nullptr;

    QMenu *audioMenuTop_ = nullptr;
    QMenu *audioDeviceMenu_ = nullptr;
    QAction *audioStereoAction_ = nullptr;
    QAction *audioMonoAction_ = nullptr;
    QAction *audioAutoChannelsAction_ = nullptr;
    QAction *fullscreenAction_ = nullptr;
    QAction *settingsAction_ = nullptr;
    QAction *equalizerAction_ = nullptr;
    QAction *aboutAction_ = nullptr;
    /// Timeline and control bar together: docked below the video normally,
    /// floated over it in fullscreen.
    QWidget *controlsContainer_ = nullptr;
    QVBoxLayout *centralLayout_ = nullptr;
    QTimer *fullscreenControlsTimer_ = nullptr;

    /// Menu actions registered on the window for their shortcuts, and the F11
    /// shortcut, both of which have to be removed before the menus are rebuilt.
    QList<QPointer<QAction>> menuShortcutActions_;
    QPointer<QShortcut> fullscreenShortcut_;

    QMenu *fileMenu_ = nullptr;
    QMenu *playbackMenu_ = nullptr;
    QMenu *subtitlesMenu_ = nullptr;
    QMenu *showSubtitleMenu_ = nullptr;
    QAction *subEnabledAction_ = nullptr;
    QAction *subAddFileAction_ = nullptr;
    QAction *subBoldAction_ = nullptr;
    /// Track entries currently in the Show Subtitle submenu, owned by this
    /// window so clearing the submenu never destroys the permanent entries.
    QList<QAction *> subTrackActions_;

    QMenu *audioMenu_ = nullptr;

    QMenu *videoMenu_ = nullptr;
    QAction *flipAction_ = nullptr;
    QAction *mirrorAction_ = nullptr;
    QVector<QAction *> rotateActions_; // parallel to {0, 90, 180, 270}

    bool videoFlip_ = false;
    bool videoMirror_ = false;
    int videoRotation_ = 0;
    int videoFrameModeIndex_ = -1; ///< index into VideoSettings::frameModes()
    double videoAspectRatio_ = -1.0; ///< mpv video-aspect-override
    QVector<QAction *> frameModeActions_;
    QVector<QAction *> aspectActions_;
    QVector<QAction *> zoomActions_;
    int videoZoomPercent_ = 100;
    double videoPanX_ = 0.0;
    double videoPanY_ = 0.0;
    /// Picture adjustments in effect right now: the app-wide defaults, or this
    /// file's own values once it has any.
    VideoSettings::Adjust videoAdjust_;

    // What loadFile() was last handed: a local path or a URL. Kept so the
    // Download entries know whether there is anything worth downloading.
    QString currentMediaPath_;

    /// What the title bar is showing: mpv's own media title once it reports
    /// one, the file name until then, empty when nothing is playing.
    QString mediaTitle_;

    // mpv reports "not paused" before anything is loaded, which would show the
    // Pause icon on an idle player. Only true once a file has actually loaded.
    bool mediaLoaded_ = false;

    /// True while a playback-error box is on screen. Errors can arrive faster
    /// than they can be dismissed -- mpv retries a failing file, and each
    /// attempt reports again -- which would otherwise stack modal dialogs
    /// endlessly, with a new one appearing for every one closed.
    bool errorDialogOpen_ = false;

    /// "Resize the window to fit the video" from Settings, and whether the
    /// file just loaded still owes us that resize.
    bool fitWindowToVideo_ = false;
    bool pendingFitToVideo_ = false;

    /// "Always open files maximized" from Settings.
    bool openMaximized_ = false;

    /// Asks Windows not to sleep the machine, or blank the display, while
    /// something is actually playing.
    void updateSleepInhibit();
    bool hasVideo_ = false;
    /// The EXECUTION_STATE last handed to SetThreadExecutionState(), so it is
    /// only called when the answer changes. Plain integer to keep windows.h
    /// out of this header.
    unsigned long sleepInhibitState_ = 0;

    /// Since the last file handed over by another launch. Explorer starts one
    /// process per selected file, so arrivals inside this window belong to one
    /// selection and queue instead of each replacing the last.
    QElapsedTimer externalOpenTimer_;
    static constexpr qint64 kExternalOpenBurstMs = 1500;

    double currentPosition_ = 0.0;
    double currentDuration_ = 0.0;

    // False right after Stop/EOF so the async pause/position echo that
    // follows doesn't repaint the taskbar; a real "playing" transition
    // (pauseChanged(false)) re-arms it.
    bool taskbarActive_ = false;

    // Cached preferences (SettingsDialog), refreshed at startup and whenever
    // it closes with changes accepted.
    QString seekMode_;
    double seekSeconds_ = 5.0;
    double seekPercentage_ = 0.1;
    double seekMinSeconds_ = 2.0;
    int maxVolume_ = 200;
};
