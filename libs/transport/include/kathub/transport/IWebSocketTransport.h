#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace kathub {
namespace transport {

/// Abstract WebSocket transport adapter.
///
/// Decouples Qt WebSocket dependency from business logic.
/// Implementations wrap a concrete WebSocket server (e.g.
/// QtWebSocket-based WsServer) behind this pure-C++ interface
/// so that domain/host layers never import QWebSocket headers.
///
/// Lifecycle: construct → start(port) → ... → stop() → destroy.
class IWebSocketTransport
{
public:
    virtual ~IWebSocketTransport() = default;

    /// Start listening on the given port. Non-blocking.
    /// @param port  TCP port number (e.g. 8081).
    /// @return true if the server started successfully.
    virtual bool start(std::uint16_t port) = 0;

    /// Stop listening and close all client connections.
    virtual void stop() = 0;

    /// Callback invoked when a text message arrives from a client.
    /// Parameters: (clientId, message).
    using MessageCallback = std::function<void(
        const std::string &clientId,
        const std::string &message)>;

    /// Register a callback for incoming text messages.
    /// Only one callback is active at a time; calling again replaces
    /// the previous one.
    virtual void onMessage(MessageCallback callback) = 0;

    /// Broadcast a text message to every connected client.
    /// @param msg  The message to broadcast.
    virtual void broadcast(const std::string &msg) = 0;

    /// Return the list of currently connected client identifiers.
    virtual std::vector<std::string> getConnections() const = 0;
};

} // namespace transport
} // namespace kathub
