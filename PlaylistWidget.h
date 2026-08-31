#pragma once

#include <QListWidget>

/// QListWidget that accepts files/folders dragged in from Explorer.
/// Forwards the raw dropped paths via filesDropped(); MainWindow resolves
/// folders to their playable contents and filters by extension.
class PlaylistWidget : public QListWidget
{
    Q_OBJECT

public:
    explicit PlaylistWidget(QWidget *parent = nullptr);

signals:
    void filesDropped(const QStringList &paths);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
};
