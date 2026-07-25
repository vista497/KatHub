#include "EventBus.h"

#include <QMutexLocker>

// ============================================================================
//  Construction / Destruction
// ============================================================================

EventBus::EventBus(QObject *parent)
    : QObject(parent)
    , m_nextHandle(0)
{
}

EventBus::~EventBus() = default;

// ============================================================================
//  IEventBus — subscribe
// ============================================================================

int EventBus::subscribe(const QString &topic, Subscriber callback)
{
    QMutexLocker lock(&m_mutex);

    const int handle = m_nextHandle++;
    Entry entry{handle, std::move(callback)};
    m_subscribers[topic].append(std::move(entry));
    return handle;
}

// ============================================================================
//  IEventBus — unsubscribe
// ============================================================================

void EventBus::unsubscribe(const QString &topic, int handle)
{
    QMutexLocker lock(&m_mutex);

    auto it = m_subscribers.find(topic);
    if (it == m_subscribers.end())
        return;

    auto &list = it.value();
    list.erase(
        std::remove_if(list.begin(), list.end(),
                       [handle](const Entry &e) { return e.handle == handle; }),
        list.end());

    if (list.isEmpty())
        m_subscribers.erase(it);
}

// ============================================================================
//  IEventBus — publish
// ============================================================================

void EventBus::publish(const QString &topic, const QJsonObject &data)
{
    // Snapshot subscribers under the mutex so callbacks can safely
    // call subscribe / unsubscribe without deadlocking.
    QList<Subscriber> snapshot;
    {
        QMutexLocker lock(&m_mutex);
        auto it = m_subscribers.constFind(topic);
        if (it != m_subscribers.constEnd()) {
            snapshot.reserve(it->size());
            for (const auto &entry : *it)
                snapshot.append(entry.callback);
        }
    }

    // Invoke outside the lock.
    for (const auto &cb : snapshot)
        cb(data);

    emit eventPublished(topic, data);
}
