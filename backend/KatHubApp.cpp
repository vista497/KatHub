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
#include "SetupWizard.h"
#include "VaultGraphHandler.h"
#include "VaultFileHandler.h"
#include "VaultSessionsHandler.h"
#include "WsServer.h"
#include "WsStatusHandler.h"
#include "ChatHandler.h"
#include "HermesApiClient.h"
#include "HermesCliHelper.h"
#include "HermesSessionsHandler.h"
#include "ModelsHandler.h"
#include "SystemHandler.h"
#include "HealthHandler.h"
#include "CreditsHandler.h"
#include "AgentsHandler.h"
#include "AgentChatHandler.h"
#include "CronHandler.h"
#include "SkillsHandler.h"
#include "KanbanHandler.h"
#include "speech/SpeechManager.h"
#include "handlers/SpeechHandler.h"
#include "httplib.h"
#include "ai/AIController.h"
#include "ai/Conversation.h"
#include "ai/ToolDispatcher.h"
#include "prompts/PromptManager.h"
#include "prompts/AgentProfile.h"
#include "core/CrashHandler.h"
#include "core/Logger.h"

#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>

#include <atomic>
#include <thread>

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTextStream>
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
            // Log to file only, avoid spamming stderr with non-fatal exceptions
            KatHub::CrashHandler::clearPanicFlag();
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

    // Auto-detect Tailscale hostname if running in Hand mode with default host
    if (mode_ == Mode::Hand && handHost_ == QStringLiteral("localhost")) {
        QProcess ts;
        ts.start(QStringLiteral("tailscale"), {QStringLiteral("status"), QStringLiteral("--json")});
        if (ts.waitForFinished(3000) && ts.exitCode() == 0) {
            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(ts.readAllStandardOutput(), &err);
            if (err.error == QJsonParseError::NoError && doc.isObject()) {
                QJsonObject self = doc.object().value(QStringLiteral("Self")).toObject();
                QString hostName = self.value(QStringLiteral("HostName")).toString();
                if (!hostName.isEmpty()) {
                    handHost_ = hostName;
                    std::cout << "Tailscale hostname detected: "
                              << hostName.toStdString() << std::endl;
                }
            }
        }
        // Silently keep "localhost" on failure — still works locally
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
    QString hermesProfile = QStringLiteral("default");

    if (mode_ == Mode::Watchdog) {
        std::cout << "KatHub Watchdog starting..." << std::endl;
        watchdogStartChild();
        return;
    }

    if (mode_ == Mode::Server) {
        std::cout << "KatHub starting in Server mode..." << std::endl;

        // File logging: kathub-backend.log рядом с exe — логи отправки
        // сообщений ([ChatSend]) и ошибки можно анализировать из файла,
        // не полагаясь на UI/консоль.
        {
            QString logPath = QCoreApplication::applicationDirPath()
                              + QStringLiteral("/kathub-backend.log");
            KatHub::Logger::instance().init(KatHub::LogLevel::Debug,
                                            logPath.toStdString());
            LOG_INFO("[ChatSend] log file initialized: "
                     + logPath.toStdString());
        }

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
        // Directly instantiate VaultGraphHandler (no QObject — REGISTER_HANDLER
        // may not force-link under MSVC).
        {
            auto *vh = new VaultGraphHandler();
            vh->setVaultPath("C:/Users/User/n8n_memory/Memory/Katty_ai");
            httpServer_->registerHandler(vh);
            std::cout << "Registered handler: " << vh->route() << std::endl;
        }
        {
            auto *fh = new VaultFileHandler();
            fh->setVaultPath("C:/Users/User/n8n_memory/Memory/Katty_ai");
            httpServer_->registerHandler(fh);
            std::cout << "Registered handler: " << fh->route() << std::endl;
        }
        {
            auto *sh2 = new VaultSessionsHandler();
            sh2->setVaultPath("C:/Users/User/n8n_memory/Memory/Katty_ai");
            httpServer_->registerHandler(sh2);
            std::cout << "Registered handler: " << sh2->route() << std::endl;
        }

        // ── Hermes Agent API client ──────────────────────────────
        // Read API key and host from .env file (same as Hermes uses).
        // Try standard Hermes path first, then application directory.
        {
            QString apiKey;
            QString hermesHost = QStringLiteral("http://127.0.0.1:8642");
            QStringList envPaths = {
                QDir(QDir::homePath()).absoluteFilePath("AppData/Local/hermes/.env"),
                QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(".env"),
            };
            for (const auto &p : envPaths) {
                QFile f(p);
                if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    QTextStream in(&f);
                    while (!in.atEnd()) {
                        QString line = in.readLine().trimmed();
                        if (line.startsWith("API_SERVER_KEY=")) {
                            apiKey = line.mid(15);
                        } else if (line.startsWith("HERMES_HOST=")) {
                            hermesHost = QStringLiteral("http://") + line.mid(12);
                            std::cout << "Using Hermes host from .env: " << hermesHost.toStdString() << std::endl;
                        } else if (line.startsWith("HERMES_PROFILE=")) {
                            hermesProfile = line.mid(14);
                        }
                    }
                    f.close();
                    if (!apiKey.isEmpty()) break;
                }
            }
            if (apiKey.isEmpty()) {
                std::cerr << "WARNING: Hermes API key not found in .env (tried: "
                          << envPaths.join(", ").toStdString() << ")" << std::endl;
            }
            hermesApi_ = std::make_shared<HermesApiClient>(
                hermesHost.toStdString(), apiKey.toStdString());
            std::cout << "Hermes API client created (alive="
                      << (hermesApi_->isAlive() ? "yes" : "no") << ")" << std::endl;
        }

        // ── Hermes Sessions handler ─────────────────────────────
        {
            auto *hsh = new HermesSessionsHandler();
            hsh->setApiClient(hermesApi_);
            httpServer_->registerHandler(hsh);
            std::cout << "Registered handler: " << hsh->route() << std::endl;
        }

        // DELETE route for session removal — via hermes CLI
        {
            httpServer_->server().Delete(R"(/api/hermes/sessions/(.*))",
                [](const httplib::Request &req, httplib::Response &res) {
                    std::string sid = req.matches[1];
                    auto r = HermesCliHelper::runArgv(
                        {"sessions", "delete", sid, "--yes"}, 30000);
                    QJsonObject out;
                    if (r.exitCode != 0) {
                        out["error"] = QString::fromStdString(
                            !r.stderrText.empty() ? r.stderrText
                                                  : "hermes sessions delete failed");
                        res.status = 502;
                    } else {
                        out["deleted"] = true;
                        out["sessionId"] = QString::fromStdString(sid);
                        res.status = 200;
                    }
                    res.set_content(QJsonDocument(out).toJson(QJsonDocument::Compact).toStdString(),
                                    "application/json; charset=utf-8");
                });
        }

        // ── Models handler ─────────────────────────────────────
        {
            auto *mh = new ModelsHandler();
            httpServer_->registerHandler(mh);
            std::cout << "Registered handler: " << mh->route() << std::endl;
        }

        // ── System handler ─────────────────────────────────────
        {
            auto *sh = new SystemHandler();
            sh->setApiClient(hermesApi_);
            sh->setStartTime(httpServer_->startTime());
            sh->setPorts(port_, wsPort_);
            httpServer_->registerHandler(sh);
            std::cout << "Registered handler: " << sh->route() << std::endl;
        }

        // ── Health handler ─────────────────────────────────────
        {
            auto *hh = new HealthHandler();
            httpServer_->registerHandler(hh);
            std::cout << "Registered handler: " << hh->route() << std::endl;
        }

        // ── Credits handler ────────────────────────────────────
        {
            auto *ch = new CreditsHandler();
            httpServer_->registerHandler(ch);
            std::cout << "Registered handler: " << ch->route() << std::endl;
        }

        // ── Agents handler ─────────────────────────────────────
        {
            auto *ah = new AgentsHandler();
            httpServer_->registerHandler(ah);
            std::cout << "Registered handler: " << ah->route() << std::endl;
        }

        // ── Agent chat handler (separate chats per agent) ──────
        {
            auto *ach = new AgentChatHandler();
            httpServer_->registerHandler(ach);
            std::cout << "Registered handler: " << ach->route() << std::endl;
        }

        // ── Cron handler ───────────────────────────────────────
        {
            auto *ch = new CronHandler();
            httpServer_->registerHandler(ch);
            std::cout << "Registered handler: " << ch->route() << std::endl;
        }

        // ── Skills handler ─────────────────────────────────────
        {
            auto *skh = new SkillsHandler();
            httpServer_->registerHandler(skh);
            std::cout << "Registered handler: " << skh->route() << std::endl;
        }

        // ── Kanban handler ─────────────────────────────────────
        {
            auto *kh = new KanbanHandler();
            httpServer_->registerHandler(kh);
            std::cout << "Registered handler: " << kh->route() << std::endl;
        }

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
            // If the handler is a ChatHandler, inject Hermes CLI.
            if (auto *ch = dynamic_cast<ChatHandler *>(handler)) {
                ch->setApiClient(hermesApi_);
                ch->setProfile(hermesProfile.toStdString());
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
        // Search upward from the executable until we find backend/static/,
        // or fall back to static/ next to the exe (installed build).
        {
            QDir dir(QCoreApplication::applicationDirPath());
            bool found = false;
            for (int i = 0; i < 5; i++) {
                if (dir.exists(QStringLiteral("backend/static"))) { found = true; break; }
                dir.cdUp();
            }
            QString staticPath = found
                ? dir.absoluteFilePath(QStringLiteral("backend/static"))
                : QCoreApplication::applicationDirPath() + QStringLiteral("/static");
            if (QDir(staticPath).exists()) {
                httpServer_->mountStaticDir(staticPath.toStdString(), "/");
                std::cout << "Serving static files from: " << staticPath.toStdString() << std::endl;
            } else {
                std::cerr << "WARNING: static files not found at " << staticPath.toStdString() << std::endl;
            }
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
        // File logging: тот же kathub-backend.log рядом с exe (см. Server mode).
        {
            QString logPath = QCoreApplication::applicationDirPath()
                              + QStringLiteral("/kathub-backend.log");
            KatHub::Logger::instance().init(KatHub::LogLevel::Debug,
                                            logPath.toStdString());
            LOG_INFO("[ChatSend] log file initialized: "
                     + logPath.toStdString());
        }

        // Create the event bus.
        signalHub_ = std::make_unique<KatHub::SignalHub>();

        // Wire SignalHub into HostApi for plugin access.
        hostApi_->eventBus = signalHub_.get();

        // Create HTTP server so WebEngine can connect to localhost.
        httpServer_ = std::make_unique<HttpServer>();
        httpServer_->setSignalHub(signalHub_.get());

        // ── Hermes Agent API client ──────────────────────────────
        // Read API key and host from .env file.
        // Try standard Hermes path first, then application directory.
        {
            QString apiKey;
            QString hermesHost = QStringLiteral("http://127.0.0.1:8642");
            QStringList envPaths = {
                QDir(QDir::homePath()).absoluteFilePath("AppData/Local/hermes/.env"),
                QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(".env"),
            };
            for (const auto &p : envPaths) {
                QFile f(p);
                if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    QTextStream in(&f);
                    while (!in.atEnd()) {
                        QString line = in.readLine().trimmed();
                        if (line.startsWith("API_SERVER_KEY=")) {
                            apiKey = line.mid(15);
                        } else if (line.startsWith("HERMES_HOST=")) {
                            hermesHost = QStringLiteral("http://") + line.mid(12);
                        } else if (line.startsWith("HERMES_PROFILE=")) {
                            hermesProfile = line.mid(14);
                        }
                    }
                    f.close();
                    if (!apiKey.isEmpty()) break;
                }
            }
            if (apiKey.isEmpty()) {
                std::cerr << "WARNING: Hermes API key not found in .env (tried: "
                          << envPaths.join(", ").toStdString() << ")" << std::endl;
            }
            hermesApi_ = std::make_shared<HermesApiClient>(
                hermesHost.toStdString(), apiKey.toStdString());
            std::cout << "Hermes API client created (alive="
                      << (hermesApi_->isAlive() ? "yes" : "no") << ")" << std::endl;
        }

        // ── Hermes Sessions handler ─────────────────────────────
        {
            auto *hsh = new HermesSessionsHandler();
            hsh->setApiClient(hermesApi_);
            httpServer_->registerHandler(hsh);
            std::cout << "Registered handler: " << hsh->route() << std::endl;
        }

        // DELETE route for session removal — via hermes CLI
        {
            httpServer_->server().Delete(R"(/api/hermes/sessions/(.*))",
                [](const httplib::Request &req, httplib::Response &res) {
                    std::string sid = req.matches[1];
                    auto r = HermesCliHelper::runArgv(
                        {"sessions", "delete", sid, "--yes"}, 30000);
                    QJsonObject out;
                    if (r.exitCode != 0) {
                        out["error"] = QString::fromStdString(
                            !r.stderrText.empty() ? r.stderrText
                                                  : "hermes sessions delete failed");
                        res.status = 502;
                    } else {
                        out["deleted"] = true;
                        out["sessionId"] = QString::fromStdString(sid);
                        res.status = 200;
                    }
                    res.set_content(QJsonDocument(out).toJson(QJsonDocument::Compact).toStdString(),
                                    "application/json; charset=utf-8");
                });
        }

        // ── Models handler ─────────────────────────────────────
        {
            auto *mh = new ModelsHandler();
            httpServer_->registerHandler(mh);
        }

        // ── System handler ─────────────────────────────────────
        {
            auto *sh = new SystemHandler();
            sh->setApiClient(hermesApi_);
            sh->setStartTime(httpServer_->startTime());
            sh->setPorts(port_, wsPort_);
            httpServer_->registerHandler(sh);
        }

        // ── Credits handler ────────────────────────────────────
        {
            auto *ch = new CreditsHandler();
            httpServer_->registerHandler(ch);
        }

        // ── Agents handler ─────────────────────────────────────
        {
            auto *ah = new AgentsHandler();
            httpServer_->registerHandler(ah);
        }

        // ── Cron handler ───────────────────────────────────────
        {
            auto *ch = new CronHandler();
            httpServer_->registerHandler(ch);
        }

        // ── Skills handler ─────────────────────────────────────
        {
            auto *skh = new SkillsHandler();
            httpServer_->registerHandler(skh);
        }

        // ── Kanban handler ─────────────────────────────────────
        {
            auto *kh = new KanbanHandler();
            httpServer_->registerHandler(kh);
        }

        // Register built-in handlers (StatusHandler, StaticFileHandler, etc.).
        // Directly instantiate VaultGraphHandler (no QObject — REGISTER_HANDLER
        // may not force-link under MSVC).
        {
            auto *vh = new VaultGraphHandler();
            vh->setVaultPath("C:/Users/User/n8n_memory/Memory/Katty_ai");
            httpServer_->registerHandler(vh);
        }
        {
            auto *fh = new VaultFileHandler();
            fh->setVaultPath("C:/Users/User/n8n_memory/Memory/Katty_ai");
            httpServer_->registerHandler(fh);
        }
        {
            auto *sh2 = new VaultSessionsHandler();
            sh2->setVaultPath("C:/Users/User/n8n_memory/Memory/Katty_ai");
            httpServer_->registerHandler(sh2);
        }

        const auto &staticHandlers = StaticHandlerRegistry::instance().handlers();
        for (auto *handler : staticHandlers) {
            if (auto *sh = dynamic_cast<StatusHandler *>(handler)) {
                sh->setSignalHub(signalHub_.get());
            }
            if (auto *ch = dynamic_cast<ChatHandler *>(handler)) {
                ch->setApiClient(hermesApi_);
                ch->setProfile(hermesProfile.toStdString());
            }
            httpServer_->registerHandler(handler);
        }

        // Mount static files (Vue 3 frontend).
        // Search upward from the executable until we find backend/static/,
        // or fall back to static/ next to the exe (installed build).
        {
            QDir dir(QCoreApplication::applicationDirPath());
            bool found = false;
            for (int i = 0; i < 5; i++) {
                if (dir.exists(QStringLiteral("backend/static"))) { found = true; break; }
                dir.cdUp();
            }
            QString staticPath = found
                ? dir.absoluteFilePath(QStringLiteral("backend/static"))
                : QCoreApplication::applicationDirPath() + QStringLiteral("/static");
            if (QDir(staticPath).exists()) {
                httpServer_->mountStaticDir(staticPath.toStdString(), "/");
                std::cout << "Serving static files from: " << staticPath.toStdString() << std::endl;
            } else {
                std::cerr << "WARNING: static files not found at " << staticPath.toStdString() << std::endl;
            }
        }

        httpServer_->start(port_);
        std::cout << "Listening on :" << port_ << " (HTTP)" << std::endl;

        // ── First-run setup wizard ───────────────────────────────
        if (!KatHub::SetupWizard::isAlreadyConfigured()) {
            KatHub::SetupWizard wizard;
            wizard.exec();
            if (!wizard.ok()) {
                // User closed — assume they don't want to continue.
                std::cerr << "Setup wizard cancelled, exiting." << std::endl;
                requestShutdown();
                return;
            }
            // If wizard saved an API key, re-read it into the client.
            if (!wizard.apiKey().isEmpty()) {
                hermesApi_->setApiKey(wizard.apiKey().toStdString());
                std::cout << "Hermes API key updated from setup wizard." << std::endl;
            }
        }

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

        // ── Speech layer (STT + TTS) — Hand mode only ─────────────
        {
            speechManager_ = std::make_unique<SpeechManager>();
            speechManager_->setEnabled(true);
            speechManager_->connectTTSSignals();

            // Распознанная речь → фронту (topic: speech.recognized) + wake-word гейт
            QObject::connect(speechManager_.get(),
                &SpeechManager::speechRecognizedWithTimestamp,
                [this](const QString &text, qint64 timestampMs) {
                    onSpeechRecognized(text, timestampMs);
                });
            QObject::connect(speechManager_.get(),
                &SpeechManager::speechError,
                [this](const QString &error) {
                    QJsonObject data;
                    data[QStringLiteral("error")] = error;
                    signalHub_->publish(QStringLiteral("speech.error"), data);
                });

            // TTS-статус → фронту
            QObject::connect(speechManager_.get(),
                &SpeechManager::ttsPlaybackStarted,
                [this]() {
                    signalHub_->publish(QStringLiteral("speech.ttsStarted"),
                                       QJsonObject());
                });
            QObject::connect(speechManager_.get(),
                &SpeechManager::ttsPlaybackFinished,
                [this]() {
                    signalHub_->publish(QStringLiteral("speech.ttsFinished"),
                                       QJsonObject());
                });

            // Команда от фронта: озвучить текст → StartTTS
            signalHub_->subscribe(QStringLiteral("speech.speak"),
                [this](const QJsonObject &data) {
                    const QString text =
                        data.value(QStringLiteral("text")).toString();
                    if (!text.isEmpty() && speechManager_) {
                        speechManager_->StartTTS(text);
                    }
                });

            // Команда от фронта: распознать последние N секунд из буфера
            signalHub_->subscribe(QStringLiteral("speech.capture"),
                [this](const QJsonObject &data) {
                    if (!speechManager_) return;
                    const int seconds =
                        data.value(QStringLiteral("seconds")).toInt(30);
                    const QString text =
                        speechManager_->transcribeLastSeconds(seconds);
                    QJsonObject result;
                    result[QStringLiteral("text")] = text;
                    result[QStringLiteral("seconds")] = seconds;
                    signalHub_->publish(QStringLiteral("speech.captured"),
                                       result);
                });

            // Команда от фронта: PTT — включить/выключить стриминг в STT
            signalHub_->subscribe(QStringLiteral("speech.ptt"),
                [this](const QJsonObject &data) {
                    if (!speechManager_) return;
                    const bool pressed =
                        data.value(QStringLiteral("pressed")).toBool(false);
                    if (pressed) {
                        speechManager_->startSpeech();
                    } else {
                        speechManager_->stopSpeech();
                    }
                });

            // Результат буферизованного распознавания → фронту
            QObject::connect(speechManager_.get(),
                &SpeechManager::bufferedTranscriptionReady,
                [this](const QString &text, int windowSeconds) {
                    QJsonObject data;
                    data[QStringLiteral("text")] = text;
                    data[QStringLiteral("seconds")] = windowSeconds;
                    signalHub_->publish(QStringLiteral("speech.captured"),
                                       data);
                });

            // По умолчанию — буферизованный захват: микрофон пишет в кольцевой
            // буфер, в STT ничего не уходит до команды speech.capture / PTT.
            speechManager_->startBufferedCapture();

            // HTTP-канал для речевых команд (фронт работает HTTP-поллингом,
            // WS в Hand mode не создаётся).
            {
                auto *sph = new SpeechHandler();
                sph->setSpeechManager(speechManager_.get());
                httpServer_->registerHandler(sph);
                std::cout << "Registered handler: " << sph->route() << std::endl;
            }
        }

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

// ============================================================================
// Voice wake-word gate
// ============================================================================
// STT расшифровывает непрерывно (стриминг в WhisperWrapper). Сюда приходит
// каждый распознанный фрагмент. Текст ВСЕГДА публикуется фронту (субтитры),
// но в LLM уходит ТОЛЬКО при наличии ключевого слова (wake word).
void KatHubApp::onSpeechRecognized(const QString &text, qint64 timestampMs)
{
    // 1. Субтитры: распознанный текст всегда виден фронту.
    {
        QJsonObject data;
        data[QStringLiteral("text")] = text;
        data[QStringLiteral("timestamp")] = timestampMs;
        signalHub_->publish(QStringLiteral("speech.recognized"), data);
    }

    // 2. Wake-word гейт: без ключевого слова в LLM НЕ уходит ничего.
    if (voiceLlmBusy_.load() || text.isEmpty()) {
        return;
    }
    if (!text.contains(wakeWord_, Qt::CaseInsensitive)) {
        return;
    }

    // 3. Ключевое слово есть — отправляем в LLM.
    voiceLlmBusy_.store(true);

    if (!hermesApi_) {
        QJsonObject data;
        data[QStringLiteral("error")] = QStringLiteral("API client not configured");
        signalHub_->publish(QStringLiteral("speech.llmError"), data);
        voiceLlmBusy_.store(false);
        return;
    }

    // Voice-сессия: создаём один раз, дальше переиспользуем (контекст диалога).
    if (voiceSessionId_.empty()) {
        std::string createResp = hermesApi_->createSession();
        QJsonParseError cerr;
        QJsonDocument cdoc = QJsonDocument::fromJson(
            QByteArray::fromStdString(createResp), &cerr);
        if (cerr.error == QJsonParseError::NoError && cdoc.isObject()) {
            QJsonObject o = cdoc.object();
            if (o.contains("session_id"))
                voiceSessionId_ = o.value("session_id").toString().toStdString();
            else if (o.contains("session"))
                voiceSessionId_ = o["session"].toObject().value("id").toString().toStdString();
            else if (o.contains("id"))
                voiceSessionId_ = o.value("id").toString().toStdString();
        }
    }

    const std::string sessionId = voiceSessionId_;
    const std::string promptStr = text.toStdString();
    auto api = hermesApi_;

    std::thread([this, api, sessionId, promptStr]() {
        std::string result;
        if (!sessionId.empty()) {
            result = api->chatCompletions(sessionId, promptStr);
        } else {
            result = "ERROR: Failed to create voice session";
        }
        QString qResult = QString::fromStdString(result);
        // KatHubApp не QObject — invokeMethod через qApp (главный поток)
        QMetaObject::invokeMethod(qApp, [this, qResult]() {
            onVoiceLlmReply(qResult);
        }, Qt::QueuedConnection);
    }).detach();
}

// Ответ LLM на голосовой запрос (после ключевого слова).
void KatHubApp::onVoiceLlmReply(const QString &reply)
{
    voiceLlmBusy_.store(false);

    if (reply.isEmpty()) {
        QJsonObject data;
        data[QStringLiteral("error")] = QStringLiteral("Empty LLM reply");
        signalHub_->publish(QStringLiteral("speech.llmError"), data);
        return;
    }

    if (reply.startsWith(QStringLiteral("ERROR:"))) {
        QJsonObject data;
        data[QStringLiteral("error")] = reply.mid(6).trimmed();
        signalHub_->publish(QStringLiteral("speech.llmError"), data);
        return;
    }

    // Ответ фронту + озвучка.
    {
        QJsonObject data;
        data[QStringLiteral("text")] = reply;
        signalHub_->publish(QStringLiteral("speech.llmReply"), data);
    }
    if (speechManager_) {
        speechManager_->StartTTS(reply);
    }
}
