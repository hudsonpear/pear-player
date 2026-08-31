#include "PlaylistWidget.h"

#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>

PlaylistWidget::PlaylistWidget(QWidget *parent)
    : QListWidget(parent)
{
    setAcceptDrops(true);
}

void PlaylistWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void PlaylistWidget::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void PlaylistWidget::dropEvent(QDropEvent *event)
{
    const QMimeData *mime = event->mimeData();
    if (!mime->hasUrls()) {
        return;
    }

    QStringList paths;
    for (const QUrl &url : mime->urls()) {
        if (url.isLocalFile()) {
            paths << url.toLocalFile();
        }
    }

    if (!paths.isEmpty()) {
        emit filesDropped(paths);
        event->acceptProposedAction();
    }
}
