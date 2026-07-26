#pragma once

#include <QString>
#include <QHash>

namespace kathub::events {

/// Extensible string-based event type.
///
/// Plugins and the core can define new event types without
/// sharing a centralized enum.  Common event types are
/// provided as static constants.
class EventType
{
public:
    /// Construct a custom event type from a name (e.g. "plugin:my-event").
    inline explicit EventType(const QString &name)
        : m_name(name) {}

    /// Human-readable name (e.g. "system:startup").
    inline const QString &name() const { return m_name; }

    inline bool operator==(const EventType &other) const { return m_name == other.m_name; }
    inline bool operator!=(const EventType &other) const { return !(*this == other); }

    /// Required for use as a QHash key.
    friend inline uint qHash(const EventType &t, uint seed = 0)
    {
        return qHash(t.m_name, seed);
    }

    // ---------------------------------------------------------------
    //  Built-in event types
    // ---------------------------------------------------------------

    /// Core lifecycle.
    static const EventType SystemStartup;
    static const EventType SystemShutdown;

    /// Plugin lifecycle.
    static const EventType PluginLoaded;
    static const EventType PluginUnloaded;

    /// Generic notifications.
    static const EventType Notification;

private:
    QString m_name;
};

} // namespace kathub::events
