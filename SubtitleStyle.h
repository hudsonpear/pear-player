#pragma once

#include <QColor>
#include <QString>

/// User-configurable subtitle appearance, persisted in QSettings and applied
/// to mpv as a group.
///
/// Colors carry their own alpha, which is what the "transparency" controls in
/// the Settings dialog edit -- mpv takes colors as #AARRGGBB where FF is fully
/// opaque, so alpha and color are one value rather than two properties.
struct SubtitleStyle
{
    /// When true nothing below is applied and mpv keeps its own defaults.
    /// Kept as an explicit flag so a user can experiment and get back to a
    /// known-good look without having to remember the original numbers.
    bool useDefaults = true;

    QString fontFamily = QStringLiteral("sans-serif");
    double fontSize = 38.0;
    bool bold = false;

    QColor fontColor = QColor(255, 255, 255, 255);
    QColor outlineColor = QColor(0, 0, 0, 255);
    double outlineThickness = 1.65;

    bool shadowEnabled = false;
    QColor shadowColor = QColor(0, 0, 0, 175);
    double shadowSize = 0.0;

    double letterSpacing = 0.0;  ///< mpv sub-spacing, in pixels.
    double lineSpacing = 0.0;    ///< mpv sub-ass-line-spacing, in pixels.

    // No horizontal offset: mpv writes its single sub-margin-x value into both
    // margins, so they cancel and centred subtitles cannot be moved sideways.
    double verticalPosition = 100.0; ///< mpv sub-pos, 0 (top) to 150.

    QString align = QStringLiteral("center"); ///< left | center | right

    /// Reads the stored style, falling back to the defaults above.
    [[nodiscard]] static SubtitleStyle load();
    void save() const;

    /// mpv's own factory values, for the "use default settings" path and the
    /// Reset button in the Settings dialog.
    [[nodiscard]] static SubtitleStyle mpvDefaults();
};
