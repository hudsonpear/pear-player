#pragma once

#include <QDialog>
#include <QVector>

class QSlider;
class QComboBox;

/// Ten-band graphic equaliser: one vertical slider per band, a preset box and
/// a Reset button.
///
/// Edits apply immediately rather than on OK -- an equaliser is adjusted by
/// ear, so the effect has to be audible while dragging. Every change is also
/// stored, so there is nothing to confirm and the dialog only needs Close.
class EqualizerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit EqualizerDialog(QWidget *parent = nullptr);

signals:
    /// Emitted whenever the curve changes, with one gain in dB per band.
    void gainsChanged(const QVector<int> &gainsDb);

    /// A named preset was picked (or Reset chose Flat). Not emitted for a
    /// hand-dragged slider, which has no name worth reporting.
    void presetApplied(const QString &name);

private:
    void onSliderMoved();
    void onPresetSelected(int index);
    void resetToFlat();

    /// Pushes gains onto the sliders without the change being mistaken for a
    /// hand edit (which would switch the preset box to "Custom").
    void setSliderGains(const QVector<int> &gainsDb);

    [[nodiscard]] QVector<int> currentGains() const;

    /// Applies to the player and stores, in one place so no path can change
    /// the sound without also persisting it.
    void applyAndStore();

    QVector<QSlider *> sliders_;
    QComboBox *presetCombo_ = nullptr;

    /// True while setSliderGains() is running, so the resulting valueChanged
    /// signals do not look like the user dragging a slider.
    bool updatingSliders_ = false;
};
