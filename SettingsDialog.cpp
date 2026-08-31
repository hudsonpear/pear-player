#include "SettingsDialog.h"
#include "SettingsKeys.h"
#include "Theme.h"
#include "Translation.h"
#include "SubtitleStyle.h"
#include "VideoSettings.h"
#include "FileMemory.h"

#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QRadioButton>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QSlider>
#include <QComboBox>
#include <QFontComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QColorDialog>
#include <QLineEdit>
#include <QFileDialog>
#include <QDir>
#include <QDesktopServices>
#include <QUrl>

#include <algorithm>
#include <QAbstractItemView>
#include <QMessageBox>
#include <QSet>
#include <QVector>
#include <QDialogButtonBox>
#include <QLabel>
#include <QSettings>
#include <QPixmap>
#include <QPainter>
#include <QIcon>

namespace {

// A colour is held on the button itself rather than in a member, so the swatch
// buttons need no bespoke widget class (and no extra moc pass).
constexpr char kColorProperty[] = "chosenColor";

QColor buttonColor(const QPushButton *button)
{
    return button->property(kColorProperty).value<QColor>();
}

// Deliberately an icon rather than a stylesheet: this project avoids widget
// stylesheets because they push widgets onto a different paint path, which is
// what makes QOpenGLWidget misbehave elsewhere in the app.
QIcon colorSwatch(const QColor &color, QSize size)
{
    QPixmap swatch(size);
    swatch.fill(Qt::transparent);
    QPainter painter(&swatch);
    // Chequerboard behind the colour so a translucent choice reads as
    // translucent instead of looking like a darker solid colour.
    painter.fillRect(swatch.rect(), QColor(90, 90, 90));
    for (int y = 0; y < size.height(); y += 8) {
        for (int x = 0; x < size.width(); x += 16) {
            painter.fillRect(x + (y / 8 % 2) * 8, y, 8, 8, QColor(130, 130, 130));
        }
    }
    painter.fillRect(swatch.rect(), color);
    painter.setPen(QColor(20, 20, 20));
    painter.drawRect(0, 0, swatch.width() - 1, swatch.height() - 1);
    return QIcon(swatch);
}

void setButtonColor(QPushButton *button, const QColor &color)
{
    button->setProperty(kColorProperty, color);
    button->setIcon(colorSwatch(color, QSize(32, 16)));
    button->setText(color.name(QColor::HexRgb).toUpper());
}

QPushButton *makeColorButton(QWidget *parent, const QString &dialogTitle)
{
    auto *button = new QPushButton(parent);
    button->setFocusPolicy(Qt::StrongFocus);
    QObject::connect(button, &QPushButton::clicked, button, [button, dialogTitle] {
        // Alpha is edited by the separate transparency spin boxes, so the
        // colour picker only deals in RGB and the current alpha is preserved.
        const QColor current = buttonColor(button);
        const QColor picked = QColorDialog::getColor(current, button, dialogTitle);
        if (picked.isValid()) {
            QColor result = picked;
            result.setAlpha(current.alpha());
            setButtonColor(button, result);
        }
    });
    return button;
}

int alphaToTransparencyPercent(int alpha)
{
    return qRound((255 - alpha) * 100.0 / 255.0);
}

int transparencyPercentToAlpha(int percent)
{
    return qRound(255.0 - (percent * 255.0 / 100.0));
}

} // namespace

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
    , originalAccent_(Theme::accent())
{
    setWindowTitle(tr("Settings"));

    auto *mainLayout = new QVBoxLayout(this);

    tabs_ = new QTabWidget(this);
    tabs_->addTab(createGeneralTab(), tr("General"));
    tabs_->addTab(createInterfaceTab(), tr("Interface"));
    subtitlesTabIndex_ = tabs_->addTab(createSubtitlesTab(), tr("Subtitles"));
    tabs_->addTab(createVideoTab(), tr("Video"));
    tabs_->addTab(createHotkeysTab(), tr("Hotkeys"));
    mainLayout->addWidget(tabs_);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, [this] {
        saveToSettings();
        accept();
    });
    // The accent is applied as it is picked, so Cancel has to undo it -- the
    // rest of the dialog stores nothing until OK and needs no such care.
    connect(buttonBox, &QDialogButtonBox::rejected, this, [this] {
        Theme::applyAccent(originalAccent_);
        reject();
    });
    mainLayout->addWidget(buttonBox);

    loadFromSettings();
    updateSubtitleControlsEnabled();

    // Hooked up only after the controls hold their stored values, so filling
    // them in does not fire a preview before the user has changed anything.
    connectSubtitlePreviewSignals();

    // Wide and short: the multi-column tabs above are laid out for this shape,
    // and the height is left to the layout so nothing gets clipped. 820 is the
    // width the two-column tabs want; the three-column Hotkeys list asks for
    // more, and squeezing it into 820 would elide the descriptions instead.
    resize(std::max(820, sizeHint().width()), sizeHint().height());
}

void SettingsDialog::showSubtitlesTab()
{
    if (tabs_ && subtitlesTabIndex_ >= 0) {
        tabs_->setCurrentIndex(subtitlesTabIndex_);
    }
}

