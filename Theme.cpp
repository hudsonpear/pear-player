#include "Theme.h"

#include <QApplication>
#include <QPalette>
#include <QSettings>
#include <QStyleFactory>
#include <QWidget>
#include <QColor>

#include <algorithm>
#include <cmath>

namespace Theme
{
namespace {

const QString kAccentKey = QStringLiteral("theme/accentColor");

// The colour in effect. Cached rather than read from QSettings on every paint:
// the sliders ask for it on each repaint.
QColor g_accent = kDefaultAccent;

/// The darker end of the gradient.
///
/// Taken from the relationship the two hand-picked pear greens already had
/// rather than a plain darker(): measured in HSV, the leaf green sits 9 degrees
/// round the wheel from the body green, is slightly more saturated, and is
/// about 82.5% as bright. Reproducing that shift means the default colour still
/// yields exactly the original pair, and any other colour gets a second shade
/// that relates to it the same way.
QColor deriveEnd(const QColor &base)
{
    int hue = 0;
    int saturation = 0;
    int value = 0;
    base.getHsv(&hue, &saturation, &value);
    // Grey has no hue (-1); rotating that would produce a colour out of nowhere.
    if (hue < 0) {
        return base.darker(121);
    }
    return QColor::fromHsv((hue + 9) % 360,
                            std::min(255, saturation + 8),
                            qRound(value * 0.825));
}

} // namespace

QColor accent()
{
    return g_accent;
}

QColor accentStart()
{
    return g_accent;
}

QColor accentEnd()
{
    return deriveEnd(g_accent);
}

namespace {

/// WCAG relative luminance.
double relativeLuminance(const QColor &color)
{
    const auto channel = [](double value) {
        value /= 255.0;
        return value <= 0.03928 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * channel(color.red())
        + 0.7152 * channel(color.green())
        + 0.0722 * channel(color.blue());
}

double contrastRatio(const QColor &a, const QColor &b)
{
    const double la = relativeLuminance(a);
    const double lb = relativeLuminance(b);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}

} // namespace

QColor onAccentText()
{
    // Whichever of the two actually reads better on this colour, measured as a
    // contrast ratio rather than guessed from a brightness threshold. A fixed
    // threshold put white on mid teal and mid slate, where both fell under the
    // WCAG AA minimum of 4.5:1 -- dark text clears it comfortably on both.
    const QColor fill = accentEnd();
    const QColor dark(20, 22, 16);
    const QColor light(255, 255, 255);
    return contrastRatio(fill, dark) >= contrastRatio(fill, light) ? dark : light;
}

QColor loadAccent()
{
    const QSettings settings;
    const QColor stored(settings.value(kAccentKey).toString());
    return stored.isValid() ? stored : kDefaultAccent;
}

void saveAccent(const QColor &color)
{
    QSettings settings;
    settings.setValue(kAccentKey, color.name(QColor::HexRgb));
}

const QVector<Preset> &presets()
{
    static const QVector<Preset> list = {
        {QCoreApplication::translate("Theme", "Pear"), kDefaultAccent},
        {QCoreApplication::translate("Theme", "Sky"), QColor(74, 158, 245)},
        {QCoreApplication::translate("Theme", "Violet"), QColor(163, 122, 240)},
        {QCoreApplication::translate("Theme", "Rose"), QColor(235, 95, 125)},
        {QCoreApplication::translate("Theme", "Amber"), QColor(240, 172, 58)},
        {QCoreApplication::translate("Theme", "Teal"), QColor(56, 199, 186)},
        {QCoreApplication::translate("Theme", "Slate"), QColor(150, 162, 178)},
    };
    return list;
}

void applyAccent(const QColor &color)
{
    if (color.isValid()) {
        g_accent = color;
    }

    if (auto *app = qobject_cast<QApplication *>(QCoreApplication::instance())) {
        QPalette palette = app->palette();
        palette.setColor(QPalette::Link, accentStart());
        palette.setColor(QPalette::Highlight, accentEnd());
        palette.setColor(QPalette::HighlightedText, onAccentText());
        app->setPalette(palette);
    }

    // A new palette repaints the stock widgets by itself, but the sliders and
    // icon buttons paint with the accent directly, and nothing tells them the
    // colour moved.
    const QWidgetList widgets = QApplication::allWidgets();
    for (QWidget *widget : widgets) {
        widget->update();
    }
}

void applyDark(QApplication &app)
{
    // The native Windows style ignores most palette roles, so switch to
    // Fusion first -- it's the only built-in style that actually respects
    // a custom QPalette for menus, buttons, combo boxes, etc.
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    g_accent = loadAccent();

    QPalette palette;
    palette.setColor(QPalette::Window, QColor(37, 37, 38));
    palette.setColor(QPalette::WindowText, Qt::white);
    palette.setColor(QPalette::Base, QColor(25, 25, 25));
    palette.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
    palette.setColor(QPalette::ToolTipBase, QColor(45, 45, 45));
    palette.setColor(QPalette::ToolTipText, Qt::white);
    palette.setColor(QPalette::Text, Qt::white);
    palette.setColor(QPalette::Button, QColor(53, 53, 53));
    palette.setColor(QPalette::ButtonText, Qt::white);
    palette.setColor(QPalette::BrightText, Qt::red);
    // Fusion paints slider grooves, check indicators, spin-box and list
    // selection from Highlight, so this is what makes every input in the
    // Settings and Equalizer dialogs match the timeline and volume sliders
    // rather than showing Windows blue.
    palette.setColor(QPalette::Link, accentStart());
    palette.setColor(QPalette::Highlight, accentEnd());
    // Dark text on the green: white on a mid-green highlight is hard to read,
    // which the default blue did not suffer from. Recomputed per accent, since
    // a dark blue or violet needs the opposite.
    palette.setColor(QPalette::HighlightedText, onAccentText());

    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(127, 127, 127));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(127, 127, 127));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(127, 127, 127));

    // Deliberately not using QApplication::setStyleSheet(): an app-wide
    // stylesheet forces every widget onto a different paint path, which is
    // a known trigger for QOpenGLWidget (VideoWidget) to render on top of
    // its sibling widgets instead of respecting normal stacking order. The
    // palette entries above already cover tooltip colors without it.
    app.setPalette(palette);
}

} // namespace Theme
