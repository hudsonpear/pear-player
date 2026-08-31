#include "SingleInstance.h"

#include <QLocalServer>
#include <QLocalSocket>

#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace {
// Long enough that a busy primary instance still answers, short enough that a
// stale socket does not visibly delay startup.
constexpr int kConnectTimeoutMs = 300;
constexpr int kWriteTimeoutMs = 1000;
} // namespace

SingleInstance::SingleInstance(const QString &key, QObject *parent)
    : QObject(parent)
    , key_(key)
{
}

SingleInstance::~SingleInstance() = default;

bool SingleInstance::sendToPrimary(const QString &message)
{
    QLocalSocket socket;
    socket.connectToServer(key_);
    if (!socket.waitForConnected(kConnectTimeoutMs)) {
        // Nobody is listening: either we are the first instance, or a previous
        // one crashed and left the socket behind. listen() handles both.
        return false;
    }

#ifdef Q_OS_WIN
    // Windows refuses to let a background process take the foreground, so the
    // primary instance's activateWindow() would silently do nothing and the
    // file would start playing behind everything else. Only the process that
    // currently owns the foreground can hand that right over -- which is this
    // one, since the user just launched it -- so permission is granted here
    // rather than attempted on the receiving side.
    AllowSetForegroundWindow(ASFW_ANY);
#endif

    socket.write(message.toUtf8());
    socket.flush();
    socket.waitForBytesWritten(kWriteTimeoutMs);
    socket.disconnectFromServer();
    return true;
}

bool SingleInstance::listen()
{
    server_ = std::make_unique<QLocalServer>();

    // A process killed without cleaning up leaves its socket file behind, and
    // listen() would then fail for ever after. Since sendToPrimary() has
    // already established that nobody is answering, any socket still sitting
    // there is stale and safe to clear.
    QLocalServer::removeServer(key_);

    if (!server_->listen(key_)) {
        server_.reset();
        return false;
    }

    connect(server_.get(), &QLocalServer::newConnection, this, [this] {
        QLocalSocket *client = server_->nextPendingConnection();
        if (!client) {
            return;
        }
        connect(client, &QLocalSocket::disconnected, client, &QLocalSocket::deleteLater);

        // The payload is one short path, so it arrives in a single read in
        // practice; waiting once covers the case where it does not.
        if (client->bytesAvailable() == 0) {
            client->waitForReadyRead(kWriteTimeoutMs);
        }
        emit messageReceived(QString::fromUtf8(client->readAll()));
    });

    return true;
}
