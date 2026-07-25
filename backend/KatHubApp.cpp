#include "KatHubApp.h"

#include "HostApi.h"
#include "HttpServer.h"
#include "IHttpHandler.h"
#include "PluginLoader.h"
#include "PluginRegistry.h"
#include "SignalHub.h"
#include "StaticFileHandler.h"
#include "StatusHandler.h"
#include "HandWindow.h"
#include "WsServer.h"
#include "WsStatusHandler.h"
#include "ChatHandler.h"
#include "ai/AIController.h"
#include "ai/Conversation.h"
#include "ai/ToolDispatcher.h"
#include "prompts/PromptManager.h"
#include "prompts/AgentProfile.h"
#include "core/CrashHandler.h"
#include "core/Logger.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QJsonObject>
#include <QProcess>
#include <QString>
#include <QUrl>
#include <QTimer>

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
    // Hand mode needs QApplication for QWidget / QWebEngineView.
    // Server mode only needs QCoreApplication (no GUI dependency).
    if (mode == Mode::Hand) {
        app_ = new QApplication(argc, argv);
    } else {
        app_ = new QCoreApplication(argc, argv);
    }
    g_app = this;

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // ── Crash Handler ──────────────────────────────────────────
    // Install Windows SEH crash handler for self-diagnosis on crash.
    {
        std::string logDir;
        // Use AppData for crash logs.
        const char *appdata = std::getenv("APPDATA");
        if (appdata)
            logDir = std::string(appdata) + "/KatHub/logs";
        else
            logDir = "logs";  // fallback

        // Callback: publish crash to SignalHub (if alive) + Logger.
        auto onPanic = [](const std::string &info) {
            // Write to Logger as last resort.
            KatHub::Logger::instance().error("[CRASH] " + info, "CrashHandler");
        };

        KatHub::CrashHandler::install(logDir, onPanic);

        // Check for previous crash.
        if (KatHub::CrashHandler::hadPreviousCrash()) {
            std::string prevPanic = KatHub::CrashHandler::readPanicLog();
            std::cerr << "\n*** WARNING: Previous run ended with a crash! ***\n\n";
            std::cerr << prevPanic << "\n";
            std::cerr << "*** End of previous crash report ***\n\n";
        }
    }

    // Create config loader early so CLI args can override it.
    config_ = std::make_unique<JsonConfigLoader>();

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
    QString configPath;

    for (int i = 1; i < args.size(); ++i) {
        const QString &arg = args[i];

        if (arg == QStringLiteral("--server")) {
            mode_ = Mode::Server;
        } else if (arg == QStringLiteral("--hand")) {
            mode_ = Mode::Hand;
        } else if (arg == QStringLiteral("--watchdog")) {
            mode_ = Mode::Watchdog;
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
        } else if (arg == QStringLiteral("--ws-port")) {
            if (i + 1 < args.size()) {
                bool ok = false;
                int p = args[++i].toInt(&ok);
                if (ok && p > 0 && p < 65536) {
                    wsPort_ = p;
                } else {
                    std::cerr << "Invalid ws-port: " << args[i].toStdString() << std::endl;
                }
            } else {
                std::cerr << "--ws-port requires a value" << std::endl;
            }
        } else if (arg == QStringLiteral("--host")) {
            if (i + 1 < args.size()) {
                handHost_ = args[++i];
            } else {
                std::cerr << "--host requires a hostname" << std::endl;
            }
        } else if (arg == QStringLiteral("--config")) {
            if (i + 1 < args.size()) {
                configPath = args[++i];
            } else {
                std::cerr << "--config requires a file path" << std::endl;
            }
        }
    }

    // Load config file (if specified).
    if (!configPath.isEmpty()) {
        if (config_->load(configPath)) {
            std::cout << "Loaded config: " << configPath.toStdString() << std::endl;
        } else {
            std::cerr << "Failed to load config: " << configPath.toStdString() << std::endl;
        }
    }

    // Apply KATHUB_* env overrides (always applied, even without --config).
    config_->applyEnvOverrides();

    // Override port_ / wsPort_ from config if not set via CLI.
    // CLI args already set the members above; only apply config if they're
    // still at their defaults (8080 / 8081).
    if (config_->contains(QStringLiteral("server.port")) && port_ == 8080) {
        port_ = config_->intValue(QStringLiteral("server.port"), 8080);
    }
    if (config_->contains(QStringLiteral("ws.port")) && wsPort_ == 8081) {
        wsPort_ = config_->intValue(QStringLiteral("ws.port"), 8081);
    }
}