QWidget *SettingsDialog::createVideoTab()
{
    auto *page = new QWidget(this);
    auto *columns = new QHBoxLayout(page);
    auto *leftColumn = new QVBoxLayout();
    auto *rightColumn = new QVBoxLayout();
    columns->addLayout(leftColumn, 1);
    columns->addLayout(rightColumn, 1);

    // --- Picture -----------------------------------------------------------
    auto *pictureGroup = new QGroupBox(tr("Picture"), page);
    auto *pictureLayout = new QFormLayout(pictureGroup);

    // Sliders rather than spin boxes: these are judged by eye, so dragging
    // beats typing a number.
    const auto addAdjustRow = [this, pictureGroup, pictureLayout](const QString &label) {
        auto *slider = new QSlider(Qt::Horizontal, pictureGroup);
        slider->setRange(VideoSettings::kMinAdjust, VideoSettings::kMaxAdjust);
        slider->setTickPosition(QSlider::TicksBelow);
        slider->setTickInterval(50);

        auto *value = new QLabel(pictureGroup);
        value->setMinimumWidth(36);
        value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        connect(slider, &QSlider::valueChanged, value, [value](int v) { value->setNum(v); });

        auto *row = new QHBoxLayout();
        row->addWidget(slider, 1);
        row->addWidget(value);
        pictureLayout->addRow(label, row);
        return slider;
    };

    brightnessSlider_ = addAdjustRow(tr("Brightness:"));
    contrastSlider_ = addAdjustRow(tr("Contrast:"));
    saturationSlider_ = addAdjustRow(tr("Saturation:"));
    hueSlider_ = addAdjustRow(tr("Hue:"));
    gammaSlider_ = addAdjustRow(tr("Gamma:"));

    auto *resetPicture = new QPushButton(tr("Reset Picture"), pictureGroup);
    connect(resetPicture, &QPushButton::clicked, this, [this] {
        brightnessSlider_->setValue(0);
        contrastSlider_->setValue(0);
        saturationSlider_->setValue(0);
        hueSlider_->setValue(0);
        gammaSlider_->setValue(0);
    });
    pictureLayout->addRow(resetPicture);

    leftColumn->addWidget(pictureGroup);
    leftColumn->addStretch(1);

    // --- Remembered files ---------------------------------------------------
    auto *memoryGroup = new QGroupBox(tr("Saved File Data"), page);
    auto *memoryLayout = new QVBoxLayout(memoryGroup);

    auto *explanation = new QLabel(
        tr("These are remembered for each video and restored the next time it plays:"
            "\n\n"
            "• Subtitle file, chosen track and sync, and whether subtitles were showing\n"
            "• Flip, mirror and rotation\n"
            "• Brightness, contrast, saturation and hue\n"
            "• Zoom or stretch preset, and picture position"),
        memoryGroup);
    explanation->setWordWrap(true);
    memoryLayout->addWidget(explanation);

    clearFileDataButton_ = new QPushButton(memoryGroup);
    connect(clearFileDataButton_, &QPushButton::clicked, this, [this] {
        const int total = FileMemoryStore::count();
        if (total == 0) {
            return;
        }
        // Irreversible and affects every remembered file, so it is confirmed
        // rather than acted on straight away.
        const auto answer = QMessageBox::question(
            this, tr("Delete Saved File Data"),
            tr("Forget the remembered settings for %n file(s)?", nullptr, total),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer == QMessageBox::Yes) {
            FileMemoryStore::clearAll();
            refreshFileDataButton();
        }
    });
    memoryLayout->addWidget(clearFileDataButton_);

    rightColumn->addWidget(memoryGroup);
    rightColumn->addStretch(1);

    refreshFileDataButton();
    return page;
}

SubtitleStyle SettingsDialog::styleFromControls() const
{
    SubtitleStyle style;
    style.useDefaults = subUseDefaultsCheck_->isChecked();
    style.fontFamily = subFontCombo_->currentFont().family();
    style.fontSize = subFontSizeSpin_->value();
    style.bold = subWeightCombo_->currentData().toBool();

    style.fontColor = buttonColor(subColorButton_);
    style.fontColor.setAlpha(transparencyPercentToAlpha(subTransparencySpin_->value()));
    style.outlineColor = buttonColor(subOutlineColorButton_);
    style.outlineThickness = subThicknessSpin_->value();

    style.shadowEnabled = subShadowCheck_->isChecked();
    style.shadowColor = buttonColor(subShadowColorButton_);
    style.shadowColor.setAlpha(transparencyPercentToAlpha(subShadowTransparencySpin_->value()));
    style.shadowSize = subShadowSizeSpin_->value();

    style.verticalPosition = subVerticalSpin_->value();
    style.letterSpacing = subLetterSpacingSpin_->value();
    style.lineSpacing = subLineSpacingSpin_->value();
    style.align = subAlignCombo_->currentData().toString();

    return style;
}

void SettingsDialog::emitVideoPreview()
{
    emit videoAdjustPreviewed(brightnessSlider_->value(), contrastSlider_->value(),
                               saturationSlider_->value(), hueSlider_->value(),
                               gammaSlider_->value());
}

void SettingsDialog::emitSubtitlePreview()
{
    emit subtitleStylePreviewed(styleFromControls());
}

void SettingsDialog::connectSubtitlePreviewSignals()
{
    connect(subUseDefaultsCheck_, &QCheckBox::toggled, this, &SettingsDialog::emitSubtitlePreview);
    connect(subFontCombo_, &QFontComboBox::currentFontChanged, this, &SettingsDialog::emitSubtitlePreview);
    connect(subFontSizeSpin_, &QDoubleSpinBox::valueChanged, this, &SettingsDialog::emitSubtitlePreview);
    connect(subWeightCombo_, &QComboBox::currentIndexChanged, this, &SettingsDialog::emitSubtitlePreview);
    connect(subTransparencySpin_, &QSpinBox::valueChanged, this, &SettingsDialog::emitSubtitlePreview);
    connect(subThicknessSpin_, &QDoubleSpinBox::valueChanged, this, &SettingsDialog::emitSubtitlePreview);
    connect(subShadowCheck_, &QCheckBox::toggled, this, &SettingsDialog::emitSubtitlePreview);
    connect(subShadowSizeSpin_, &QDoubleSpinBox::valueChanged, this, &SettingsDialog::emitSubtitlePreview);
    connect(subShadowTransparencySpin_, &QSpinBox::valueChanged, this, &SettingsDialog::emitSubtitlePreview);
    connect(subVerticalSpin_, &QDoubleSpinBox::valueChanged, this, &SettingsDialog::emitSubtitlePreview);
    connect(subLetterSpacingSpin_, &QDoubleSpinBox::valueChanged, this, &SettingsDialog::emitSubtitlePreview);
    connect(subLineSpacingSpin_, &QDoubleSpinBox::valueChanged, this, &SettingsDialog::emitSubtitlePreview);
    connect(subAlignCombo_, &QComboBox::currentIndexChanged, this, &SettingsDialog::emitSubtitlePreview);

    // The colour buttons update themselves in a lambda connected at creation
    // time; connecting here means this runs after that one, so the swatch has
    // already taken the new colour by the time the preview is emitted.
    for (QPushButton *button : {subColorButton_, subOutlineColorButton_, subShadowColorButton_}) {
        connect(button, &QPushButton::clicked, this, &SettingsDialog::emitSubtitlePreview);
    }

    connect(brightnessSlider_, &QSlider::valueChanged, this, &SettingsDialog::emitVideoPreview);
    connect(contrastSlider_, &QSlider::valueChanged, this, &SettingsDialog::emitVideoPreview);
    connect(saturationSlider_, &QSlider::valueChanged, this, &SettingsDialog::emitVideoPreview);
    connect(hueSlider_, &QSlider::valueChanged, this, &SettingsDialog::emitVideoPreview);
    connect(gammaSlider_, &QSlider::valueChanged, this, &SettingsDialog::emitVideoPreview);
}

