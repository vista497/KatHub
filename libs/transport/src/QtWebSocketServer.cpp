#include "QtWebSocketServer.h"

#include <QWebSocketServer>
#include <QWebSocket>
#include <QJsonDocument>
#include <QTimer>
#include <QDebug>

// ---------------------------------------------------------------------------
//  Construction / Destruction
// ---------------------------------------------------------------------------

QtWebSocketServer::QtWebSocketServer(const QString &serverName, QObject *parent)
    : QObject(parent)
{
    m_server = new QWebSocketServer(serverName,
                                    QWebSocketServer::NonSecureMode,
                                    this);

    connect(m_server, &QWebSocketServer::newConnection,
            this, &QtWebSocketServer::onNewConnection);

    m_pingTimer = new QTimer(this);
    m_pingTimer->setInterval(30'000);
    connect(m_pingTimer, &QTimer::timeout, this, [this]() {
        for (QWebSocket *sock : m_clients) {
            if (sock && sock->state() == QAbstractSocket::ConnectedState)
                sock->ping();
        }
    });
}

QtWebSocketServer::~QtWebSocketServer()
{
    close();
}

// ---------------------------------------------------------------------------
//  Lifecycle
// ---------------------------------------------------------------------------

bool QtWebSocketServer::listen(const QHostAddress &address, quint16 port)
{
    if (m_server->isListening())
        return true;

    const bool ok = m_server->listen(address, port);
    if (ok) {
        qDebug() << "QtWebSocketServer: listening on"
                 << m_server->serverAddress().toString()
                 << "port" << m_server->serverPort();
        m_pingTimer->start();
    } else {
        qWarning() << "QtWebSocketServer: listen failed —"
                    << m_server->errorString();
    }
    return ok;
}

void QtWebSocketServer::close()
{
    m_pingTimer->stop();

    m_server->close();

    // Disconnect signals + schedule deletion for every client.
    for (QWebSocket *sock : std::exchange(m_clients, {})) {
        if (sock) {
            disconnect(sock, nullptr, this, nullptr);
            sock->close();
            sock->deleteLater();
        }
    }
}

bool QtWebSocketServer::isListening() const
{
    return m_server->isListening();
}

QString QtWebSocketServer::errorString() const
{
    return m_server->errorString();
}

quint16 QtWebSocketServer::serverPort() const
{
    return m_server->serverPort();
}

QHostAddress QtWebSocketServer::serverAddress() const
{
    return m_server->serverAddress();
}

// ---------------------------------------------------------------------------
//  Broadcast
// ---------------------------------------------------------------------------

void QtWebSocketServer::broadcastText(const QString &text)
{
    for (QWebSocket *sock : m_clients) {
        if (sock && sock->state() == QAbstractSocket::ConnectedState)
            sock->sendTextMessage(text);
    }
}

void QtWebSocketServer::broadcastBinary(const QByteArray &data)
{
    for (QWebSocket *sock : m_clients) {
        if (sock && sock->state() == QAbstractSocket::ConnectedState)
            sock->sendBinaryMessage(data);
    }
}

void QtWebSocketServer::broadcastJson(const QJsonObject &json)
{
    const QByteArray payload =
        QJsonDocument(json).toJson(QJsonDocument::Compact);
    const QString text = QString::fromUtf8(payload);

    for (QWebSocket *sock : m_clients) {
        if (sock && sock->state() == QAbstractSocket::ConnectedState)
            sock->sendTextMessage(text);
    }
}

// ---------------------------------------------------------------------------
//  Per-client send
// ---------------------------------------------------------------------------

void QtWebSocketServer::sendText(QWebSocket *client, const QString &text)
{
    if (client && client->state() == QAbstractSocket::ConnectedState)
        client->sendTextMessage(text);
}

void QtWebSocketServer::sendBinary(QWebSocket *client, const QByteArray &data)
{
    if (client && client->state() == QAbstractSocket::ConnectedState)
        client->sendBinaryMessage(data);
}

void QtWebSocketServer::sendJson(QWebSocket *client, const QJsonObject &json)
{
    if (client && client->state() == QAbstractSocket::ConnectedState) {
        client->sendTextMessage(
            QString::fromUtf8(QJsonDocument(json).toJson(QJsonDocument::Compact)));
    }
}

// ---------------------------------------------------------------------------
//  Client management
// ---------------------------------------------------------------------------

int QtWebSocketServer::clientCount() const
{
    return m_clients.size();
}

QList<QWebSocket *> QtWebSocketServer::clients() const
{
    return m_clients;
}

// ---------------------------------------------------------------------------
//  Private slots
// ---------------------------------------------------------------------------

void QtWebSocketServer::onNewConnection()
{
    while (QWebSocket *sock = m_server->nextPendingConnection()) {
        m_clients.append(sock);

        connect(sock, &QWebSocket::disconnected,
                this, &QtWebSocketServer::onSocketDisconnected);

        // Per-socket text forwarding.
        connect(sock, &QWebSocket::textMessageReceived,
                this, &QtWebSocketServer::onTextMessage);

        // Per-socket binary forwarding.
        connect(sock, &QWebSocket::binaryMessageReceived,
                this, &QtWebSocketServer::onBinaryMessage);

        qDebug() << "QtWebSocketServer: client connected (total:"
                 << m_clients.size() << ")";

        emit clientConnected(sock);
    }
}

void QtWebSocketServer::onSocketDisconnected()
{
    auto *sock = qobject_cast<QWebSocket *>(sender());
    if (!sock)
        return;

    m_clients.removeAll(sock);

    qDebug() << "QtWebSocketServer: client disconnected (total:"
             << m_clients.size() << ")";

    emit clientDisconnected(sock);

    sock->deleteLater();
}

void QtWebSocketServer::onTextMessage(const QString &message)
{
    auto *sock = qobject_cast<QWebSocket *>(sender());
    if (sock)
        emit textMessageReceived(sock, message);
}

void QtWebSocketServer::onBinaryMessage(const QByteArray &message)
{
    auto *sock = qobject_cast<QWebSocket *>(sender());
    if (sock)
        emit binaryMessageReceived(sock, message);
}
