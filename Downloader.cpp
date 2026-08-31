#include "Downloader.h"

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QSaveFile>
#include <QFileInfo>
#include <QDir>

Downloader::Downloader(QObject *parent)
    : QObject(parent)
{
}

Downloader::~Downloader()
{
    // Abort rather than wait: the reply is destroyed with this object, and a
    // QSaveFile that is never committed discards its temporary file.
    cancel();
}

void Downloader::start(const QUrl &url, const QString &destinationPath)
{
    if (reply_) {
        return;
    }

    cancelled_ = false;
    destinationPath_ = destinationPath;

    file_ = std::make_unique<QSaveFile>(destinationPath);
    if (!file_->open(QIODevice::WriteOnly)) {
        const QString reason = file_->errorString();
        file_.reset();
        emit failed(tr("Cannot write to %1: %2").arg(QDir::toNativeSeparators(destinationPath), reason));
        return;
    }

    QNetworkRequest request(url);
    // Media links very often redirect (CDN hand-offs, http->https). Qt6
    // follows redirects by default; stated explicitly so the behaviour does
    // not depend on a default that has changed across Qt versions.
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    // Some CDNs reject requests without a User-Agent outright.
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("PearPlayer"));

    reply_ = manager_.get(request);

    connect(reply_, &QNetworkReply::readyRead, this, [this] {
        if (file_ && reply_) {
            file_->write(reply_->readAll());
        }
    });

    connect(reply_, &QNetworkReply::downloadProgress, this, &Downloader::progress);

    connect(reply_, &QNetworkReply::finished, this, [this] {
        if (cancelled_) {
            return;
        }

        const QNetworkReply::NetworkError error = reply_->error();
        const QString errorText = reply_->errorString();
        const int status = reply_->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (error != QNetworkReply::NoError) {
            cleanUp(/*commit=*/false);
            emit failed(errorText);
            return;
        }

        // A 4xx/5xx body is a successful transfer of an error page as far as
        // QNetworkReply is concerned, so the status code is checked too --
        // otherwise a "Not Found" page gets saved as if it were the video.
        if (status >= 400) {
            cleanUp(/*commit=*/false);
            emit failed(tr("Server returned HTTP %1.").arg(status));
            return;
        }

        // A link to a streaming site answers a plain GET with its web page,
        // not the video. mpv can still play such a link (its ytdl hook
        // resolves it), so it is easy to try downloading one -- and without
        // this check the HTML would be saved under a .mp4 name and look like
        // a corrupt video rather than the wrong thing entirely.
        const QString contentType =
            reply_->header(QNetworkRequest::ContentTypeHeader).toString();
        if (contentType.startsWith(QLatin1StringView("text/html"), Qt::CaseInsensitive)) {
            cleanUp(/*commit=*/false);
            emit failed(tr("That link returns a web page, not a media file. "
                            "Direct links to a video or audio file can be downloaded; "
                            "links to streaming sites cannot."));
            return;
        }

        // Drain whatever arrived alongside the finished signal.
        if (file_ && reply_) {
            file_->write(reply_->readAll());
        }

        const QString saved = destinationPath_;
        if (!file_ || !file_->commit()) {
            const QString reason = file_ ? file_->errorString() : tr("unknown error");
            cleanUp(/*commit=*/false);
            emit failed(tr("Could not save the file: %1").arg(reason));
            return;
        }

        cleanUp(/*commit=*/false); // already committed; just tears down state
        emit finished(saved);
    });
}

void Downloader::cancel()
{
    if (!reply_) {
        return;
    }
    cancelled_ = true;
    reply_->abort();
    cleanUp(/*commit=*/false);
}

void Downloader::cleanUp(bool commit)
{
    if (file_) {
        if (commit) {
            file_->commit();
        }
        // Destroying an uncommitted QSaveFile removes its temporary file, so
        // a cancelled download never leaves a partial file on disk.
        file_.reset();
    }

    if (reply_) {
        reply_->deleteLater();
        reply_ = nullptr;
    }
}
