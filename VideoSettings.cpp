#include "VideoSettings.h"
#include "SettingsKeys.h"

#include <QSettings>
#include <QCoreApplication>

#include <algorithm>

namespace VideoSettings {

namespace {

int clampAdjust(const QVariant &value)
{
    return std::clamp(SettingsKeys::readSignedInt(value, 0), kMinAdjust, kMaxAdjust);
}

} // namespace

Adjust loadAdjust()
{
    const QSettings settings;
    Adjust adjust;
    adjust.brightness = clampAdjust(settings.value(SettingsKeys::kVideoBrightness, 0));
    adjust.contrast = clampAdjust(settings.value(SettingsKeys::kVideoContrast, 0));
    adjust.saturation = clampAdjust(settings.value(SettingsKeys::kVideoSaturation, 0));
    adjust.hue = clampAdjust(settings.value(SettingsKeys::kVideoHue, 0));
    adjust.gamma = clampAdjust(settings.value(SettingsKeys::kVideoGamma, 0));
    return adjust;
}

void saveAdjust(const Adjust &adjust)
{
    QSettings settings;
    settings.setValue(SettingsKeys::kVideoBrightness, adjust.brightness);
    settings.setValue(SettingsKeys::kVideoContrast, adjust.contrast);
    settings.setValue(SettingsKeys::kVideoSaturation, adjust.saturation);
    settings.setValue(SettingsKeys::kVideoHue, adjust.hue);
    settings.setValue(SettingsKeys::kVideoGamma, adjust.gamma);
}

const QVector<FrameModeInfo> &frameModes()
{
    const auto name = [](const char *text) {
        return QCoreApplication::translate("VideoSettings", text);
    };

    // Columns after the name: keep-aspect, panscan, scale-y, zoom, and
    // whether numpad 0 steps through it.
    //
    // Default plus the last six form the numpad-0 cycle: untouched, three
    // vertical stretches, then three zoom steps, and back to untouched.
    // video-zoom is a log2 factor, so 0.25 / 0.5 / 0.75 are roughly 1.19x,
    // 1.41x and 1.68x.
    static const QVector<FrameModeInfo> all = {
        {FrameMode::Default,                name("Default"),                   true,  0.0, 1.0,  0.0,  true},

        {FrameMode::StretchToWindow,        name("Stretch To Window"),         false, 0.0, 1.0,  0.0,  false},
        {FrameMode::TouchWindowFromInside,  name("Touch Window From Inside"),  true,  0.0, 1.0,  0.0,  false},
        {FrameMode::TouchWindowFromOutside, name("Touch Window From Outside"), true,  1.0, 1.0,  0.0,  false},

        {FrameMode::Stretch1,               name("Stretch 1"),                 true,  0.0, 1.10, 0.0,  true},
        {FrameMode::Stretch2,               name("Stretch 2"),                 true,  0.0, 1.20, 0.0,  true},
        {FrameMode::Stretch3,               name("Stretch 3"),                 true,  0.0, 1.30, 0.0,  true},
        {FrameMode::Zoom1,                  name("Zoom 1"),                    true,  0.0, 1.0,  0.25, true},
        {FrameMode::Zoom2,                  name("Zoom 2"),                    true,  0.0, 1.0,  0.50, true},
        {FrameMode::Zoom3,                  name("Zoom 3"),                    true,  0.0, 1.0,  0.75, true},
    };
    return all;
}

int defaultFrameModeIndex()
{
    const QVector<FrameModeInfo> &modes = frameModes();
    for (int i = 0; i < modes.size(); ++i) {
        if (modes.at(i).mode == FrameMode::Default) {
            return i;
        }
    }
    return 0;
}

const QVector<AspectRatio> &aspectRatios()
{
    const auto name = [](const char *text) {
        return QCoreApplication::translate("VideoSettings", text);
    };

    static const QVector<AspectRatio> all = {
        {name("Default (DAR)"), -1.0},
        {QStringLiteral("4:3"), 4.0 / 3.0},
        {QStringLiteral("5:4"), 5.0 / 4.0},
        {QStringLiteral("16:9"), 16.0 / 9.0},
        {QStringLiteral("235:100"), 2.35},
        {QStringLiteral("185:100"), 1.85},
        // 0 tells mpv to ignore the ratio the file declares and take the
        // stored pixel dimensions at face value.
        {name("Assume square pixels (SAR)"), 0.0},
    };
    return all;
}

} // namespace VideoSettings
