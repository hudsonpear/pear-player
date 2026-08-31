#pragma once

#include "SubtitleStyle.h"

#include <QDialog>
#include <QColor>

class QRadioButton;
class QLineEdit;
class QDoubleSpinBox;
class QSpinBox;
class QComboBox;
class QCheckBox;
class QPushButton;
class QSlider;
class QTabWidget;
class QFontComboBox;
class QWidget;

/// Preferences dialog. Reads current values from QSettings on construction
/// and writes them back only on OK; Cancel discards edits untouched.
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);

    /// Opens with the Subtitles tab in front, for the "Subtitles Settings"
    /// entry in the Subtitles menu.
    void showSubtitlesTab();


signals:
    /// Emitted as the controls move, so the picture and subtitles change
    /// while the dialog is open. Nothing is stored until OK, so the caller
    /// is responsible for putting the old values back on Cancel.
    void videoAdjustPreviewed(int brightness, int contrast, int saturation, int hue, int gamma);
    void subtitleStylePreviewed(const SubtitleStyle &style);

private:
    QWidget *createGeneralTab();

    /// Window, click behaviour, screenshots and theme -- split out of General
    /// to keep the dialog short enough for smaller screens.
    QWidget *createInterfaceTab();
    QWidget *createSubtitlesTab();
    QWidget *createVideoTab();

    /// Read-only reference list of the player's fixed hotkeys.
    QWidget *createHotkeysTab();

    /// Keeps the "delete saved file data" button's label in step with how many
    /// files are actually remembered, including after it is pressed.
    void refreshFileDataButton();

    /// Reads the subtitle controls into a style, shared by the live preview
    /// and by saving on OK so the two can never disagree.
    [[nodiscard]] SubtitleStyle styleFromControls() const;

    void emitVideoPreview();
    void emitSubtitlePreview();

    /// Connects every subtitle control to emitSubtitlePreview() in one place,
    /// so a control added later is easy to remember to hook up.
    void connectSubtitlePreviewSignals();

    /// Puts an accent colour into effect straight away and updates the swatch.
    /// Kept out of QSettings until OK, so Cancel can restore originalAccent_.
    void previewAccent(const QColor &color);

    void loadFromSettings();
    void saveToSettings();

    /// Greys out every subtitle control while "use default settings" is on,
    /// so the dialog shows that those values are not in effect.
    void updateSubtitleControlsEnabled();

    /// Fills the subtitle controls from a style. Shared by the initial load
    /// and the Reset button, so both routes cannot drift apart.
    void applySubtitleStyleToControls(const SubtitleStyle &style);

    QTabWidget *tabs_ = nullptr;
    int subtitlesTabIndex_ = -1;

    // --- Video -----------------------------------------------------------
    QSlider *brightnessSlider_ = nullptr;
    QSlider *contrastSlider_ = nullptr;
    QSlider *saturationSlider_ = nullptr;
    QSlider *hueSlider_ = nullptr;
    QSlider *gammaSlider_ = nullptr;
    QPushButton *clearFileDataButton_ = nullptr;
    QCheckBox *rememberPositionCheck_ = nullptr;
    QCheckBox *autoAdvanceCheck_ = nullptr;
    QCheckBox *chapterMarkersCheck_ = nullptr;
    QCheckBox *fitWindowCheck_ = nullptr;
    QCheckBox *openMaximizedCheck_ = nullptr;

    // --- Theme -----------------------------------------------------------
    QPushButton *accentButton_ = nullptr;
    /// The accent as it was when the dialog opened, for Cancel.
    QColor originalAccent_;

    // --- General ---------------------------------------------------------
    QRadioButton *seekFixedRadio_ = nullptr;
    QRadioButton *seekPercentRadio_ = nullptr;
    QDoubleSpinBox *seekSecondsSpin_ = nullptr;
    QDoubleSpinBox *seekPercentSpin_ = nullptr;
    QDoubleSpinBox *seekMinSecondsSpin_ = nullptr;

    QComboBox *maxVolumeCombo_ = nullptr;

    QCheckBox *clickToPauseCheck_ = nullptr;
    QCheckBox *hwDecCheck_ = nullptr;

    QRadioButton *dblFullscreenRadio_ = nullptr;
    QRadioButton *dblPlayPauseRadio_ = nullptr;

    QRadioButton *startupCenterRadio_ = nullptr;
    QRadioButton *startupTopLeftRadio_ = nullptr;
    QCheckBox *rememberGeometryCheck_ = nullptr;
    QCheckBox *alwaysOnTopCheck_ = nullptr;
    QLineEdit *screenshotDirEdit_ = nullptr;
    QComboBox *languageCombo_ = nullptr;

    // --- Subtitles -------------------------------------------------------
    QCheckBox *subUseDefaultsCheck_ = nullptr;
    QFontComboBox *subFontCombo_ = nullptr;
    QDoubleSpinBox *subFontSizeSpin_ = nullptr;
    QComboBox *subWeightCombo_ = nullptr;

    QPushButton *subColorButton_ = nullptr;
    QSpinBox *subTransparencySpin_ = nullptr;
    QPushButton *subOutlineColorButton_ = nullptr;
    QDoubleSpinBox *subThicknessSpin_ = nullptr;

    QCheckBox *subShadowCheck_ = nullptr;
    QPushButton *subShadowColorButton_ = nullptr;
    QDoubleSpinBox *subShadowSizeSpin_ = nullptr;
    QSpinBox *subShadowTransparencySpin_ = nullptr;

    QDoubleSpinBox *subVerticalSpin_ = nullptr;
    QDoubleSpinBox *subLetterSpacingSpin_ = nullptr;
    QDoubleSpinBox *subLineSpacingSpin_ = nullptr;
    QComboBox *subAlignCombo_ = nullptr;
    QPushButton *subResetButton_ = nullptr;

    /// Every widget disabled by "use default settings", collected so the
    /// enable/disable pass does not have to name them one by one.
    QList<QWidget *> subtitleControls_;
};
