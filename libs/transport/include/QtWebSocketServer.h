#pragma once

#include <QObject>
#include <QList>
#include <QHostAddress>
#include <QByteArray>
#include <QJsonObject>

class QWebSocketServer;
class QWebSocket;
class QTimer;

/// Clean Qt wrapper around QWebSocketServer for reusable WebSocket transport.
///
/// Manages client lifecycle (connect / disconnect), receives both text and
/// binary frames, and provides broadcast + per-client send. Everything lives
/// on the Qt event loop — no threads are spawned internally.
///
/// Usage:
///   auto *ws = new QtWebSocketServer("MyApp", this);
///   connect(ws, &QtWebSocketServer::textMessageReceived,
///           this, &MyHandler::onMessage);
///   ws->listen(QHostAddress::Any, 9000);
class QtWebSocketServer : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(QtWebSocketServer)

public:
    /// @param serverName  Internal name passed to QWebSocketServer ctor.
    /// @param parent      Qt parent object.
    explicit QtWebSocketServer(const QString &serverName, QObject *parent = nullptr);
    ~QtWebSocketServer() override;

    // ---- Lifecycle ----------------------------------------------------------

    /// Start listening.  Non-blocking — returns immediately.
    /// @returns true if the server socket bound successfully.
    bool listen(const QHostAddress &address = QHostAddress::Any, quint16 port = 0);

    /// Graceful shutdown: disconnect signals from every client,
    /// close them, and shut down the underlying QWebSocketServer.
    void close();

    /// @returns true while the server is accepting connections.
    bool isListening() const;

    /// @returns human-readable description of the last error.
    QString errorString() const;

    /// @returns port the server is actually listening on (useful when port=0).
    quint16 serverPort() const;

    /// @returns the bound address.
    QHostAddress serverAddress() const;

    // ---- Broadcast ----------------------------------------------------------

    /// Send a text frame to every connected client.
    void broadcastText(const QString &text);

    /// Send a binary frame to every connected client.
    void broadcastBinary(const QByteArray &data);

    /// Convenience: serialise a QJsonObject to compact JSON and broadcast it.
    void broadcastJson(const QJsonObject &json);

    // ---- Per-client send ----------------------------------------------------

    /// Send a text frame to a single client.  No-op if client is nullptr or
    /// not in ConnectedState.
    void sendText(QWebSocket *client, const QString &text);

    /// Send a binary frame to a single client.
    void sendBinary(QWebSocket *client, const QByteArray &data);

    /// Convenience: serialise QJsonObject → compact JSON → send to one client.
    static void sendJson(QWebSocket *client, const QJsonObject &json);

    // ---- Client management --------------------------------------------------

    /// Number of currently connected clients.
    int clientCount() const;

    /// Snapshot of the current client list.  The QWebSocket pointers are
    /// owned by the server — do not delete them.
    QList<QWebSocket *> clients() const;

signals:
    /// Emitted after a new client is accepted and wired up.
    void clientConnected(QWebSocket *client);

    /// Emitted just before a client is removed from the list and deleted.
    /// After this signal returns the pointer is no longer valid.
    void clientDisconnected(QWebSocket *client);

    /// Emitted when a connected client sends a text frame.
    void textMessageReceived(QWebSocket *client, const QString &message);

    /// Emitted when a connected client sends a binary frame.
    void binaryMessageReceived(QWebSocket *client, const QByteArray &message);

private slots:
    void onNewConnection();
    void onSocketDisconnected();

    // Per-socket forwarding helpers.
    void onTextMessage(const QString &message);
    void onBinaryMessage(const QByteArray &message);

private:
    /// Start the ping timer (30 s interval).
    void startPingTimer();
    void stopPingTimer();

    QWebSocketServer *m_server = nullptr;
    QTimer           *m_pingTimer = nullptr;
    QList<QWebSocket *> m_clients;
};
