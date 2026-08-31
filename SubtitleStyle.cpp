#include "SubtitleStyle.h"
#include "SettingsKeys.h"

#include <QSettings>

namespace {

// Colors round-trip through #AARRGGBB text, the same spelling mpv accepts, so
// what is stored is exactly what is later handed to the player.
QColor readColor(const QSettings &settings, const QString &key, const QColor &fallback)
{
    const QString stored = settings.value(key).toString();
    if (stored.isEmpty()) {
        return fallback;
    }
    const QColor color = QColor::fromString(stored);
    return color.isValid() ? color : fallback;
}

} // namespace

SubtitleStyle SubtitleStyle::mpvDefaults()
{
    return SubtitleStyle{}; // the member initialisers are mpv's own defaults
}

SubtitleStyle SubtitleStyle::load()
{
    const QSettings settings;
    const SubtitleStyle defaults;
    SubtitleStyle style;

    style.useDefaults = settings.value(SettingsKeys::kSubUseDefaults, defaults.useDefaults).toBool();
    style.fontFamily = settings.value(SettingsKeys::kSubFontFamily, defaults.fontFamily).toString();
    style.fontSize = settings.value(SettingsKeys::kSubFontSize, defaults.fontSize).toDouble();
    style.bold = settings.value(SettingsKeys::kSubBold, defaults.bold).toBool();

    style.fontColor = readColor(settings, SettingsKeys::kSubColor, defaults.fontColor);
    style.outlineColor = readColor(settings, SettingsKeys::kSubOutlineColor, defaults.outlineColor);
    style.outlineThickness = settings.value(SettingsKeys::kSubOutlineThickness, defaults.outlineThickness).toDouble();

    style.shadowEnabled = settings.value(SettingsKeys::kSubShadowEnabled, defaults.shadowEnabled).toBool();
    style.shadowColor = readColor(settings, SettingsKeys::kSubShadowColor, defaults.shadowColor);
    style.shadowSize = settings.value(SettingsKeys::kSubShadowSize, defaults.shadowSize).toDouble();

    style.letterSpacing = settings.value(SettingsKeys::kSubLetterSpacing, defaults.letterSpacing).toDouble();
    style.lineSpacing = settings.value(SettingsKeys::kSubLineSpacing, defaults.lineSpacing).toDouble();

    style.verticalPosition = settings.value(SettingsKeys::kSubVerticalPosition, defaults.verticalPosition).toDouble();
    style.align = settings.value(SettingsKeys::kSubAlign, defaults.align).toString();

    return style;
}

void SubtitleStyle::save() const
{
    QSettings settings;
    settings.setValue(SettingsKeys::kSubUseDefaults, useDefaults);
    settings.setValue(SettingsKeys::kSubFontFamily, fontFamily);
    settings.setValue(SettingsKeys::kSubFontSize, fontSize);
    settings.setValue(SettingsKeys::kSubBold, bold);
    settings.setValue(SettingsKeys::kSubColor, fontColor.name(QColor::HexArgb));
    settings.setValue(SettingsKeys::kSubOutlineColor, outlineColor.name(QColor::HexArgb));
    settings.setValue(SettingsKeys::kSubOutlineThickness, outlineThickness);
    settings.setValue(SettingsKeys::kSubShadowEnabled, shadowEnabled);
    settings.setValue(SettingsKeys::kSubShadowColor, shadowColor.name(QColor::HexArgb));
    settings.setValue(SettingsKeys::kSubShadowSize, shadowSize);
    settings.setValue(SettingsKeys::kSubLetterSpacing, letterSpacing);
    settings.setValue(SettingsKeys::kSubLineSpacing, lineSpacing);
    settings.setValue(SettingsKeys::kSubVerticalPosition, verticalPosition);
    settings.setValue(SettingsKeys::kSubAlign, align);
}
