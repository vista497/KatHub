#include "KatHubApp.h"

#include "HostApi.h"
#include "HttpServer.h"
#include "IHttpHandler.h"
#include "PluginLoader.h"
#include "PluginRegistry.h"
#include "SignalHub.h"
#include "StaticFileHandler.h"
#include "StatusHandler.h"
#include "WsServer.h"

#include <QCoreApplication>
#include <QString>

#include <iostream>
#include <csignal>
#include <cstring>

// ============================================================================
//  Signal handling — static so the signal handler can reach the app instance.
// ============================================================================
static KatHubApp *g_app = nullptr;

static void signalHandler(int /*sig*/)
{
    std::cout << "Shutting down..." << std::endl;
    if (g_app) {
        g_app->requestShutdown();
    }
    QCoreApplication::quit();
}

// ============================================================================
//  Construction / Destruction
// ============================================================================

KatHubApp::KatHubApp(int argc, char *argv[], Mode mode)
    : mode_(mode)
{
    app_ = new QCoreApplication(argc, argv);
    g_app = this;

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    parseArgs();
    buildHostApi();
}

KatHubApp::~KatHubApp()
{
    g_app = nullptr;
    delete hostApi_;
    delete app_;
}

// ============================================================================
//  Argument parsing
// ============================================================================

void KatHubApp::parseArgs()
{
    const QStringList args = QCoreApplication::arguments();

    for (int i = 1; i < args.size(); ++i) {
        const QString &arg = args[i];

        if (arg == QStringLiteral("--server")) {
            mode_ = Mode::Server;
        } else if (arg == QStringLiteral("--hand")) {
            mode_ = Mode::Hand;
        } else if (arg == QStringLiteral("--port")) {
            if (i + 1 < args.size()) {
                bool ok = false;
                int p = args[++i].toInt(&ok);
                if (ok && p > 0 && p < 65536) {
                    port_ = p;
                } else {
                    std::cerr << "Invalid port: " << args[i].toStdString() << std::endl;
                }
            } else {
                std::cerr << "--port requires a value" << std::endl;
            }
        }
    }
}

// ============================================================================
//  HostApi
// ============================================================================

void KatHubApp::buildHostApi()
{
    hostApi_ = new HostApi;
    hostApi_->router        = nullptr;   // stub
    hostApi_->eventBus      = nullptr;   // stub
    hostApi_->configService = nullptr;   // stub
    hostApi_->logService    = nullptr;   // stub
    hostApi_->hostVersion   = "0.1.0";
}

HostApi &KatHubApp::hostApi()
{
    return *hostApi_;
}

// ============================================================================
//  init()
// ============================================================================

void KatHubApp::init()
{
    if (mode_ == Mode::Server) {
        std::cout << "KatHub starting in Server mode..." << std::endl;

        // Create PluginLoader (loads dynamic plugins).
        pluginLoader_ = std::make_unique<PluginLoader>(PluginRegistry::instance());

        // Create the event bus.
        signalHub_ = std::make_unique<KatHub::SignalHub>();

        // Create the WebSocket server, connected to SignalHub.
        wsServer_ = std::make_unique<WsServer>(signalHub_.get());
        wsServer_->start(8081);

        // Create the HTTP server and wire it to SignalHub.
        httpServer_ = std::make_unique<HttpServer>();
        httpServer_->setSignalHub(signalHub_.get());

        // Register built-in handlers registered via REGISTER_HANDLER macro.
        const auto &staticHandlers = StaticHandlerRegistry::instance().handlers();
        for (auto *handler : staticHandlers) {
            // If the handler is a StatusHandler, give it the SignalHub.
            if (auto *sh = dynamic_cast<StatusHandler *>(handler)) {
                sh->setSignalHub(signalHub_.get());
            }
            httpServer_->registerHandler(handler);
            std::cout << "Registered handler: " << handler->route() << std::endl;
        }

        // Register dynamic plugin handlers from PluginRegistry.
        const auto &pluginHandlers = PluginRegistry::instance().handlers();
        for (auto *handler : pluginHandlers) {
            httpServer_->registerHandler(handler);
            std::cout << "Registered plugin handler: " << handler->route() << std::endl;
        }

        // Set up HttpServer with parsed port.
        std::cout << "Listening on :" << port_ << " (HTTP), :8081 (WebSocket)" << std::endl;
        httpServer_->start(port_);

    } else {
        std::cout << "KatHub starting in Hand mode (stub)..." << std::endl;
        // TODO: Create WebViewWindow with QWebEngineView placeholder.
        // webViewWindow_ = std::make_unique<WebViewWindow>();
        // webViewWindow_->show();
    }
}

// ============================================================================
//  run()
// ============================================================================

int KatHubApp::run()
{
    return app_->exec();
}

// ============================================================================
//  requestShutdown()
// ============================================================================

void KatHubApp::requestShutdown()
{
    if (httpServer_) {
        httpServer_->stop();
    }

    if (wsServer_) {
        wsServer_->stop();
    }

    // PluginLoader destructor handles orderly plugin teardown.
    pluginLoader_.reset();

    std::cout << "KatHub backend stopped." << std::endl;
}

// ============================================================================
//  Accessors
// ============================================================================

int KatHubApp::port() const
{
    return port_;
}

QCoreApplication &KatHubApp::app()
{
    return *app_;
}
