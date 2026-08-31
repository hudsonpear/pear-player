#pragma once

#include <QObject>
#include <QString>

#include <memory>

class QLocalServer;

/// Keeps the player to one window.
///
/// The first process to start owns a named local socket and becomes the
/// primary instance. Any later launch connects to that socket, hands over the
/// file it was asked to open and exits, so double-clicking a file while the
/// player is already running replaces what is playing instead of opening a
/// second window.
class SingleInstance : public QObject
{
    Q_OBJECT

public:
    /// key names the socket; it must be identical in every process, and
    /// unique enough not to collide with another application's.
    explicit SingleInstance(const QString &key, QObject *parent = nullptr);
    ~SingleInstance() override;

    /// Tries to hand message to an already-running instance. Returns true if
    /// one answered, in which case this process has nothing left to do and
    /// should exit.
    [[nodiscard]] bool sendToPrimary(const QString &message);

    /// Starts listening as the primary instance. Call only after
    /// sendToPrimary() has returned false.
    bool listen();

signals:
    /// A later launch handed us a file to open. Empty when it was started with
    /// no arguments, which still means "come to the front".
    void messageReceived(const QString &message);

private:
    QString key_;
    std::unique_ptr<QLocalServer> server_;
};
