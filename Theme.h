#pragma once

#include <QColor>
#include <QString>
#include <QVector>

class QApplication;

/// App-wide dark theme. A single call at startup so every stock Qt widget
/// (menus, buttons, dialogs, tooltips, scrollbars...) renders dark, not
/// just the custom-painted sliders.
namespace Theme
{
/// The pear's own body green, sampled from the app icon. The colour every
/// other accent shade is derived from unless the user picks another.
inline const QColor kDefaultAccent(184, 217, 50);

/// One base colour drives the whole accent. Everything the app tints -- the
/// timeline and volume fills, checked buttons, and the Fusion palette's
/// Highlight, which is what colours sliders, checkboxes, spin boxes and list
/// selections -- is derived from it, so a single choice re-tints the app.
[[nodiscard]] QColor accent();

/// Lighter and darker ends of the accent gradient. accentStart() is the base
/// colour itself; accentEnd() is derived from it, keeping the same
/// relationship the hand-picked pear greens had.
[[nodiscard]] QColor accentStart();
[[nodiscard]] QColor accentEnd();

/// Text drawn on top of the accent -- near-black or white, whichever the
/// chosen colour can actually be read against.
[[nodiscard]] QColor onAccentText();

/// Puts a colour into effect immediately: rebuilds the palette and repaints
/// every widget, including the custom-painted ones. Does not store it.
void applyAccent(const QColor &color);

/// Reads and writes the stored accent. Kept apart from applyAccent() so the
/// Settings dialog can preview a colour and still discard it on Cancel.
[[nodiscard]] QColor loadAccent();
void saveAccent(const QColor &color);

/// Named colours offered in Settings, the first being kDefaultAccent.
struct Preset {
    QString name;
    QColor color;
};
[[nodiscard]] const QVector<Preset> &presets();

/// Applies the dark palette using the stored accent. Call once at startup.
void applyDark(QApplication &app);
}