void SettingsDialog::refreshFileDataButton()
{
    const int total = FileMemoryStore::count();
    clearFileDataButton_->setText(tr("Delete Saved File Data (%1)").arg(total));
    clearFileDataButton_->setEnabled(total > 0);
}

QWidget *SettingsDialog::createHotkeysTab()
{
    // A reference list, not an editor: the keys are fixed in code. Kept in
    // step by hand with VideoWidget::keyPressEvent() (the plain keys) and the
    // setShortcut() calls in MainWindow's menu builders (everything with a
    // modifier). Adding a shortcut in either place means adding a row here.
    struct Entry { QString keys; QString action; };
    struct Group { QString title; QVector<Entry> entries; };

    // Three columns, not two: this is the tallest tab in the dialog and the
    // whole dialog is sized by it, so how these groups are distributed decides
    // how tall the Settings window is.
    const QVector<Group> firstColumn = {
        {tr("Playback"), {
            {tr("Space"),           tr("Play / Pause")},
            {tr("Left / Right"),    tr("Seek backward / forward")},
            {tr("Home / End"),      tr("Jump to start / end")},
            {tr("Ctrl+Left / Ctrl+Right"), tr("Previous / next frame")},
            {tr("C / X"),           tr("Speed up / slow down by 0.1")},
            {tr("Z"),               tr("Normal speed")},
            {tr("S"),               tr("Save a screenshot")},
        }},
        {tr("Sound"), {
            {tr("Up / Down"),       tr("Volume up / down")},
            {tr("M"),               tr("Mute")},
        }},
        {tr("Window"), {
            {tr("F, Enter"),        tr("Toggle fullscreen")},
            {tr("F11"),             tr("Toggle fullscreen")},
            {tr("Esc"),             tr("Leave fullscreen")},
        }},
    };

    const QVector<Group> secondColumn = {
        {tr("Subtitles"), {
            {tr("Alt+H"),           tr("Show / hide subtitles")},
            {tr("Alt+Up / Alt+Down"), tr("Move subtitles up / down")},
            {tr("Alt+Home"),        tr("Reset subtitle position")},
            {tr("Alt+B"),           tr("Bold")},
            {tr("Alt+PgUp / Alt+PgDn"), tr("Increase / decrease font size")},
            {tr("."),               tr("Sync 0.5 seconds faster")},
            {tr(","),               tr("Sync 0.5 seconds slower")},
            {tr("/"),               tr("Reset sync")},
            {tr("Alt+S"),           tr("Save the current sync for this file")},
        }},
        {tr("Mouse"), {
            {tr("Double-click"),    tr("Fullscreen, or play/pause (Interface tab)")},
            {tr("Click"),           tr("Play / pause, when enabled")},
            {tr("Wheel"),           tr("Volume up / down")},
        }},
    };

    const QVector<Group> thirdColumn = {
        {tr("File"), {
            {tr("Ctrl+O"),          tr("Open file")},
            {tr("Ctrl+U"),          tr("Open URL")},
            {tr("Ctrl+D"),          tr("Download from URL")},
            {tr("Delete"),          tr("Send the playing file to the Recycle Bin")},
            {tr("Ctrl+Q"),          tr("Quit")},
        }},
        {tr("Picture"), {
            {tr("Numpad 0"),        tr("Cycle video frame mode")},
            {tr("Numpad 8 / 2"),    tr("Move picture up / down")},
            {tr("Numpad 4 / 6"),    tr("Move picture left / right")},
            {tr("Numpad 5"),        tr("Centre picture")},
        }},
    };

    const auto buildColumn = [](const QVector<Group> &groups) {
        auto *column = new QVBoxLayout();
        for (const Group &group : groups) {
            auto *box = new QGroupBox(group.title);
            auto *form = new QFormLayout(box);
            for (const Entry &entry : group.entries) {
                auto *keys = new QLabel(entry.keys, box);
                QFont keyFont = keys->font();
                keyFont.setBold(true);
                keys->setFont(keyFont);
                // Selectable so a key can be copied out of the list.
                keys->setTextInteractionFlags(Qt::TextSelectableByMouse);
                form->addRow(keys, new QLabel(entry.action, box));
            }
            column->addWidget(box);
        }
        column->addStretch(1);
        return column;
    };

    auto *page = new QWidget(this);
    auto *columns = new QHBoxLayout(page);
    columns->addLayout(buildColumn(firstColumn));
    columns->addLayout(buildColumn(secondColumn));
    columns->addLayout(buildColumn(thirdColumn));
    return page;
}

