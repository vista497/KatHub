#pragma once

#include "EventType.h"

#include <QString>
#include <QJsonObject>
#include <QtGlobal>

namespace kathub::events {

/// Immutable event value object passed through the event bus.
///
/// Every event carries:
///   - type      — semantic category (see EventType)
///   - source    — human-readable origin identifier (e.g. "plugin:status")
///   - timestamp — milliseconds since Unix epoch (UTC)
///   - payload   — arbitrary JSON data
struct Event
{
    Event() = default;

    Event(EventType type,
          QString source,
          qint64 timestamp,
          QJsonObject payload = {})
        : type(std::move(type))
        , source(std::move(source))
        , timestamp(timestamp)
        , payload(std::move(payload))
    {}

    EventType     type;
    QString       source;
    qint64        timestamp = 0;
    QJsonObject   payload;
};

} // namespace kathub::events
