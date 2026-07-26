#include "kathub/events/EventType.h"

namespace kathub::events {

// ---------------------------------------------------------------
// Built-in constants
// ---------------------------------------------------------------

const EventType EventType::SystemStartup  { QStringLiteral("system:startup") };
const EventType EventType::SystemShutdown { QStringLiteral("system:shutdown") };
const EventType EventType::PluginLoaded   { QStringLiteral("plugin:loaded") };
const EventType EventType::PluginUnloaded { QStringLiteral("plugin:unloaded") };
const EventType EventType::Notification   { QStringLiteral("app:notification") };

} // namespace kathub::events