// ============================================================================
//  HostApi
// ============================================================================

void KatHubApp::buildHostApi()
{
    hostApi_ = new HostApi;
    hostApi_->router        = nullptr;   // stub
    hostApi_->eventBus      = nullptr;   // wired in init()
    hostApi_->configService = config_.get();
    hostApi_->logService    = nullptr;   // stub
    hostApi_->hostVersion   = "0.1.0";
}

HostApi &KatHubApp::hostApi()
{
    return *hostApi_;
}

// ============================================================================
//  configureServices()
// ============================================================================

void KatHubApp::configureServices()
{
    // Create the Qt-based SignalHub — the primary event bus for the system.
    signalHub_ = std::make_unique<KatHub::SignalHub>();

    std::cout << "SignalHub registered." << std::endl;
}

// ============================================================================
//  init()
// ============================================================================

void KatHubApp::init()
{
    if (mode_ == Mode::Watchdog) {
        std::cout << "KatHub Watchdog starting..." << std::endl;
        watchdogStartChild();
        return;
    }

    if (mode_ == Mode::Server) {
        std::cout << "KatHub starting in Server mode..." << std::endl;

        // Create PluginLoader (loads dynamic plugins).
        pluginLoader_ = std::make_unique<PluginLoader>(PluginRegistry::instance());

        // Create SignalHub for WebSocket integration.
        signalHub_ = std::make_unique<KatHub::SignalHub>();

        // Wire SignalHub into HostApi for plugin access.
        hostApi_->eventBus = signalHub_.get();

        // Wire SignalHub into PluginRegistry for lifecycle events.
        PluginRegistry::instance().setSignalHub(signalHub_.get());

        // Create the WebSocket server, connected to SignalHub.
        wsServer_ = std::make_unique<WsServer>(signalHub_.get());
        wsServer_->start(static_cast<quint16>(wsPort_));

        // Create the HTTP server and wire it to SignalHub.
        httpServer_ = std::make_unique<HttpServer>();
        httpServer_->setSignalHub(signalHub_.get());

        // --- AI Subsystem ---
        // Create PromptManager with base directory for templates.
        // Resolve prompts path relative to executable dir.
        {
            QDir promptsDir(QCoreApplication::applicationDirPath());
            promptsDir.cdUp(); // Debug
            promptsDir.cdUp(); // backend
            promptsDir.cdUp(); // build
            QString promptsPath = promptsDir.absoluteFilePath(
                QStringLiteral("backend/prompts/templates"));
            promptManager_ = std::make_unique<KatHub::PromptManager>(promptsPath);
        }

        // Create AIController — the bridge between handlers and AI backends.
        // AIService is NOT set here (requires API key), but handlers can
        // still work — ChatHandler returns an error if AIService is missing.
        aiController_ = std::make_unique<KatHub::AIController>();
        aiController_->setSignalHub(signalHub_.get());

        // Set a default system prompt on the conversation.
        aiController_->setSystemPrompt(
            QStringLiteral("You are a helpful assistant."));

        // Register built-in handlers registered via REGISTER_HANDLER macro.
        const auto &staticHandlers = StaticHandlerRegistry::instance().handlers();
        for (auto *handler : staticHandlers) {
            // If the handler is a StatusHandler, give it the SignalHub.
            if (auto *sh = dynamic_cast<StatusHandler *>(handler)) {
                sh->setSignalHub(signalHub_.get());
            }
            // If the handler is a WsStatusHandler, give it the WsServer.
            if (auto *wsh = dynamic_cast<WsStatusHandler *>(handler)) {
                wsh->setWsServer(wsServer_.get());
            }
            // If the handler is a ChatHandler, inject AI dependencies.
            if (auto *ch = dynamic_cast<ChatHandler *>(handler)) {
                ch->setAIController(aiController_.get());
                ch->setPromptManager(promptManager_.get());
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
        std::cout << "Listening on :" << port_ << " (HTTP), :" << wsPort_ << " (WebSocket)" << std::endl;

        // Mount static files (Vue 3 frontend built into backend/static/).
        // Compute path relative to the executable: build/backend/Debug/ → project root → backend/static/
        {
            QString exeDir = QCoreApplication::applicationDirPath();
            // exeDir = .../build/backend/Debug → go up 3 levels to project root
            QDir dir(exeDir);
            dir.cdUp(); // Debug
            dir.cdUp(); // backend
            dir.cdUp(); // build
            QString staticPath = dir.absoluteFilePath(QStringLiteral("backend/static"));
            httpServer_->mountStaticDir(staticPath.toStdString(), "/");
            std::cout << "Serving static files from: " << staticPath.toStdString() << std::endl;
        }

        httpServer_->start(port_);

        // Emit system.ready event — signals that KatHub has fully started.
        {
            QJsonObject readyPayload;
            readyPayload[QStringLiteral("port")] = port_;
            readyPayload[QStringLiteral("version")] = QStringLiteral("0.1.0");
            signalHub_->publish(QStringLiteral("system.ready"), readyPayload);
        }

    } else {
        std::cout << "KatHub starting in Hand mode..." << std::endl;

        // Create the event bus.
        signalHub_ = std::make_unique<KatHub::SignalHub>();

        // Wire SignalHub into HostApi for plugin access.
        hostApi_->eventBus = signalHub_.get();

        // Create HTTP server so WebEngine can connect to localhost.
        httpServer_ = std::make_unique<HttpServer>();
        httpServer_->setSignalHub(signalHub_.get());

        // Register built-in handlers (StatusHandler, StaticFileHandler, etc.).
        const auto &staticHandlers = StaticHandlerRegistry::instance().handlers();
        for (auto *handler : staticHandlers) {
            if (auto *sh = dynamic_cast<StatusHandler *>(handler)) {
                sh->setSignalHub(signalHub_.get());
            }
            httpServer_->registerHandler(handler);
        }

        // Mount static files (Vue 3 frontend).
        {
            QString exeDir = QCoreApplication::applicationDirPath();
            QDir dir(exeDir);
            dir.cdUp(); // Debug
            dir.cdUp(); // backend
            dir.cdUp(); // build
            QString staticPath = dir.absoluteFilePath(QStringLiteral("backend/static"));
            httpServer_->mountStaticDir(staticPath.toStdString(), "/");
            std::cout << "Serving static files from: " << staticPath.toStdString() << std::endl;
        }

        httpServer_->start(port_);
        std::cout << "Listening on :" << port_ << " (HTTP)" << std::endl;

        // Create HandWindow widget — frameless QMainWindow with QWebEngineView.
        const QString handUrl =
            QStringLiteral("http://%1:%2").arg(handHost_).arg(port_);
        handWindow_ = std::make_unique<KatHub::HandWindow>(QUrl(handUrl), port_);
        handWindow_->show();

        std::cout << "HandWindow connecting to " << handUrl.toStdString()
                  << std::endl;

        // Quit from tray: stop server first, then exit cleanly.
        QObject::connect(handWindow_.get(), &KatHub::HandWindow::quitRequested,
                [this]() {
                    requestShutdown();
                    QCoreApplication::quit();
                });

        // Wire to EventBus: receive navigation commands.
        signalHub_->subscribe(QStringLiteral("navigate.to"),
            [this](const QJsonObject &data) {
                if (data.contains(QStringLiteral("url"))) {
                    const QString url = data[QStringLiteral("url")].toString();
                    if (!url.isEmpty() && handWindow_) {
                        handWindow_->loadUrl(QUrl(url));
                    }
                }
            });

        // Wire to EventBus: receive JavaScript execution commands.
        signalHub_->subscribe(QStringLiteral("webengine.executeJs"),
            [this](const QJsonObject &data) {
                if (data.contains(QStringLiteral("js")) && handWindow_) {
                    handWindow_->executeJavaScript(
                        data[QStringLiteral("js")].toString());
                }
            });

        // Emit system.ready event.
        {
            QJsonObject readyPayload;
            readyPayload[QStringLiteral("mode")] = QStringLiteral("hand");
            readyPayload[QStringLiteral("version")] = QStringLiteral("0.1.0");
            signalHub_->publish(QStringLiteral("system.ready"), readyPayload);
        }
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

int KatHubApp::wsPort() const
{
    return wsPort_;
}

QCoreApplication &KatHubApp::app()
{
    return *app_;
}

// ============================================================================
//  Watchdog
// ============================================================================

void KatHubApp::watchdogStartChild()
{
    if (watchdogRestarts_ >= WATCHDOG_MAX_RESTARTS) {
        std::cerr << "[WATCHDOG] Max restarts (" << WATCHDOG_MAX_RESTARTS
                  << ") reached. Giving up." << std::endl;
        QCoreApplication::quit();
        return;
    }

    // Find our own executable path
    QString exePath = QCoreApplication::applicationFilePath();

    std::cout << "[WATCHDOG] Starting child: " << exePath.toStdString()
              << " --server --port " << port_
              << " --ws-port " << wsPort_ << std::endl;

    if (!watchdogChild_) {
        watchdogChild_ = std::make_unique<QProcess>();
        QObject::connect(watchdogChild_.get(),
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            [this](int exitCode, QProcess::ExitStatus status) {
                watchdogOnChildFinished(exitCode, status);
            });
    }

    QStringList args;
    args << QStringLiteral("--server")
         << QStringLiteral("--port") << QString::number(port_)
         << QStringLiteral("--ws-port") << QString::number(wsPort_);

    watchdogChild_->start(exePath, args);
}

void KatHubApp::watchdogOnChildFinished(int exitCode, int exitStatus)
{
    bool crashed = (static_cast<QProcess::ExitStatus>(exitStatus) == QProcess::CrashExit);

    if (crashed) {
        std::cerr << "[WATCHDOG] Child CRASHED (exit code " << exitCode
                  << "). Restart #" << (watchdogRestarts_ + 1) << "..." << std::endl;

        // Read panic log if available.
        std::string panicLog = KatHub::CrashHandler::readPanicLog();
        if (!panicLog.empty()) {
            std::cerr << "[WATCHDOG] Crash report:\n" << panicLog << std::endl;
        }
    } else {
        std::cout << "[WATCHDOG] Child exited normally (code " << exitCode
                  << ")." << std::endl;
    }

    if (exitCode == 0) {
        // Clean exit — don't restart.
        std::cout << "[WATCHDOG] Child exited cleanly. Watchdog done." << std::endl;
        QCoreApplication::quit();
        return;
    }

    // Restart with backoff.
    ++watchdogRestarts_;
    int backoff = WATCHDOG_BACKOFF_SEC * watchdogRestarts_;

    std::cout << "[WATCHDOG] Restarting in " << backoff << " seconds..." << std::endl;

    QTimer::singleShot(backoff * 1000, [this]() {
        watchdogStartChild();
    });
}
