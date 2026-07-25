#pragma once

#include "config/JsonConfigLoader.h"

#include <memory>
#include <string>

class QCoreApplication;
class QString;
class PluginLoader;
class PluginRegistry;
class HttpServer;
class WsServer;
struct HostApi;

namespace KatHub {
class SignalHub;
class HandWindow;
}
// Composition root for the KatHub application.
// Owns all major subsystems and wires them together.
class KatHubApp
{
public:
    enum class Mode
    {
        Server,   // HTTP server mode
        Hand      // GUI / WebView hand mode (stub)
    };

    // Construct with command-line arguments and mode.
    // Call init() afterward to create subsystems.
    KatHubApp(int argc, char *argv[], Mode mode);

    // No copy / move.
    KatHubApp(const KatHubApp &) = delete;
    KatHubApp &operator=(const KatHubApp &) = delete;
    KatHubApp(KatHubApp &&) = delete;
    KatHubApp &operator=(KatHubApp &&) = delete;

    ~KatHubApp();

    // Configure core services (EventBus, etc.) before init().
    void configureServices();

    // Initialise all subsystems according to the mode.
    // In Server mode: creates PluginLoader, PluginRegistry, HttpServer,
    //   SignalHub, WsServer, registers built-in + plugin handlers,
    //   starts listening.
    // In Hand mode: creates a WebViewWindow (stub — QWebEngineView placeholder).
    void init();

    // Enter the Qt event loop. Returns the application exit code.
    int run();

    // Trigger graceful shutdown (stop server, unload plugins).
    void requestShutdown();

    // Access the HostApi for plugin initialisation.
    HostApi &hostApi();

    // Access the port (parsed from --port, config, or default 8080).
    int port() const;

    // Access the WebSocket port (parsed from --ws-port, config, or default 8081).
    int wsPort() const;

    // Access the QCoreApplication.
    QCoreApplication &app();

private:
    // Parse --server / --hand, --port, and --config from argv.
    // Also loads config from file and applies KATHUB_* env overrides.
    void parseArgs();

    // Build the HostApi struct.
    void buildHostApi();

    QCoreApplication *app_ = nullptr;
    Mode mode_ = Mode::Server;
    int port_ = 8080;
    int wsPort_ = 8081;
    QString handHost_{QStringLiteral("localhost")};
    std::unique_ptr<JsonConfigLoader> config_;

    // HostApi — shared with plugins.
    HostApi *hostApi_ = nullptr;

    // Subsystems (Server mode).
    std::unique_ptr<PluginLoader> pluginLoader_;
    std::unique_ptr<HttpServer> httpServer_;
    std::unique_ptr<KatHub::SignalHub> signalHub_;
    std::unique_ptr<WsServer> wsServer_;

    // Subsystems (Hand mode).
    std::unique_ptr<KatHub::HandWindow> handWindow_;
};
