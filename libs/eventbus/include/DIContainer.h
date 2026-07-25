#pragma once

#include "IEventBus.h"

#include <QMutex>
#include <QHash>
#include <QString>
#include <QMutexLocker>
#include <memory>
#include <stdexcept>
#include <typeinfo>

/// Simple DI container / service locator.
///
/// Thread-safe singleton that maps interface type names to
/// concrete instances.  Intended for wiring the composition root
/// (KatHubApp) and resolving dependencies at startup.
///
/// Usage:
///   DIContainer::instance().registerSingleton<IEventBus>(eventBus);
///   auto *bus = DIContainer::instance().resolve<IEventBus>();
class DIContainer
{
public:
    static DIContainer &instance();

    /// Register a singleton instance for the given interface tag.
    /// Call once at startup from the composition root.
    /// Overwrites any previously registered instance for the same tag.
    template <typename Interface>
    void registerSingleton(Interface *instance)
    {
        QMutexLocker lock(&m_mutex);
        m_registry[typeName<Interface>()] = static_cast<void *>(instance);
    }

    /// Resolve a previously registered singleton.
    /// Throws std::runtime_error if not found.
    template <typename Interface>
    Interface *resolve()
    {
        QMutexLocker lock(&m_mutex);
        auto it = m_registry.constFind(typeName<Interface>());
        if (it == m_registry.constEnd()) {
            throw std::runtime_error(
                QStringLiteral("DIContainer: no registration for '%1'")
                    .arg(QString::fromLatin1(typeName<Interface>()))
                    .toStdString());
        }
        return static_cast<Interface *>(it.value());
    }

    /// Check whether an interface is registered.
    template <typename Interface>
    bool isRegistered()
    {
        QMutexLocker lock(&m_mutex);
        return m_registry.contains(typeName<Interface>());
    }

private:
    DIContainer() = default;
    Q_DISABLE_COPY_MOVE(DIContainer)

    /// Compile-time type tag — uses the mangled class name (zero runtime cost).
    template <typename T>
    static const char *typeName()
    {
        // typeid(T).name() returns a unique per-type string at compile time;
        // on MSVC it's the undecorated class name.
        return typeid(T).name();
    }

    mutable QMutex m_mutex;
    QHash<QString, void *> m_registry;
};

// Inline singleton accessor (Meyer's singleton).
inline DIContainer &DIContainer::instance()
{
    static DIContainer s;
    return s;
}
