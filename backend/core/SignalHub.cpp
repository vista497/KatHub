#include "SignalHub.h"

#include <QWriteLocker>
#include <QReadLocker>

namespace KatHub {

SignalHub::SignalHub(QObject *parent)
    : QObject(parent)
    , m_nextHandle(0)
{
}

SignalHub::~SignalHub() = default;

int SignalHub::subscribe(const QString &topic, Subscriber callback)
{
    QWriteLocker lock(&m_lock);

    int handle = m_nextHandle++;
    Entry entry{handle, std::move(callback)};
    m_subscribers[topic].append(std::move(entry));
    return handle;
}

void SignalHub::unsubscribe(const QString &topic, int handle)
{
    QWriteLocker lock(&m_lock);

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

void SignalHub::publish(const QString &topic, const QJsonObject &data)
{
    // Snapshot subscribers under read lock, then invoke outside the lock
    // so callbacks can safely call subscribe/unsubscribe without deadlocking.
    QList<Subscriber> snapshot;
    {
        QReadLocker lock(&m_lock);
        auto it = m_subscribers.constFind(topic);
        if (it != m_subscribers.constEnd()) {
            snapshot.reserve(it->size());
            for (const auto &entry : *it)
                snapshot.append(entry.callback);
        }
    }

    for (const auto &cb : snapshot)
        cb(data);

    emit signalPublished(topic, data);
}

} // namespace KatHub
