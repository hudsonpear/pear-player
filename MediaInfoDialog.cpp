#include "MediaInfoDialog.h"
#include "MpvPlayer.h"

#include <QVBoxLayout>
#include <QTreeWidget>
#include <QHeaderView>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QLocale>
#include <QDir>

namespace {

/// Adds a row under a section, skipping values mpv reports as empty so the
/// list shows only what the file actually has.
void addRow(QTreeWidgetItem *section, const QString &label, const QString &value)
{
    if (value.isEmpty()) {
        return;
    }
    auto *row = new QTreeWidgetItem(section);
    row->setText(0, label);
    row->setText(1, value);
}

QString formatDuration(const QString &seconds)
{
    bool ok = false;
    const double total = seconds.toDouble(&ok);
    if (!ok || total <= 0.0) {
        return {};
    }
    const int whole = static_cast<int>(total);
    return QStringLiteral("%1:%2:%3")
        .arg(whole / 3600, 2, 10, QLatin1Char('0'))
        .arg((whole % 3600) / 60, 2, 10, QLatin1Char('0'))
        .arg(whole % 60, 2, 10, QLatin1Char('0'));
}

} // namespace

MediaInfoDialog::MediaInfoDialog(const MpvPlayer &player, const QString &mediaPath, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Media Information"));

    auto *layout = new QVBoxLayout(this);

    tree_ = new QTreeWidget(this);
    tree_->setColumnCount(2);
    tree_->setHeaderLabels({tr("Property"), tr("Value")});
    tree_->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    tree_->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    tree_->setRootIsDecorated(false);
    tree_->setIndentation(12);
    layout->addWidget(tree_);

    // --- File ---------------------------------------------------------------
    auto *fileSection = new QTreeWidgetItem(tree_, {tr("File")});
    fileSection->setFirstColumnSpanned(true);
    const QFileInfo info(mediaPath);
    if (info.isFile()) {
        addRow(fileSection, tr("Name"), info.fileName());
        addRow(fileSection, tr("Folder"), QDir::toNativeSeparators(info.absolutePath()));
        addRow(fileSection, tr("Size"), QLocale().formattedDataSize(info.size()));
    } else {
        // A URL has no file on disk, so show the address itself instead.
        addRow(fileSection, tr("Source"), mediaPath);
    }
    addRow(fileSection, tr("Format"), player.propertyText(QStringLiteral("file-format")));
    addRow(fileSection, tr("Duration"), formatDuration(player.propertyText(QStringLiteral("duration"))));

    // --- Video ---------------------------------------------------------------
    const QString videoCodec = player.propertyText(QStringLiteral("video-codec"));
    if (!videoCodec.isEmpty()) {
        auto *videoSection = new QTreeWidgetItem(tree_, {tr("Video")});
        videoSection->setFirstColumnSpanned(true);
        addRow(videoSection, tr("Codec"), videoCodec);
        addRow(videoSection, tr("Format"), player.propertyText(QStringLiteral("video-format")));

        const QString width = player.propertyText(QStringLiteral("width"));
        const QString height = player.propertyText(QStringLiteral("height"));
        if (!width.isEmpty() && !height.isEmpty()) {
            addRow(videoSection, tr("Resolution"), QStringLiteral("%1 x %2").arg(width, height));
        }
        addRow(videoSection, tr("Frame rate"), player.propertyText(QStringLiteral("container-fps")));
        addRow(videoSection, tr("Bitrate"), player.propertyText(QStringLiteral("video-bitrate")));
    }

    // --- Audio ---------------------------------------------------------------
    const QString audioCodec = player.propertyText(QStringLiteral("audio-codec"));
    if (!audioCodec.isEmpty()) {
        auto *audioSection = new QTreeWidgetItem(tree_, {tr("Audio")});
        audioSection->setFirstColumnSpanned(true);
        addRow(audioSection, tr("Codec"), audioCodec);
        addRow(audioSection, tr("Channels"), player.propertyText(QStringLiteral("audio-params/channel-count")));
        addRow(audioSection, tr("Sample rate"), player.propertyText(QStringLiteral("audio-params/samplerate")));
        addRow(audioSection, tr("Bitrate"), player.propertyText(QStringLiteral("audio-bitrate")));
    }

    // --- Metadata ------------------------------------------------------------
    const QMap<QString, QString> metadata = player.metadata();
    if (!metadata.isEmpty()) {
        auto *metaSection = new QTreeWidgetItem(tree_, {tr("Metadata")});
        metaSection->setFirstColumnSpanned(true);
        for (auto it = metadata.constBegin(); it != metadata.constEnd(); ++it) {
            addRow(metaSection, it.key(), it.value());
        }
    }

    tree_->expandAll();

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    layout->addWidget(buttonBox);

    resize(560, 480);
}
