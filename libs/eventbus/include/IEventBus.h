#pragma once

#include <QJsonObject>
#include <QString>
#include <functional>

/// Pure abstract interface for a thread-safe event bus.
///
/// Subscribers register callbacks per topic.
/// publish() delivers a QJsonObject payload to all subscribers
/// of a given topic. Implementations MAY also emit a Qt signal.
///
/// Thread-safety contract:
///   subscribe(), unsubscribe() and publish() must be safe to
///   call concurrently from any thread.
class IEventBus
{
public:
    using Subscriber = std::function<void(const QJsonObject &)>;

    virtual ~IEventBus() = default;

    /// Register a callback for the given topic.
    /// Returns an opaque handle for later unsubscription.
    /// Thread-safe.
    virtual int subscribe(const QString &topic, Subscriber callback) = 0;

    /// Remove a previously registered subscriber by handle.
    /// If the topic has no remaining subscribers it is cleaned up.
    /// Thread-safe.
    virtual void unsubscribe(const QString &topic, int handle) = 0;

    /// Publish a JSON payload to all subscribers of a topic.
    /// Callbacks are invoked synchronously but outside any
    /// internal lock so they may call subscribe/unsubscribe
    /// without deadlocking.
    /// Thread-safe.
    virtual void publish(const QString &topic, const QJsonObject &data) = 0;
};
