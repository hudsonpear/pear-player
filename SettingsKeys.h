#pragma once

#include <QString>
#include <QVariant>
#include <QSettings>
#include <QStandardPaths>

/// QSettings keys and defaults for user preferences, shared between
/// SettingsDialog (writes them) and MainWindow (reads/applies them).
namespace SettingsKeys {

inline const QString kSeekMode = QStringLiteral("seek/mode"); // "seconds" | "percentage"
inline const QString kSeekSeconds = QStringLiteral("seek/seconds");
inline const QString kSeekPercentage = QStringLiteral("seek/percentage");
inline const QString kSeekMinSeconds = QStringLiteral("seek/minSeconds");
inline const QString kMaxVolume = QStringLiteral("volume/max");
inline const QString kClickToPause = QStringLiteral("input/clickToPause");
inline const QString kDoubleClickAction = QStringLiteral("input/doubleClickAction"); // "fullscreen" | "playpause"
inline const QString kStartupPosition = QStringLiteral("window/startupPosition"); // "center" | "topleft"
inline const QString kHwDecEnabled = QStringLiteral("playback/hwDecEnabled");

// --- Subtitle appearance (see SubtitleStyle) ---------------------------
inline const QString kSubUseDefaults = QStringLiteral("subtitles/useDefaults");
inline const QString kSubFontFamily = QStringLiteral("subtitles/fontFamily");
inline const QString kSubFontSize = QStringLiteral("subtitles/fontSize");
inline const QString kSubBold = QStringLiteral("subtitles/bold");
inline const QString kSubColor = QStringLiteral("subtitles/color");
inline const QString kSubOutlineColor = QStringLiteral("subtitles/outlineColor");
inline const QString kSubOutlineThickness = QStringLiteral("subtitles/outlineThickness");
inline const QString kSubShadowEnabled = QStringLiteral("subtitles/shadowEnabled");
inline const QString kSubShadowColor = QStringLiteral("subtitles/shadowColor");
inline const QString kSubShadowSize = QStringLiteral("subtitles/shadowSize");
inline const QString kSubLetterSpacing = QStringLiteral("subtitles/letterSpacing");
inline const QString kSubLineSpacing = QStringLiteral("subtitles/lineSpacing");
inline const QString kSubVerticalPosition = QStringLiteral("subtitles/verticalPosition");
inline const QString kSubAlign = QStringLiteral("subtitles/align"); // "left" | "center" | "right"

/// Delay stored by "Save the Current Sync", re-applied to every file loaded
/// afterwards. NaN-free: absent means "no saved sync".
inline const QString kSubSavedDelay = QStringLiteral("subtitles/savedDelay");

// --- Video (see VideoSettings.h) ---------------------------------------
inline const QString kVideoBrightness = QStringLiteral("video/brightness");
inline const QString kVideoContrast = QStringLiteral("video/contrast");
inline const QString kVideoSaturation = QStringLiteral("video/saturation");
inline const QString kVideoHue = QStringLiteral("video/hue");
inline const QString kVideoGamma = QStringLiteral("video/gamma");

/// When set, chapter starts are drawn as ticks on the timeline.
inline const QString kShowChapterMarkers = QStringLiteral("playback/showChapterMarkers");
constexpr bool kDefaultShowChapterMarkers = true;

/// When set, finishing a file starts the next playlist entry.
inline const QString kAutoAdvance = QStringLiteral("playback/autoAdvance");
constexpr bool kDefaultAutoAdvance = true;

/// When set, playback resumes where each file was left off.
inline const QString kRememberPosition = QStringLiteral("playback/rememberPosition");
constexpr bool kDefaultRememberPosition = false;

/// When set, opening a file resizes the window so the picture is shown at its
/// own pixel size. Ignored in fullscreen and while maximized.
inline const QString kFitWindowToVideo = QStringLiteral("playback/fitWindowToVideo");
constexpr bool kDefaultFitWindowToVideo = false;

/// When set, opening a file maximizes the window. Takes precedence over
/// kFitWindowToVideo, which cannot size a maximized window.
inline const QString kOpenMaximized = QStringLiteral("playback/openMaximized");
constexpr bool kDefaultOpenMaximized = false;

/// When set, the window reopens at the size and position it was closed at,
/// in place of kStartupPosition.
inline const QString kRememberGeometry = QStringLiteral("window/rememberGeometry");
constexpr bool kDefaultRememberGeometry = false;
/// The saved geometry itself, as QWidget::saveGeometry() produced it.
inline const QString kWindowGeometry = QStringLiteral("window/geometry");

/// When set, the window stays above other windows.
inline const QString kAlwaysOnTop = QStringLiteral("window/alwaysOnTop");
constexpr bool kDefaultAlwaysOnTop = false;

/// Where the S key writes frames. Empty means the default below, resolved at
/// use rather than stored, so it still points somewhere sensible if the user's
/// Pictures folder moves.
inline const QString kScreenshotDir = QStringLiteral("screenshots/directory");

/// Interface language: the base name of a file in the translations folder, or
/// empty for English, which is the text built into the app.
inline const QString kLanguage = QStringLiteral("ui/language");

/// The folder screenshots go to: the stored one, or a "Pear Player" folder
/// under the user's Pictures when nothing has been chosen.
inline QString screenshotDirectory()
{
    const QSettings settings;
    const QString stored = settings.value(kScreenshotDir).toString();
    if (!stored.isEmpty()) {
        return stored;
    }
    const QString pictures = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    // PicturesLocation can be empty on an unusual profile; the home folder is
    // always somewhere the user can write.
    const QString base = pictures.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::HomeLocation)
        : pictures;
    return base + QStringLiteral("/Pear Player");
}

