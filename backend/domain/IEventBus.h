#pragma once

#include "domain_export.h"
#include "EventType.h"
#include "Event.h"

#include <functional>

namespace KatHub {

/// Abstract event bus interface.
///
/// Plugins receive an opaque `void* eventBus` via HostApi and cast
/// it to `IEventBus*` to publish events or subscribe to them.
///
/// Concrete implementations live in the core layer (e.g. an adapter
/// wrapping SignalHub).
class KATHUB_DOMAIN_EXPORT IEventBus
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

} // namespace KatHub
