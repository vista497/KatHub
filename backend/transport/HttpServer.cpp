#include "HttpServer.h"

#include "IHttpHandler.h"
#include "SignalHub.h"
#include "httplib.h"

// windows.h (pulled in by httplib.h) defines DELETE as a macro.
// Undefine it so IHttpHandler::HttpMethod enum values compile.
#ifdef DELETE
#undef DELETE
#endif

#include <iostream>
#include <thread>

// Convert IHttpHandler::HttpMethod to a string for httplib
static const char *methodStr(IHttpHandler::HttpMethod m)
{
    switch (m) {
    case IHttpHandler::HttpMethod::GET:    return "GET";
    case IHttpHandler::HttpMethod::POST:   return "POST";
    case IHttpHandler::HttpMethod::PUT:    return "PUT";
    case IHttpHandler::HttpMethod::DELETE: return "DELETE";
    }
    return "GET";
}

HttpServer::HttpServer()
    : server_(std::make_unique<httplib::Server>())
{
}

HttpServer::~HttpServer()
{
    stop();
}

void HttpServer::start(int port)
{
    if (running_.load())
        return;

    port_.store(port);
    startTime_ = std::chrono::steady_clock::now();

    // Install currently registered handlers before starting
    installHandlers();

    running_.store(true);

    worker_ = std::thread([this]() {
        server_->listen("0.0.0.0", port_.load());
    });

    // Brief sleep to let the listener bind
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

void HttpServer::setSignalHub(KatHub::SignalHub *hub)
{
    signalHub_ = hub;
}

void HttpServer::stop()
{
    if (!running_.load())
        return;

    server_->stop();
    running_.store(false);

    if (worker_.joinable())
        worker_.join();
}

void HttpServer::registerHandler(IHttpHandler *handler)
{
    if (!handler)
        return;

    {
        std::unique_lock lock(handlerMutex_);
        handlers_.push_back(handler);
    }

    // If already running, register immediately with httplib
    if (running_.load()) {
        const char *route = handler->route();
        auto method = handler->method();

        // Capture handler pointer; the lambda calls IHttpHandler::handle
        auto httplibHandler = [this, handler](const httplib::Request &req,
                                               httplib::Response &res) {
            // Pass request body and response pointer
            handler->handle(req.body.c_str(), &res);

            // Publish event to SignalHub after each request
            if (signalHub_) {
                QJsonObject event;
                event[QStringLiteral("route")] = QString::fromLatin1(handler->route());
                signalHub_->publish(QStringLiteral("http.request"), event);
            }
        };

        switch (method) {
        case IHttpHandler::HttpMethod::GET:
            server_->Get(route, httplibHandler);
            break;
        case IHttpHandler::HttpMethod::POST:
            server_->Post(route, httplibHandler);
            break;
        case IHttpHandler::HttpMethod::PUT:
            server_->Put(route, httplibHandler);
            break;
        case IHttpHandler::HttpMethod::DELETE:
            server_->Delete(route, httplibHandler);
            break;
        }
    }
}

bool HttpServer::isRunning() const
{
    return running_.load();
}

std::chrono::steady_clock::time_point HttpServer::startTime() const
{
    return startTime_;
}

httplib::Server &HttpServer::server()
{
    return *server_;
}

void HttpServer::installHandlers()
{
    std::shared_lock lock(handlerMutex_);

    for (auto *handler : handlers_) {
        const char *route = handler->route();
        auto method = handler->method();

        auto httplibHandler = [this, handler](const httplib::Request &req,
                                               httplib::Response &res) {
            handler->handle(req.body.c_str(), &res);

            // Publish event to SignalHub after each request
            if (signalHub_) {
                QJsonObject event;
                event[QStringLiteral("route")] = QString::fromLatin1(handler->route());
                signalHub_->publish(QStringLiteral("http.request"), event);
            }
        };

        switch (method) {
        case IHttpHandler::HttpMethod::GET:
            server_->Get(route, httplibHandler);
            break;
        case IHttpHandler::HttpMethod::POST:
            server_->Post(route, httplibHandler);
            break;
        case IHttpHandler::HttpMethod::PUT:
            server_->Put(route, httplibHandler);
            break;
        case IHttpHandler::HttpMethod::DELETE:
            server_->Delete(route, httplibHandler);
            break;
        }
    }
}