QWidget *SettingsDialog::createGeneralTab()
{
    auto *page = new QWidget(this);
    // Two columns rather than one tall stack: the groups are all short, so
    // side by side keeps the dialog from running off the bottom of smaller
    // screens.
    auto *columns = new QHBoxLayout(page);
    auto *leftColumn = new QVBoxLayout();
    auto *rightColumn = new QVBoxLayout();
    columns->addLayout(leftColumn, 1);
    columns->addLayout(rightColumn, 1);

    // --- Seek jump ----------------------------------------------------
    auto *seekGroup = new QGroupBox(tr("Jump Forward/Backward (Left/Right Arrow)"), page);
    auto *seekLayout = new QVBoxLayout(seekGroup);

    auto *fixedRow = new QHBoxLayout();
    seekFixedRadio_ = new QRadioButton(tr("Fixed amount:"), seekGroup);
    seekSecondsSpin_ = new QDoubleSpinBox(seekGroup);
    seekSecondsSpin_->setRange(0.1, 600.0);
    seekSecondsSpin_->setDecimals(1);
    seekSecondsSpin_->setSuffix(tr(" sec"));
    fixedRow->addWidget(seekFixedRadio_);
    fixedRow->addWidget(seekSecondsSpin_);
    fixedRow->addStretch(1);
    seekLayout->addLayout(fixedRow);

    auto *percentRow = new QHBoxLayout();
    seekPercentRadio_ = new QRadioButton(tr("Percentage of duration:"), seekGroup);
    seekPercentSpin_ = new QDoubleSpinBox(seekGroup);
    seekPercentSpin_->setRange(0.01, 100.0);
    seekPercentSpin_->setDecimals(2);
    seekPercentSpin_->setSuffix(tr(" %"));
    percentRow->addWidget(seekPercentRadio_);
    percentRow->addWidget(seekPercentSpin_);
    percentRow->addStretch(1);
    seekLayout->addLayout(percentRow);

    auto *minRow = new QHBoxLayout();
    minRow->addSpacing(20);
    minRow->addWidget(new QLabel(tr("Minimum jump:"), seekGroup));
    seekMinSecondsSpin_ = new QDoubleSpinBox(seekGroup);
    seekMinSecondsSpin_->setRange(0.0, 60.0);
    seekMinSecondsSpin_->setDecimals(1);
    seekMinSecondsSpin_->setSuffix(tr(" sec"));
    minRow->addWidget(seekMinSecondsSpin_);
    minRow->addStretch(1);
    seekLayout->addLayout(minRow);

    leftColumn->addWidget(seekGroup);

    // --- Volume ---------------------------------------------------------
    auto *volumeGroup = new QGroupBox(tr("Volume"), page);
    auto *volumeLayout = new QFormLayout(volumeGroup);
    maxVolumeCombo_ = new QComboBox(volumeGroup);
    for (int pct = 100; pct <= 1000; pct += 100) {
        maxVolumeCombo_->addItem(tr("%1%").arg(pct), pct);
    }
    volumeLayout->addRow(tr("Max volume:"), maxVolumeCombo_);
    leftColumn->addWidget(volumeGroup);

    // --- Playback --------------------------------------------------------
    auto *playbackGroup = new QGroupBox(tr("Playback"), page);
    auto *playbackLayout = new QVBoxLayout(playbackGroup);
    hwDecCheck_ = new QCheckBox(tr("Use hardware decoding (restart required)"), playbackGroup);
    hwDecCheck_->setToolTip(tr("Faster, but can cause corrupted frames (colored horizontal lines)\non some GPU/driver combinations. Leave off if you see artifacts."));
    playbackLayout->addWidget(hwDecCheck_);

    // Applies to everything played, so it belongs here rather than beside the
    // per-video data on the Video tab.
    rememberPositionCheck_ = new QCheckBox(tr("Resume files where they were left off"), playbackGroup);
    rememberPositionCheck_->setToolTip(
        tr("Positions within %1 seconds of the start or end are not stored,\n"
            "so a file watched to the end starts again from the beginning.")
            .arg(SettingsKeys::kResumeMinSeconds, 0, 'f', 0));
    playbackLayout->addWidget(rememberPositionCheck_);

    autoAdvanceCheck_ = new QCheckBox(tr("Start the next playlist entry when a file finishes"), playbackGroup);
    autoAdvanceCheck_->setToolTip(
        tr("Off means playback stops at the end of each file.\n"
            "Loop File overrides this, and Loop Playlist wraps around at the end."));
    playbackLayout->addWidget(autoAdvanceCheck_);

    openMaximizedCheck_ = new QCheckBox(tr("Always open files maximized"), playbackGroup);
    openMaximizedCheck_->setToolTip(
        tr("Maximizes the window each time a file starts playing.\n"
            "Leaves a fullscreen window alone."));
    playbackLayout->addWidget(openMaximizedCheck_);

    fitWindowCheck_ = new QCheckBox(tr("Resize the window to fit the video"), playbackGroup);
    fitWindowCheck_->setToolTip(
        tr("Opening a file resizes the window so the picture is shown at its own\n"
            "size -- a tall video makes a tall window, a wide one a wide window.\n"
            "Videos larger than the screen are scaled down to fit.\n"
            "Ignored while fullscreen or maximized."));
    playbackLayout->addWidget(fitWindowCheck_);
    // A maximized window has no size of its own to set, so the two cannot both
    // apply. Greyed out rather than silently ignored, so it is clear which one
    // is in charge.
    connect(openMaximizedCheck_, &QCheckBox::toggled, fitWindowCheck_, &QWidget::setDisabled);

    chapterMarkersCheck_ = new QCheckBox(tr("Display chapter markers on the timeline"), playbackGroup);
    chapterMarkersCheck_->setToolTip(
        tr("Draws a tick where each chapter starts.\n"
            "Files without chapters are unaffected."));
    playbackLayout->addWidget(chapterMarkersCheck_);
    // Its own column: Playback carries six checkboxes, about as much as Seek
    // and Volume together.
    rightColumn->addWidget(playbackGroup);

    leftColumn->addStretch(1);
    rightColumn->addStretch(1);
    return page;
}

