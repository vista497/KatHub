#pragma once

#include "IEventBus.h"

#include <QObject>
#include <QHash>
#include <QList>
#include <QMutex>

/// Qt-based thread-safe event bus implementing IEventBus.
///
/// Stores subscribers in a QHash keyed by topic.
/// All mutating operations are guarded by a single QMutex.
/// publish() snapshots the subscriber list under the lock,
/// then invokes callbacks outside the lock to avoid deadlocks
/// when callbacks themselves call subscribe/unsubscribe.
///
/// Emits eventPublished(topic, data) after every publish().
///
/// Intended to be registered as a singleton in the DI container
/// (or composition root) implementing IEventBus.
class EventBus : public QObject, public IEventBus
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(EventBus)

public:
    explicit EventBus(QObject *parent = nullptr);
    ~EventBus() override;

    // ---- IEventBus interface ------------------------------------------------

    int  subscribe(const QString &topic, Subscriber callback) override;
    void unsubscribe(const QString &topic, int handle) override;
    void publish(const QString &topic, const QJsonObject &data) override;

signals:
    /// Emitted after every publish() call.
    /// Connect to this signal for reactive / cross-thread delivery
    /// without needing an explicit subscriber callback.
    void eventPublished(const QString &topic, const QJsonObject &data);

private:
    struct Entry {
        int         handle   = 0;
        Subscriber  callback;
    };

    QHash<QString, QList<Entry>> m_subscribers;
    mutable QMutex               m_mutex;
    int                          m_nextHandle = 0;
};
