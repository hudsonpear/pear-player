#pragma once

#include <QDialog>

class MpvPlayer;
class QTreeWidget;

/// Read-only view of what is playing: container and codec details on one side,
/// the file's own metadata tags on the other.
///
/// Everything is read once when the dialog opens. It is a snapshot rather than
/// a live view because none of it changes during playback.
class MediaInfoDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MediaInfoDialog(const MpvPlayer &player, const QString &mediaPath,
                              QWidget *parent = nullptr);

private:
    QTreeWidget *tree_ = nullptr;
};