QWidget *SettingsDialog::createInterfaceTab()
{
    // Split out of the General tab, which had grown tall enough to push the
    // dialog past the bottom of smaller screens. Everything here is about how
    // the app looks and responds rather than how it plays.
    auto *page = new QWidget(this);
    auto *columns = new QHBoxLayout(page);
    auto *leftColumn = new QVBoxLayout();
    auto *rightColumn = new QVBoxLayout();
    columns->addLayout(leftColumn);
    columns->addLayout(rightColumn);

    // --- Click behavior ---------------------------------------------
    auto *clickGroup = new QGroupBox(tr("Click Behavior"), page);
    auto *clickLayout = new QVBoxLayout(clickGroup);
    clickToPauseCheck_ = new QCheckBox(tr("Click screen to pause/unpause"), clickGroup);
    clickLayout->addWidget(clickToPauseCheck_);
    leftColumn->addWidget(clickGroup);

    // --- Double-click action -------------------------------------------
    auto *dblGroup = new QGroupBox(tr("Double-Click Action"), page);
    auto *dblLayout = new QVBoxLayout(dblGroup);
    dblFullscreenRadio_ = new QRadioButton(tr("Toggle fullscreen"), dblGroup);
    dblPlayPauseRadio_ = new QRadioButton(tr("Toggle play/pause"), dblGroup);
    dblLayout->addWidget(dblFullscreenRadio_);
    dblLayout->addWidget(dblPlayPauseRadio_);
    leftColumn->addWidget(dblGroup);

    // --- Window ------------------------------------------------------------
    auto *startupGroup = new QGroupBox(tr("Window"), page);
    auto *startupLayout = new QVBoxLayout(startupGroup);
    startupCenterRadio_ = new QRadioButton(tr("Open centered on screen"), startupGroup);
    startupTopLeftRadio_ = new QRadioButton(tr("Open at top-left of screen"), startupGroup);
    startupLayout->addWidget(startupCenterRadio_);
    startupLayout->addWidget(startupTopLeftRadio_);

    rememberGeometryCheck_ = new QCheckBox(tr("Reopen at the last size and position"), startupGroup);
    rememberGeometryCheck_->setToolTip(
        tr("Remembers where the window was when it was closed.\n"
            "Replaces the two choices above while it is on."));
    startupLayout->addWidget(rememberGeometryCheck_);
    // The remembered geometry decides both, so the position radios have
    // nothing left to say while it is on.
    connect(rememberGeometryCheck_, &QCheckBox::toggled, this, [this](bool on) {
        startupCenterRadio_->setDisabled(on);
        startupTopLeftRadio_->setDisabled(on);
    });

    alwaysOnTopCheck_ = new QCheckBox(tr("Keep the window above other windows"), startupGroup);
    startupLayout->addWidget(alwaysOnTopCheck_);
    leftColumn->addWidget(startupGroup);

    // --- Language ----------------------------------------------------------
    auto *languageGroup = new QGroupBox(tr("Language"), page);
    auto *languageLayout = new QVBoxLayout(languageGroup);

    languageCombo_ = new QComboBox(languageGroup);
    // English first and always present: it is the text compiled into the app,
    // not a file that could be missing.
    languageCombo_->addItem(tr("English"), QString());
    for (const Translation::Language &language : Translation::availableLanguages()) {
        languageCombo_->addItem(language.name, language.code);
    }
    languageLayout->addWidget(languageCombo_);

    auto *languageHint = new QLabel(
        tr("Applied as soon as you press OK.\n"
            "To add a language, put a .json file in the translations folder."),
        languageGroup);
    languageHint->setWordWrap(true);
    languageLayout->addWidget(languageHint);

    auto *languageOpen = new QPushButton(tr("Open Translations Folder"), languageGroup);
    connect(languageOpen, &QPushButton::clicked, this, [this] {
        const QString directory = Translation::translationsDirectory();
        if (!QDir().mkpath(directory)) {
            QMessageBox::warning(this, tr("Translations"),
                                  tr("Cannot create %1").arg(QDir::toNativeSeparators(directory)));
            return;
        }
        QDesktopServices::openUrl(QUrl::fromLocalFile(directory));
    });
    auto *languageRow = new QHBoxLayout();
    languageRow->addWidget(languageOpen);
    languageRow->addStretch(1);
    languageLayout->addLayout(languageRow);
    rightColumn->addWidget(languageGroup);

    // --- Screenshots -------------------------------------------------------
    auto *shotGroup = new QGroupBox(tr("Screenshots"), page);
    auto *shotLayout = new QVBoxLayout(shotGroup);
    shotLayout->addWidget(new QLabel(tr("The S key saves the current frame here."), shotGroup));

    auto *shotRow = new QHBoxLayout();
    screenshotDirEdit_ = new QLineEdit(shotGroup);
    // Readable and copyable, but typed paths are not: a folder that does not
    // exist would only fail later, at the moment a frame was being saved.
    screenshotDirEdit_->setReadOnly(true);
    shotRow->addWidget(screenshotDirEdit_, 1);

    auto *browse = new QPushButton(tr("Browse..."), shotGroup);
    connect(browse, &QPushButton::clicked, this, [this] {
        const QString chosen = QFileDialog::getExistingDirectory(
            this, tr("Screenshot Folder"), screenshotDirEdit_->text());
        if (!chosen.isEmpty()) {
            screenshotDirEdit_->setText(QDir::toNativeSeparators(chosen));
        }
    });
    shotRow->addWidget(browse);

    auto *shotDefault = new QPushButton(tr("Default"), shotGroup);
    connect(shotDefault, &QPushButton::clicked, this, [this] {
        // Cleared rather than set to the current default path, so the folder
        // keeps following the user's Pictures location if that ever moves.
        screenshotDirEdit_->clear();
        screenshotDirEdit_->setPlaceholderText(
            QDir::toNativeSeparators(SettingsKeys::screenshotDirectory()));
    });
    // Second row: the path and Browse... get the width, and the two actions
    // that operate on the folder sit together underneath.
    shotLayout->addLayout(shotRow);
    auto *shotActionRow = new QHBoxLayout();
    shotActionRow->addWidget(shotDefault);

    auto *shotOpen = new QPushButton(tr("Open Folder"), shotGroup);
    connect(shotOpen, &QPushButton::clicked, this, [this] {
        // Whatever is actually in force: the field when a folder was chosen,
        // the resolved default when it is empty.
        const QString directory = screenshotDirEdit_->text().isEmpty()
            ? SettingsKeys::screenshotDirectory()
            : screenshotDirEdit_->text();
        // Created first, since it is only made on the first screenshot --
        // otherwise Explorer would refuse to open a folder that is not there
        // yet, which looks like the button is broken.
        if (!QDir().mkpath(directory)) {
            QMessageBox::warning(this, tr("Screenshots"),
                                  tr("Cannot create %1").arg(QDir::toNativeSeparators(directory)));
            return;
        }
        QDesktopServices::openUrl(QUrl::fromLocalFile(directory));
    });
    shotActionRow->addWidget(shotOpen);
    shotActionRow->addStretch(1);
    shotLayout->addLayout(shotActionRow);
    rightColumn->addWidget(shotGroup);

    // --- Theme -----------------------------------------------------------
    auto *themeGroup = new QGroupBox(tr("Theme"), page);
    auto *themeLayout = new QVBoxLayout(themeGroup);
    auto *themeHint = new QLabel(tr("One colour, used as the base for the "
                                     "sliders, highlights and buttons."), themeGroup);
    themeHint->setWordWrap(true);
    themeLayout->addWidget(themeHint);

    // Presets as a row of swatches. Chosen over a combo box so the colours
    // themselves are the choice rather than a list of names.
    auto *presetRow = new QHBoxLayout();
    presetRow->setSpacing(4);
    for (const Theme::Preset &preset : Theme::presets()) {
        auto *swatch = new QPushButton(themeGroup);
        swatch->setToolTip(preset.name);
        swatch->setFixedSize(30, 24);
        swatch->setIconSize(QSize(22, 14));
        swatch->setFocusPolicy(Qt::StrongFocus);
        swatch->setIcon(colorSwatch(preset.color, QSize(22, 14)));
        const QColor color = preset.color;
        connect(swatch, &QPushButton::clicked, this, [this, color] { previewAccent(color); });
        presetRow->addWidget(swatch);
    }
    presetRow->addStretch(1);
    themeLayout->addLayout(presetRow);

    auto *customRow = new QHBoxLayout();
    accentButton_ = new QPushButton(themeGroup);
    accentButton_->setFocusPolicy(Qt::StrongFocus);
    connect(accentButton_, &QPushButton::clicked, this, [this] {
        const QColor picked = QColorDialog::getColor(Theme::accent(), this, tr("Accent Colour"));
        if (picked.isValid()) {
            previewAccent(picked);
        }
    });
    customRow->addWidget(new QLabel(tr("Custom:"), themeGroup));
    customRow->addWidget(accentButton_);

    auto *resetAccent = new QPushButton(tr("Default"), themeGroup);
    connect(resetAccent, &QPushButton::clicked, this,
             [this] { previewAccent(Theme::kDefaultAccent); });
    customRow->addWidget(resetAccent);
    customRow->addStretch(1);
    themeLayout->addLayout(customRow);
    rightColumn->addWidget(themeGroup);

    leftColumn->addStretch(1);
    rightColumn->addStretch(1);
    return page;
}