/// Most-recently-opened files, newest first, capped at kMaxRecentFiles.
inline const QString kRecentFiles = QStringLiteral("recent/files");
constexpr int kMaxRecentFiles = 10;

/// A file closed within this many seconds of its start or end is treated as
/// "not really part-watched", so trivial positions are not stored.
constexpr double kResumeMinSeconds = 20.0;
// Flip, mirror and the zoom/stretch preset are deliberately not stored here:
// they belong to the video they were chosen for, and live in its FileMemory.

/// Group holding one subgroup per remembered media file (see FileMemory.h).
inline const QString kFileMemoryGroup = QStringLiteral("fileMemory");

// --- Equalizer (see Equalizer.h) ---------------------------------------
/// Band gains in dB as a comma-separated list, one entry per band.
inline const QString kEqualizerGains = QStringLiteral("equalizer/gains");
inline const QString kEqualizerPreset = QStringLiteral("equalizer/preset");

constexpr double kDefaultSeekSeconds = 5.0;
constexpr double kDefaultSeekPercentage = 0.1;
constexpr double kDefaultSeekMinSeconds = 2.0;
constexpr int kDefaultMaxVolume = 200;
constexpr bool kDefaultClickToPause = true;
// Off by default: hardware decoding (mpv hwdec=auto-safe) produces corrupted
// frames -- horizontal colored line artifacts -- on some GPU/driver
// combinations. Software decoding is slower but always renders correctly.
constexpr bool kDefaultHwDecEnabled = false;

/// Reads a signed integer back out of QSettings.
///
/// On Windows an int is written to the registry as a REG_DWORD, so a negative
/// value reads back as an out-of-range unsigned number and QVariant::toInt()
/// gives up and returns 0 -- which silently turned negative brightness,
/// contrast and hue into zero. Reinterpreting the 32-bit pattern recovers the
/// original value, including from settings written before this was fixed.
inline int readSignedInt(const QVariant &value, int fallback = 0)
{
    if (!value.isValid()) {
        return fallback;
    }

    bool ok = false;
    const int direct = value.toInt(&ok);
    if (ok) {
        return direct;
    }

    const qulonglong raw = value.toULongLong(&ok);
    if (ok) {
        return static_cast<int>(static_cast<quint32>(raw));
    }
    return fallback;
}

} // namespace SettingsKeys
