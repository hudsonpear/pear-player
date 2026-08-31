#pragma once

#include <QObject>
#include <QUrl>
#include <QString>
#include <QNetworkAccessManager>

#include <memory>

class QNetworkReply;
class QSaveFile;

/// Downloads one HTTP(S) URL to a file on disk.
///
/// Data is streamed to the destination as it arrives rather than buffered in
/// memory, so a multi-gigabyte movie costs no more RAM than a small clip. The
/// destination is written through QSaveFile, meaning a cancelled or failed
/// transfer leaves no half-written file behind.
///
/// One Downloader handles one transfer at a time; start() while a transfer is
/// running is ignored.
class Downloader : public QObject
{
    Q_OBJECT

public:
    explicit Downloader(QObject *parent = nullptr);
    ~Downloader() override;

    Downloader(const Downloader &) = delete;
    Downloader &operator=(const Downloader &) = delete;

    /// Begins downloading url into destinationPath. Progress and the outcome
    /// are reported through the signals below; nothing is emitted synchronously.
    void start(const QUrl &url, const QString &destinationPath);

    /// Aborts the transfer in progress, if any. No signal follows a cancel.
    void cancel();

    [[nodiscard]] bool isRunning() const noexcept { return reply_ != nullptr; }

signals:
    /// bytesTotal is -1 when the server does not report a content length
    /// (chunked responses), in which case only bytesReceived is meaningful.
    void progress(qint64 bytesReceived, qint64 bytesTotal);
    void finished(QString savedPath);
    void failed(QString message);

private:
    void cleanUp(bool commit);

    QNetworkAccessManager manager_;
    QNetworkReply *reply_ = nullptr;
    std::unique_ptr<QSaveFile> file_;
    QString destinationPath_;
    bool cancelled_ = false;
};