void SettingsDialog::previewAccent(const QColor &color)
{
    // Applied at once so the choice can be judged against the real app rather
    // than a swatch. Stored only on OK; Cancel puts the old colour back.
    Theme::applyAccent(color);
    accentButton_->setIcon(colorSwatch(color, QSize(32, 16)));
    accentButton_->setText(color.name(QColor::HexRgb).toUpper());
}

QWidget *SettingsDialog::createSubtitlesTab()
{
    auto *page = new QWidget(this);
    auto *pageLayout = new QVBoxLayout(page);

    subUseDefaultsCheck_ = new QCheckBox(tr("Use default subtitle settings"), page);
    subUseDefaultsCheck_->setToolTip(tr("Leaves subtitle appearance to the player's own defaults.\n"
                                          "Uncheck to use the settings below."));
    connect(subUseDefaultsCheck_, &QCheckBox::toggled, this, &SettingsDialog::updateSubtitleControlsEnabled);

    subResetButton_ = new QPushButton(tr("Reset to Defaults"), page);
    subResetButton_->setToolTip(tr("Puts every subtitle setting back to the player's defaults.\n"
                                     "Nothing is stored until you press OK, so Cancel still undoes it."));
    // Only rewrites the controls; saveToSettings() on OK is still what makes
    // it stick, so a mis-click is undone by pressing Cancel.
    connect(subResetButton_, &QPushButton::clicked, this, [this] {
        applySubtitleStyleToControls(SubtitleStyle::mpvDefaults());
    });

    auto *topRow = new QHBoxLayout();
    topRow->addWidget(subUseDefaultsCheck_);
    topRow->addStretch(1);
    topRow->addWidget(subResetButton_);
    pageLayout->addLayout(topRow);

    // Two columns, no scroll area: side by side the four groups fit on screen
    // outright, which reads better than scrolling a tall single column.
    auto *columns = new QHBoxLayout();
    auto *leftColumn = new QVBoxLayout();
    auto *rightColumn = new QVBoxLayout();
    columns->addLayout(leftColumn, 1);
    columns->addLayout(rightColumn, 1);

    // --- Font ------------------------------------------------------------
    auto *fontGroup = new QGroupBox(tr("Font"), page);
    auto *fontLayout = new QFormLayout(fontGroup);

    subFontCombo_ = new QFontComboBox(fontGroup);
    fontLayout->addRow(tr("Font:"), subFontCombo_);

    subFontSizeSpin_ = new QDoubleSpinBox(fontGroup);
    subFontSizeSpin_->setRange(4.0, 400.0);
    subFontSizeSpin_->setDecimals(1);
    fontLayout->addRow(tr("Size:"), subFontSizeSpin_);

    subWeightCombo_ = new QComboBox(fontGroup);
    subWeightCombo_->addItem(tr("Normal"), false);
    subWeightCombo_->addItem(tr("Bold"), true);
    fontLayout->addRow(tr("Weight:"), subWeightCombo_);

    leftColumn->addWidget(fontGroup);

    // --- Colors -----------------------------------------------------------
    auto *colorGroup = new QGroupBox(tr("Colors"), page);
    auto *colorLayout = new QFormLayout(colorGroup);

    subColorButton_ = makeColorButton(colorGroup, tr("Subtitle Color"));
    colorLayout->addRow(tr("Font color:"), subColorButton_);

    subTransparencySpin_ = new QSpinBox(colorGroup);
    subTransparencySpin_->setRange(0, 100);
    subTransparencySpin_->setSuffix(tr(" %"));
    colorLayout->addRow(tr("Transparency:"), subTransparencySpin_);

    subOutlineColorButton_ = makeColorButton(colorGroup, tr("Outline Color"));
    colorLayout->addRow(tr("Outline color:"), subOutlineColorButton_);

    subThicknessSpin_ = new QDoubleSpinBox(colorGroup);
    subThicknessSpin_->setRange(0.0, 10.0);
    subThicknessSpin_->setDecimals(2);
    subThicknessSpin_->setSingleStep(0.25);
    subThicknessSpin_->setToolTip(tr("Outline width. 0 removes the outline."));
    colorLayout->addRow(tr("Thickness:"), subThicknessSpin_);

    leftColumn->addWidget(colorGroup);

    // --- Shadow ------------------------------------------------------------
    auto *shadowGroup = new QGroupBox(tr("Shadow"), page);
    auto *shadowLayout = new QFormLayout(shadowGroup);

    subShadowCheck_ = new QCheckBox(tr("Draw a shadow"), shadowGroup);
    shadowLayout->addRow(subShadowCheck_);

    subShadowColorButton_ = makeColorButton(shadowGroup, tr("Shadow Color"));
    shadowLayout->addRow(tr("Shadow color:"), subShadowColorButton_);

    subShadowSizeSpin_ = new QDoubleSpinBox(shadowGroup);
    subShadowSizeSpin_->setRange(0.0, 10.0);
    subShadowSizeSpin_->setDecimals(2);
    subShadowSizeSpin_->setSingleStep(0.25);
    shadowLayout->addRow(tr("Shadow size:"), subShadowSizeSpin_);

    subShadowTransparencySpin_ = new QSpinBox(shadowGroup);
    subShadowTransparencySpin_->setRange(0, 100);
    subShadowTransparencySpin_->setSuffix(tr(" %"));
    shadowLayout->addRow(tr("Shadow transparency:"), subShadowTransparencySpin_);

    rightColumn->addWidget(shadowGroup);

    // --- Position ----------------------------------------------------------
    auto *positionGroup = new QGroupBox(tr("Position"), page);
    auto *positionLayout = new QFormLayout(positionGroup);

    // No horizontal control: mpv writes its single sub-margin-x value into
    // both margins, so centred subtitles cannot be shifted sideways at all.
    subVerticalSpin_ = new QDoubleSpinBox(positionGroup);
    subVerticalSpin_->setRange(0.0, 150.0);
    subVerticalSpin_->setDecimals(1);
    subVerticalSpin_->setToolTip(tr("0 is the top of the picture, 100 the usual bottom line."));
    positionLayout->addRow(tr("Vertical:"), subVerticalSpin_);

    subLetterSpacingSpin_ = new QDoubleSpinBox(positionGroup);
    subLetterSpacingSpin_->setRange(-10.0, 50.0);
    subLetterSpacingSpin_->setDecimals(2);
    subLetterSpacingSpin_->setSingleStep(0.5);
    positionLayout->addRow(tr("Space between letters:"), subLetterSpacingSpin_);

    subLineSpacingSpin_ = new QDoubleSpinBox(positionGroup);
    subLineSpacingSpin_->setRange(-10.0, 50.0);
    subLineSpacingSpin_->setDecimals(2);
    subLineSpacingSpin_->setSingleStep(0.5);
    positionLayout->addRow(tr("Space between rows:"), subLineSpacingSpin_);

    subAlignCombo_ = new QComboBox(positionGroup);
    subAlignCombo_->addItem(tr("Left"), QStringLiteral("left"));
    subAlignCombo_->addItem(tr("Center"), QStringLiteral("center"));
    subAlignCombo_->addItem(tr("Right"), QStringLiteral("right"));
    positionLayout->addRow(tr("Align:"), subAlignCombo_);

    rightColumn->addWidget(positionGroup);

    leftColumn->addStretch(1);
    rightColumn->addStretch(1);
    pageLayout->addLayout(columns);

    subtitleControls_ = {fontGroup, colorGroup, shadowGroup, positionGroup};
    return page;
}

