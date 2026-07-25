#pragma once

#include <QObject>
#include <QList>
#include <QHash>
#include <QJsonObject>
#include <QTimer>

class QWebSocketServer;
class QWebSocket;

namespace KatHub {
class SignalHub;
}

/// Qt WebSocket server wrapper connected to SignalHub.
///
/// Listens for WebSocket connections on a configurable port (default 8081).
/// Clients send JSON messages to subscribe to SignalHub topics.
/// When SignalHub publishes a subscribed topic, the payload is broadcast
/// to all connected WebSocket clients.
class WsServer : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(WsServer)

public:
    /// @param hub  SignalHub instance (dependency injection). Must outlive WsServer.
    explicit WsServer(KatHub::SignalHub *hub, QObject *parent = nullptr);
    ~WsServer() override;

    /// Start listening on the given port. Non-blocking.
    void start(quint16 port = 8081);

    /// Graceful shutdown: close all connections, stop listening.
    void stop();

    /// Returns true if the server is currently listening.
    bool isRunning() const;

    /// Returns the last error string from the underlying server.
    QString errorString() const;

private slots:
    void onNewConnection();
    void onTextMessageReceived(const QString &message);
    void onSocketDisconnected();
    void onSignalPublished(const QString &topic, const QJsonObject &data);
    void sendPing();

private:
    /// Broadcast a JSON object to every connected client.
    void broadcast(const QJsonObject &obj);

    QWebSocketServer *m_server = nullptr;
    KatHub::SignalHub *m_hub = nullptr;
    QList<QWebSocket *> m_clients;
    QHash<QString, int> m_subscriptions; // topic → SignalHub handle
    QTimer *m_pingTimer = nullptr;
};
