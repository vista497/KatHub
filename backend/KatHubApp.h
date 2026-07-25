#pragma once

#include <memory>
#include <string>

class QCoreApplication;
class PluginLoader;
class PluginRegistry;
class HttpServer;
struct HostApi;

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

    // Initialise all subsystems according to the mode.
    // In Server mode: creates PluginLoader, PluginRegistry, HttpServer,
    //   registers built-in + plugin handlers, starts listening.
    // In Hand mode: creates a WebViewWindow (stub — QWebEngineView placeholder).
    void init();

    // Enter the Qt event loop. Returns the application exit code.
    int run();

    // Trigger graceful shutdown (stop server, unload plugins).
    void requestShutdown();

    // Access the HostApi for plugin initialisation.
    HostApi &hostApi();

    // Access the port (parsed from --port, default 8080).
    int port() const;

    // Access the QCoreApplication.
    QCoreApplication &app();

private:
    // Parse --server / --hand and --port from argv.
    void parseArgs();

    // Build the HostApi struct.
    void buildHostApi();

    QCoreApplication *app_ = nullptr;
    Mode mode_ = Mode::Server;
    int port_ = 8080;

    // HostApi — shared with plugins.
    HostApi *hostApi_ = nullptr;

    // Subsystems (Server mode).
    std::unique_ptr<PluginLoader> pluginLoader_;
    std::unique_ptr<HttpServer> httpServer_;

    // Subsystems (Hand mode) — TODO.
    // std::unique_ptr<WebViewWindow> webViewWindow_;
};