void SettingsDialog::updateSubtitleControlsEnabled()
{
    const bool custom = !subUseDefaultsCheck_->isChecked();
    for (QWidget *widget : subtitleControls_) {
        widget->setEnabled(custom);
    }
}

void SettingsDialog::loadFromSettings()
{
    QSettings settings;

    const QString seekMode = settings.value(SettingsKeys::kSeekMode, QStringLiteral("seconds")).toString();
    seekFixedRadio_->setChecked(seekMode != QStringLiteral("percentage"));
    seekPercentRadio_->setChecked(seekMode == QStringLiteral("percentage"));
    seekSecondsSpin_->setValue(settings.value(SettingsKeys::kSeekSeconds, SettingsKeys::kDefaultSeekSeconds).toDouble());
    seekPercentSpin_->setValue(settings.value(SettingsKeys::kSeekPercentage, SettingsKeys::kDefaultSeekPercentage).toDouble());
    seekMinSecondsSpin_->setValue(settings.value(SettingsKeys::kSeekMinSeconds, SettingsKeys::kDefaultSeekMinSeconds).toDouble());

    const int maxVolume = settings.value(SettingsKeys::kMaxVolume, SettingsKeys::kDefaultMaxVolume).toInt();
    const int index = maxVolumeCombo_->findData(maxVolume);
    maxVolumeCombo_->setCurrentIndex(index >= 0 ? index : 0);

    clickToPauseCheck_->setChecked(settings.value(SettingsKeys::kClickToPause, SettingsKeys::kDefaultClickToPause).toBool());
    hwDecCheck_->setChecked(settings.value(SettingsKeys::kHwDecEnabled, SettingsKeys::kDefaultHwDecEnabled).toBool());

    const QString dblAction = settings.value(SettingsKeys::kDoubleClickAction, QStringLiteral("fullscreen")).toString();
    dblFullscreenRadio_->setChecked(dblAction != QStringLiteral("playpause"));
    dblPlayPauseRadio_->setChecked(dblAction == QStringLiteral("playpause"));

    const QString startupPos = settings.value(SettingsKeys::kStartupPosition, QStringLiteral("center")).toString();
    startupCenterRadio_->setChecked(startupPos != QStringLiteral("topleft"));
    startupTopLeftRadio_->setChecked(startupPos == QStringLiteral("topleft"));

    applySubtitleStyleToControls(SubtitleStyle::load());

    const VideoSettings::Adjust adjust = VideoSettings::loadAdjust();
    brightnessSlider_->setValue(adjust.brightness);
    contrastSlider_->setValue(adjust.contrast);
    saturationSlider_->setValue(adjust.saturation);
    hueSlider_->setValue(adjust.hue);
    gammaSlider_->setValue(adjust.gamma);

    rememberPositionCheck_->setChecked(
        settings.value(SettingsKeys::kRememberPosition, SettingsKeys::kDefaultRememberPosition).toBool());
    autoAdvanceCheck_->setChecked(
        settings.value(SettingsKeys::kAutoAdvance, SettingsKeys::kDefaultAutoAdvance).toBool());
    chapterMarkersCheck_->setChecked(
        settings.value(SettingsKeys::kShowChapterMarkers,
                        SettingsKeys::kDefaultShowChapterMarkers).toBool());
    fitWindowCheck_->setChecked(
        settings.value(SettingsKeys::kFitWindowToVideo,
                        SettingsKeys::kDefaultFitWindowToVideo).toBool());

    openMaximizedCheck_->setChecked(
        settings.value(SettingsKeys::kOpenMaximized,
                        SettingsKeys::kDefaultOpenMaximized).toBool());
    fitWindowCheck_->setDisabled(openMaximizedCheck_->isChecked());

    rememberGeometryCheck_->setChecked(
        settings.value(SettingsKeys::kRememberGeometry,
                        SettingsKeys::kDefaultRememberGeometry).toBool());
    startupCenterRadio_->setDisabled(rememberGeometryCheck_->isChecked());
    startupTopLeftRadio_->setDisabled(rememberGeometryCheck_->isChecked());
    alwaysOnTopCheck_->setChecked(
        settings.value(SettingsKeys::kAlwaysOnTop, SettingsKeys::kDefaultAlwaysOnTop).toBool());

    // Falls back to English when the stored language's file has since been
    // removed, which is also what the app itself does on startup.
    const int languageIndex = languageCombo_->findData(Translation::currentLanguageCode());
    languageCombo_->setCurrentIndex(languageIndex >= 0 ? languageIndex : 0);

    const QString storedShotDir = settings.value(SettingsKeys::kScreenshotDir).toString();
    screenshotDirEdit_->setText(storedShotDir.isEmpty()
                                     ? QString()
                                     : QDir::toNativeSeparators(storedShotDir));
    // Shown greyed when nothing is stored, so the default is visible without
    // being mistaken for a choice.
    screenshotDirEdit_->setPlaceholderText(
        QDir::toNativeSeparators(SettingsKeys::screenshotDirectory()));

    accentButton_->setIcon(colorSwatch(Theme::accent(), QSize(32, 16)));
    accentButton_->setText(Theme::accent().name(QColor::HexRgb).toUpper());
}

