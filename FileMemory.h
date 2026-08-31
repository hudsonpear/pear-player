#pragma once

#include <QString>

/// Per-file playback state, remembered so a video picks up where its settings
/// were left rather than reverting to the defaults every time.
///
/// Stored under a hash of the media path, which keeps QSettings keys valid for
/// paths containing characters QSettings treats specially (slashes above all).
/// The original path is stored alongside so entries stay human-readable.
struct FileMemory
{
    QString subtitlePath;        ///< External subtitle added for this file
    double subtitleDelay = 0.0;  ///< mpv sub-delay, in seconds

    /// Chosen subtitle track (mpv sid), which is what picks the language on a
    /// file carrying several. -1 means nothing was chosen for this file, so
    /// mpv's own default selection stands.
    int subtitleTrackId = -1;

    /// Whether subtitles were left showing: -1 unset, 0 hidden, 1 shown.
    /// Tri-state rather than bool so "never touched" stays distinct from
    /// "deliberately turned off".
    int subtitleVisible = -1;

    /// True once this file has had its picture or geometry changed. The whole
    /// video block below is then authoritative for it.
    ///
    /// A flag rather than "are the values non-default?": a file deliberately
    /// set back to neutral while the app-wide defaults are not neutral has to
    /// stay neutral, which an all-defaults test could not express.
    bool hasVideoState = false;

    int rotation = 0;            ///< mpv video-rotate: 0, 90, 180 or 270
    bool flip = false;           ///< vertical flip
    bool mirror = false;         ///< horizontal flip
    int brightness = 0;
    int contrast = 0;
    int saturation = 0;
    int hue = 0;
    int gamma = 0;
    int frameModeIndex = -1;     ///< index into VideoSettings::frameModes(), -1 unset
    double aspectRatio = -1.0;   ///< mpv video-aspect-override
    int zoomPercent = 100;       ///< Zoom submenu: 100 leaves the size alone
    double panX = 0.0;           ///< mpv video-pan-x, fraction of video size
    double panY = 0.0;           ///< mpv video-pan-y

    /// Where playback was left, in seconds. Negative means nothing stored, so
    /// a file watched from the very start is not confused with an unwatched
    /// one. Independent of hasVideoState: resuming is its own feature and has
    /// its own setting.
    double playbackPosition = -1.0;

    /// Audio delay in seconds, remembered like the subtitle delay.
    double audioDelay = 0.0;

    /// Nothing worth writing: avoids creating an entry for every file merely
    /// opened and played with default settings.
    [[nodiscard]] bool isEmpty() const
    {
        return subtitlePath.isEmpty() && qFuzzyIsNull(subtitleDelay)
            && subtitleTrackId < 0 && subtitleVisible < 0 && !hasVideoState
            && playbackPosition < 0.0 && qFuzzyIsNull(audioDelay);
    }
};

namespace FileMemoryStore {

/// Returns a default-constructed FileMemory when nothing is stored, so the
/// caller never has to distinguish "absent" from "all defaults".
FileMemory load(const QString &mediaPath);

/// Writes the entry, or removes it when the state is back to defaults.
void save(const QString &mediaPath, const FileMemory &memory);

void remove(const QString &mediaPath);

/// Drops every remembered file, for the button in Settings.
void clearAll();

/// How many files currently have remembered settings.
int count();

} // namespace FileMemoryStore
