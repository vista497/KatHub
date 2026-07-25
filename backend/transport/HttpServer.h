#pragma once

#include <shared_mutex>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>

// Forward declarations
class IHttpHandler;

namespace KatHub {
class SignalHub;
}

namespace httplib {
    class Server;
}

// Wraps httplib::Server in thread-pool mode.
// Thread-safe handler registration via shared_mutex.
class HttpServer
{
public:
    HttpServer();
    ~HttpServer();

    // Start listening on the given port. Non-blocking — spawns a worker thread.
    void start(int port = 8080);

    // Graceful shutdown: stops listening, joins the worker thread.
    void stop();

    // Register an HTTP handler. Thread-safe.
    // Maps IHttpHandler::route() + method() to an internal httplib handler lambda.
    void registerHandler(IHttpHandler *handler);

    // Set the SignalHub for publishing events after each request.
    void setSignalHub(KatHub::SignalHub *hub);

    // Returns true if the server is currently listening.
    bool isRunning() const;

    // Timestamp when the server was started (for uptime calculation).
    std::chrono::steady_clock::time_point startTime() const;

    // Access the underlying httplib::Server (e.g. for mount points).
    httplib::Server &server();

private:
    void installHandlers();

    std::unique_ptr<httplib::Server> server_;
    std::thread worker_;
    std::atomic<bool> running_{false};
    std::atomic<int> port_{8080};
    std::chrono::steady_clock::time_point startTime_;

    mutable std::shared_mutex handlerMutex_;
    std::vector<IHttpHandler *> handlers_;

    KatHub::SignalHub *signalHub_ = nullptr;
};
