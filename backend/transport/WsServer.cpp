#include "WsServer.h"

#include "signalhub.h"

#include <QWebSocketServer>
#include <QWebSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QDebug>

// ---------------------------------------------------------------------------
//  Construction / Destruction
// ---------------------------------------------------------------------------

WsServer::WsServer(KatHub::SignalHub *hub, QObject *parent)
    : QObject(parent)
    , m_hub(hub)
{
    Q_ASSERT(m_hub);

    m_server = new QWebSocketServer(
        QStringLiteral("KatHub-WS"),
        QWebSocketServer::NonSecureMode,
        this);

    connect(m_server, &QWebSocketServer::newConnection,
            this, &WsServer::onNewConnection);

    // Ping keepalive: send a ping frame every 30 seconds.
    m_pingTimer = new QTimer(this);
    m_pingTimer->setInterval(30'000);
    connect(m_pingTimer, &QTimer::timeout,
            this, &WsServer::sendPing);
}

WsServer::~WsServer()
{
    stop();
}

// ---------------------------------------------------------------------------
//  start / stop
// ---------------------------------------------------------------------------

void WsServer::start(quint16 port)
{
    if (m_server->isListening())
        return;

    m_port = port;

    if (m_server->listen(QHostAddress::Any, port)) {
        // In case port 0 was passed, read back the OS-assigned port.
        m_port = m_server->serverPort();
        qDebug() << "WsServer listening on port" << m_port;
        m_pingTimer->start();
    } else {
        qWarning() << "WsServer failed to listen on port" << port
                    << ":" << m_server->errorString();
    }
}

void WsServer::stop()
{
    m_pingTimer->stop();

    // Unsubscribe from all SignalHub topics.
    for (auto it = m_subscriptions.cbegin(); it != m_subscriptions.cend(); ++it) {
        m_hub->unsubscribe(it.key(), it.value());
    }
    m_subscriptions.clear();

    // Close all client connections.
    for (QWebSocket *sock : m_clients) {
        if (sock) {
            disconnect(sock, nullptr, this, nullptr);
            sock->close();
            sock->deleteLater();
        }
    }
    m_clients.clear();

    m_server->close();
    m_port = 0;
}

bool WsServer::isRunning() const
{
    return m_server->isListening();
}

int WsServer::clientCount() const
{
    return m_clients.size();
}

QStringList WsServer::subscribedTopics() const
{
    return m_subscriptions.keys();
}

quint16 WsServer::port() const
{
    return m_port;
}

QString WsServer::errorString() const
{
    return m_server->errorString();
}

// ---------------------------------------------------------------------------
//  Slots
// ---------------------------------------------------------------------------

void WsServer::onNewConnection()
{
    while (QWebSocket *sock = m_server->nextPendingConnection()) {
        m_clients.append(sock);

        connect(sock, &QWebSocket::textMessageReceived,
                this, &WsServer::onTextMessageReceived);
        connect(sock, &QWebSocket::disconnected,
                this, &WsServer::onSocketDisconnected);

        qDebug() << "WsServer: new client connected (total:" << m_clients.size() << ")";
    }
}

void WsServer::onTextMessageReceived(const QString &message)
{
    auto *sock = qobject_cast<QWebSocket *>(sender());
    if (!sock)
        return;

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError) {
        qWarning() << "WsServer: invalid JSON from client:" << err.errorString();
        return;
    }

    if (!doc.isObject())
        return;

    QJsonObject obj = doc.object();
    const QString type = obj.value(QStringLiteral("type")).toString();

    if (type == QStringLiteral("subscribe")) {
        const QString topic = obj.value(QStringLiteral("topic")).toString();
        if (topic.isEmpty()) {
            qWarning() << "WsServer: subscribe without topic";
            return;
        }

        // Subscribe to SignalHub once per unique topic.
        if (!m_subscriptions.contains(topic)) {
            // Connect the signal once so we broadcast on every publish.
            connect(m_hub, &KatHub::SignalHub::signalPublished,
                    this, &WsServer::onSignalPublished,
                    Qt::UniqueConnection);

            // Delivery is handled by the signalPublished → onSignalPublished
            // connection. The callback is just a no-op placeholder.
            int handle = m_hub->subscribe(topic, [](const QJsonObject &) {});
            m_subscriptions.insert(topic, handle);

            qDebug() << "WsServer: subscribed to topic" << topic;
        }

        // Ack back to the client.
        QJsonObject ack;
        ack[QStringLiteral("type")] = QStringLiteral("subscribed");
        ack[QStringLiteral("topic")] = topic;
        sock->sendTextMessage(QJsonDocument(ack).toJson(QJsonDocument::Compact));

    } else if (type == QStringLiteral("publish")) {
        // Bridge client message to EventBus via SignalHub.
        const QString topic = obj.value(QStringLiteral("topic")).toString();
        if (topic.isEmpty()) {
            qWarning() << "WsServer: publish without topic";
            return;
        }
        QJsonObject data = obj.value(QStringLiteral("data")).toObject();
        m_hub->publish(topic, data);

        qDebug() << "WsServer: client published to topic" << topic;

    } else {
        qDebug() << "WsServer: unknown message type:" << type;
    }
}

void WsServer::onSocketDisconnected()
{
    auto *sock = qobject_cast<QWebSocket *>(sender());
    if (!sock)
        return;

    m_clients.removeAll(sock);
    sock->deleteLater();

    qDebug() << "WsServer: client disconnected (total:" << m_clients.size() << ")";
}

void WsServer::onSignalPublished(const QString &topic, const QJsonObject &data)
{
    Q_UNUSED(topic)

    // Wrap the payload so the client knows what topic it belongs to.
    QJsonObject msg;
    msg[QStringLiteral("type")] = QStringLiteral("published");
    msg[QStringLiteral("topic")] = topic;
    msg[QStringLiteral("data")] = data;

    broadcast(msg);
}

void WsServer::sendPing()
{
    for (QWebSocket *sock : m_clients) {
        if (sock && sock->state() == QAbstractSocket::ConnectedState) {
            sock->ping();
        }
    }
}

// ---------------------------------------------------------------------------
//  Helpers
// ---------------------------------------------------------------------------

void WsServer::broadcast(const QJsonObject &obj)
{
    const QByteArray payload = QJsonDocument(obj).toJson(QJsonDocument::Compact);

    for (QWebSocket *sock : m_clients) {
        if (sock && sock->state() == QAbstractSocket::ConnectedState) {
            sock->sendTextMessage(QString::fromUtf8(payload));
        }
    }
}
