#include "EqualizerDialog.h"
#include "Equalizer.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSlider>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QFrame>

EqualizerDialog::EqualizerDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Equalizer"));

    auto *mainLayout = new QVBoxLayout(this);

    // --- Sliders -----------------------------------------------------------
    auto *grid = new QGridLayout();
    grid->setHorizontalSpacing(10);

    // Scale down the left edge: top, middle and bottom of the travel.
    auto *scaleColumn = new QVBoxLayout();
    scaleColumn->addWidget(new QLabel(tr("+%1 dB").arg(Equalizer::kMaxGainDb), this));
    scaleColumn->addStretch(1);
    scaleColumn->addWidget(new QLabel(tr("0 dB"), this));
    scaleColumn->addStretch(1);
    scaleColumn->addWidget(new QLabel(tr("%1 dB").arg(Equalizer::kMinGainDb), this));
    grid->addLayout(scaleColumn, 0, 0);

    const QVector<double> &freqs = Equalizer::frequencies();
    const QVector<int> stored = Equalizer::loadGains();

    sliders_.reserve(freqs.size());
    for (int band = 0; band < freqs.size(); ++band) {
        auto *slider = new QSlider(Qt::Vertical, this);
        slider->setRange(Equalizer::kMinGainDb, Equalizer::kMaxGainDb);
        slider->setValue(stored.value(band, 0));
        slider->setMinimumHeight(170);
        slider->setTickPosition(QSlider::NoTicks);
        slider->setFocusPolicy(Qt::StrongFocus);
        slider->setToolTip(Equalizer::frequencyLabel(freqs.at(band)));
        connect(slider, &QSlider::valueChanged, this, &EqualizerDialog::onSliderMoved);

        grid->addWidget(slider, 0, band + 1, Qt::AlignHCenter);
        grid->addWidget(new QLabel(Equalizer::frequencyLabel(freqs.at(band)), this),
                         1, band + 1, Qt::AlignHCenter);
        sliders_.append(slider);
    }
    mainLayout->addLayout(grid);

    auto *divider = new QFrame(this);
    divider->setFrameShape(QFrame::HLine);
    divider->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(divider);

    // --- Preset and buttons -------------------------------------------------
    auto *bottomRow = new QHBoxLayout();
    bottomRow->addWidget(new QLabel(tr("Preset"), this));

    presetCombo_ = new QComboBox(this);
    for (const auto &preset : Equalizer::presets()) {
        presetCombo_->addItem(preset.name);
    }
    // Last entry, so a hand-edited curve has somewhere to show up without
    // pretending to be one of the named presets.
    presetCombo_->addItem(Equalizer::customPresetName());

    const int storedIndex = presetCombo_->findText(Equalizer::loadPresetName());
    presetCombo_->setCurrentIndex(storedIndex >= 0 ? storedIndex : 0);
    connect(presetCombo_, &QComboBox::activated, this, &EqualizerDialog::onPresetSelected);
    bottomRow->addWidget(presetCombo_, 1);

    auto *resetButton = new QPushButton(tr("Reset"), this);
    connect(resetButton, &QPushButton::clicked, this, &EqualizerDialog::resetToFlat);
    bottomRow->addWidget(resetButton);

    auto *closeButton = new QPushButton(tr("Close"), this);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    bottomRow->addWidget(closeButton);

    mainLayout->addLayout(bottomRow);
}

QVector<int> EqualizerDialog::currentGains() const
{
    QVector<int> gains;
    gains.reserve(sliders_.size());
    for (const QSlider *slider : sliders_) {
        gains.append(slider->value());
    }
    return gains;
}

void EqualizerDialog::applyAndStore()
{
    const QVector<int> gains = currentGains();
    Equalizer::saveGains(gains);
    Equalizer::savePresetName(presetCombo_->currentText());
    emit gainsChanged(gains);
}

void EqualizerDialog::onSliderMoved()
{
    if (updatingSliders_) {
        return;
    }

    // Moving a slider by hand no longer matches whichever preset was picked.
    const int customIndex = presetCombo_->findText(Equalizer::customPresetName());
    if (customIndex >= 0 && presetCombo_->currentIndex() != customIndex) {
        presetCombo_->setCurrentIndex(customIndex);
    }

    applyAndStore();
}

void EqualizerDialog::onPresetSelected(int index)
{
    const QVector<Equalizer::Preset> &presets = Equalizer::presets();
    if (index < 0 || index >= presets.size()) {
        return; // "Custom" itself carries no curve of its own
    }

    setSliderGains(presets.at(index).gains);
    applyAndStore();
    emit presetApplied(presets.at(index).name);
}

void EqualizerDialog::resetToFlat()
{
    setSliderGains(Equalizer::flatGains());
    presetCombo_->setCurrentIndex(0); // Flat is always first
    applyAndStore();
    emit presetApplied(presetCombo_->currentText());
}

void EqualizerDialog::setSliderGains(const QVector<int> &gainsDb)
{
    updatingSliders_ = true;
    for (int band = 0; band < sliders_.size(); ++band) {
        sliders_.at(band)->setValue(gainsDb.value(band, 0));
    }
    updatingSliders_ = false;
}
