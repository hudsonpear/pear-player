#include "Equalizer.h"
#include "SettingsKeys.h"

#include <QSettings>
#include <QStringList>
#include <QCoreApplication>

#include <algorithm>
#include <cmath>

namespace Equalizer {

const QVector<double> &frequencies()
{
    static const QVector<double> freqs = {31, 62, 125, 250, 500, 1000, 2000, 4000, 8000, 16000};
    return freqs;
}

QString frequencyLabel(double hz)
{
    // Fixed notation, not 'g': significant-digit formatting turns 125 into
    // "1.3e+02". Whole numbers print without a decimal point, so 16000 reads
    // as "16 kHz" rather than "16.0 kHz".
    const auto trim = [](double value) {
        const bool whole = qFuzzyCompare(value, std::round(value));
        return QString::number(value, 'f', whole ? 0 : 1);
    };

    if (hz >= 1000.0) {
        return QCoreApplication::translate("Equalizer", "%1 kHz").arg(trim(hz / 1000.0));
    }
    return QCoreApplication::translate("Equalizer", "%1 Hz").arg(trim(hz));
}

QVector<int> flatGains()
{
    return QVector<int>(kBandCount, 0);
}

const QVector<Preset> &presets()
{
    // Shapes are the usual graphic-EQ curves, kept well inside the +/-12 dB
    // range so no preset clips on its own.
    static const QVector<Preset> all = {
        {QCoreApplication::translate("Equalizer", "Flat"),          {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
        {QCoreApplication::translate("Equalizer", "Rock"),          {5, 4, 2, -1, -2, 0, 2, 4, 5, 5}},
        {QCoreApplication::translate("Equalizer", "Pop"),           {-1, 1, 3, 4, 4, 2, 0, -1, -1, -2}},
        {QCoreApplication::translate("Equalizer", "Jazz"),          {4, 3, 1, 2, -1, -1, 0, 1, 3, 4}},
        {QCoreApplication::translate("Equalizer", "Classical"),     {5, 4, 3, 2, -1, -1, 0, 2, 3, 4}},
        {QCoreApplication::translate("Equalizer", "Bass Boost"),    {8, 7, 5, 3, 1, 0, 0, 0, 0, 0}},
        {QCoreApplication::translate("Equalizer", "Treble Boost"),  {0, 0, 0, 0, 0, 1, 3, 5, 7, 8}},
        {QCoreApplication::translate("Equalizer", "Vocal"),         {-2, -1, 0, 3, 5, 5, 4, 2, 0, -1}},
    };
    return all;
}

const QString &customPresetName()
{
    static const QString name = QCoreApplication::translate("Equalizer", "Custom");
    return name;
}

QVector<int> loadGains()
{
    const QSettings settings;
    const QString stored = settings.value(SettingsKeys::kEqualizerGains).toString();
    if (stored.isEmpty()) {
        return flatGains();
    }

    const QStringList parts = stored.split(QLatin1Char(','), Qt::SkipEmptyParts);
    if (parts.size() != kBandCount) {
        return flatGains(); // written by a different band count; start over
    }

    QVector<int> gains;
    gains.reserve(kBandCount);
    for (const QString &part : parts) {
        bool ok = false;
        const int value = part.toInt(&ok);
        if (!ok) {
            return flatGains();
        }
        gains.append(std::clamp(value, kMinGainDb, kMaxGainDb));
    }
    return gains;
}

void saveGains(const QVector<int> &gains)
{
    QStringList parts;
    parts.reserve(gains.size());
    for (int gain : gains) {
        parts.append(QString::number(gain));
    }

    QSettings settings;
    settings.setValue(SettingsKeys::kEqualizerGains, parts.join(QLatin1Char(',')));
}

QString loadPresetName()
{
    const QSettings settings;
    return settings.value(SettingsKeys::kEqualizerPreset, presets().first().name).toString();
}

void savePresetName(const QString &name)
{
    QSettings settings;
    settings.setValue(SettingsKeys::kEqualizerPreset, name);
}

} // namespace Equalizer
