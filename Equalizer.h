#pragma once

#include <QString>
#include <QVector>

/// Ten-band graphic equaliser settings, shared by EqualizerDialog (edits
/// them), MainWindow (applies them) and MpvPlayer (turns them into a filter
/// chain).
///
/// Gains are whole decibels so the sliders land on exact values; mpv is given
/// one biquad "equalizer" filter per band, which applies to every channel
/// regardless of the layout and works for video and audio files alike.
namespace Equalizer {

inline constexpr int kBandCount = 10;
inline constexpr int kMinGainDb = -12;
inline constexpr int kMaxGainDb = 12;

/// Band centre frequencies in Hz, one octave apart.
const QVector<double> &frequencies();

/// "31 Hz", "1 kHz" ... for the label under each slider.
QString frequencyLabel(double hz);

struct Preset {
    QString name;
    QVector<int> gains; ///< kBandCount entries, in dB
};

/// Built-in presets; the first is always Flat.
const QVector<Preset> &presets();

/// Name used in the preset box once the user moves a slider by hand.
const QString &customPresetName();

/// All zeroes -- also what a failed or absent stored value falls back to.
QVector<int> flatGains();

QVector<int> loadGains();
void saveGains(const QVector<int> &gains);

QString loadPresetName();
void savePresetName(const QString &name);

} // namespace Equalizer
