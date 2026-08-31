#pragma once

#include <QString>
#include <QVector>

/// Picture adjustments and the geometry presets cycled by numpad 0.
namespace VideoSettings {

/// mpv's brightness/contrast/saturation/hue all take -100..100, 0 meaning
/// untouched.
inline constexpr int kMinAdjust = -100;
inline constexpr int kMaxAdjust = 100;

struct Adjust {
    int brightness = 0;
    int contrast = 0;
    int saturation = 0;
    int hue = 0;
    int gamma = 0;

    [[nodiscard]] bool isNeutral() const
    {
        return brightness == 0 && contrast == 0 && saturation == 0 && hue == 0 && gamma == 0;
    }
};

Adjust loadAdjust();
void saveAdjust(const Adjust &adjust);

/// How the picture is fitted into the window.
enum class FrameMode {
    Default,               ///< nothing applied: the video as it normally loads
    StretchToWindow,       ///< fills the window, ignoring the aspect ratio
    TouchWindowFromInside, ///< fits entirely inside, letterboxed
    Zoom1,
    Zoom2,
    TouchWindowFromOutside, ///< fills the window, cropping the overflow
    Stretch1,
    Stretch2,
    Stretch3,
    Zoom3,
};

struct FrameModeInfo {
    FrameMode mode;
    QString name;

    /// mpv keepaspect: false only for Stretch To Window.
    bool keepAspect = true;

    /// mpv panscan: 0 fits inside, 1 fills and crops, between the two for the
    /// intermediate zoom steps.
    double panscan = 0.0;

    /// mpv video-scale-y: stretches the picture vertically. 1 leaves it alone.
    double scaleY = 1.0;

    /// mpv video-zoom, a log2 factor. 0 leaves the size alone; this zooms
    /// past what panscan can reach, which stops at filling the window.
    double zoom = 0.0;

    /// Whether numpad 0 steps through this entry. Only Default and the
    /// stretch and zoom series are in the cycle, so one key runs from
    /// untouched through the steps and back to untouched. Window sizes and
    /// fill modes are left out: cycling those would resize the window out
    /// from under the viewer.
    bool inNumpadCycle = false;
};

/// Every frame mode, in menu order. Stretch To Window is the default.
const QVector<FrameModeInfo> &frameModes();

/// Index of the mode a video starts on when nothing is remembered for it.
int defaultFrameModeIndex();

/// One entry of the Aspect Ratio submenu.
struct AspectRatio {
    QString name;

    /// mpv video-aspect-override: -1 keeps the ratio the file declares, 0
    /// ignores it and treats the pixels as square.
    double value = -1.0;
};

const QVector<AspectRatio> &aspectRatios();

} // namespace VideoSettings