void SettingsDialog::applySubtitleStyleToControls(const SubtitleStyle &style)
{
    subUseDefaultsCheck_->setChecked(style.useDefaults);
    subFontCombo_->setCurrentFont(QFont(style.fontFamily));
    subFontSizeSpin_->setValue(style.fontSize);
    subWeightCombo_->setCurrentIndex(style.bold ? 1 : 0);

    setButtonColor(subColorButton_, style.fontColor);
    subTransparencySpin_->setValue(alphaToTransparencyPercent(style.fontColor.alpha()));
    setButtonColor(subOutlineColorButton_, style.outlineColor);
    subThicknessSpin_->setValue(style.outlineThickness);

    subShadowCheck_->setChecked(style.shadowEnabled);
    setButtonColor(subShadowColorButton_, style.shadowColor);
    subShadowSizeSpin_->setValue(style.shadowSize);
    subShadowTransparencySpin_->setValue(alphaToTransparencyPercent(style.shadowColor.alpha()));

    subVerticalSpin_->setValue(style.verticalPosition);
    subLetterSpacingSpin_->setValue(style.letterSpacing);
    subLineSpacingSpin_->setValue(style.lineSpacing);

    const int alignIndex = subAlignCombo_->findData(style.align);
    subAlignCombo_->setCurrentIndex(alignIndex >= 0 ? alignIndex : 1);

    updateSubtitleControlsEnabled();
}

void SettingsDialog::saveToSettings()
{
    QSettings settings;
    settings.setValue(SettingsKeys::kSeekMode, seekPercentRadio_->isChecked() ? QStringLiteral("percentage") : QStringLiteral("seconds"));
    settings.setValue(SettingsKeys::kSeekSeconds, seekSecondsSpin_->value());
    settings.setValue(SettingsKeys::kSeekPercentage, seekPercentSpin_->value());
    settings.setValue(SettingsKeys::kSeekMinSeconds, seekMinSecondsSpin_->value());
    settings.setValue(SettingsKeys::kMaxVolume, maxVolumeCombo_->currentData().toInt());
    settings.setValue(SettingsKeys::kClickToPause, clickToPauseCheck_->isChecked());
    settings.setValue(SettingsKeys::kHwDecEnabled, hwDecCheck_->isChecked());
    settings.setValue(SettingsKeys::kDoubleClickAction, dblPlayPauseRadio_->isChecked() ? QStringLiteral("playpause") : QStringLiteral("fullscreen"));
    settings.setValue(SettingsKeys::kStartupPosition, startupTopLeftRadio_->isChecked() ? QStringLiteral("topleft") : QStringLiteral("center"));

    styleFromControls().save();

    VideoSettings::Adjust adjust;
    adjust.brightness = brightnessSlider_->value();
    adjust.contrast = contrastSlider_->value();
    adjust.saturation = saturationSlider_->value();
    adjust.hue = hueSlider_->value();
    adjust.gamma = gammaSlider_->value();
    VideoSettings::saveAdjust(adjust);

    settings.setValue(SettingsKeys::kRememberPosition, rememberPositionCheck_->isChecked());
    settings.setValue(SettingsKeys::kAutoAdvance, autoAdvanceCheck_->isChecked());
    settings.setValue(SettingsKeys::kShowChapterMarkers, chapterMarkersCheck_->isChecked());
    settings.setValue(SettingsKeys::kFitWindowToVideo, fitWindowCheck_->isChecked());
    settings.setValue(SettingsKeys::kOpenMaximized, openMaximizedCheck_->isChecked());
    settings.setValue(SettingsKeys::kRememberGeometry, rememberGeometryCheck_->isChecked());
    settings.setValue(SettingsKeys::kAlwaysOnTop, alwaysOnTopCheck_->isChecked());
    settings.setValue(SettingsKeys::kScreenshotDir, screenshotDirEdit_->text());
    Translation::setLanguageCode(languageCombo_->currentData().toString());
    // Already in effect; this is what makes it survive a restart.
    Theme::saveAccent(Theme::accent());
}
