#pragma once

#include "EventType.h"
#include "Event.h"

#include <functional>

namespace kathub::events {

/// Abstract event bus interface.
///
/// Provides publish/subscribe/unsubscribe semantics for typed events.
/// Subscribers register callbacks per EventType and receive an opaque
/// handle for later unsubscription.
///
/// Concrete implementations live in the core/host layer and are
/// injected via dependency injection (e.g. as a singleton implementing
/// this interface).
///
/// Thread-safety contract:
///   publish(), subscribe() and unsubscribe() must be safe to
///   call concurrently from any thread.
class IEventBus
{
public:
    using Subscriber = std::function<void(const Event &)>;

    virtual ~IEventBus() = default;

    /// Publish an event to every subscriber registered for its type.
    /// Thread-safe.
    virtual void publish(const Event &event) = 0;

    /// Register a callback to receive all events of the given type.
    /// Returns an opaque handle for later unsubscription.
    /// Thread-safe.
    virtual int subscribe(const EventType &type, Subscriber callback) = 0;

    /// Remove a previously registered subscriber by handle.
    /// No-op if the handle is invalid or already removed.
    /// Thread-safe.
    virtual void unsubscribe(int handle) = 0;
};

} // namespace kathub::events
