#pragma once

#include <QObject>
#include <QHash>
#include <QList>
#include <QJsonObject>
#include <QReadWriteLock>
#include <functional>

namespace KatHub {

/// Thread-safe event bus using Qt signals.
///
/// Subscribers register callbacks per topic.
/// publish() delivers a QJsonObject payload to all subscribers
/// of that topic and emits the Qt signal signalPublished.
class SignalHub : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SignalHub)

public:
    using Subscriber = std::function<void(const QJsonObject &)>;

    explicit SignalHub(QObject *parent = nullptr);
    ~SignalHub() override;

    /// Register a callback for the given topic.
    /// Returns a handle (index) that can be used to unsubscribe later.
    /// Thread-safe.
    int subscribe(const QString &topic, Subscriber callback);

    /// Remove a previously registered subscriber by handle.
    /// Thread-safe.
    void unsubscribe(const QString &topic, int handle);

    /// Publish a JSON payload to all subscribers of a topic.
    /// Emits signalPublished(topic, data) afterwards.
    /// Thread-safe.
    void publish(const QString &topic, const QJsonObject &data);

signals:
    /// Emitted after every publish() call.
    void signalPublished(const QString &topic, const QJsonObject &data);

private:
    struct Entry {
        int         handle = 0;
        Subscriber  callback;
    };

    QHash<QString, QList<Entry>> m_subscribers;
    mutable QReadWriteLock        m_lock;
    int                           m_nextHandle = 0;
};

} // namespace KatHub
