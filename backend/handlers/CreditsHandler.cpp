#include "CreditsHandler.h"

#include "httplib.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

#include <iostream>
#include <string>

CreditsHandler::HttpMethod CreditsHandler::method()
{
    return HttpMethod::GET;
}

const char* CreditsHandler::route()
{
    return "/api/credits";
}

// ---------------------------------------------------------------------------
// Load the RouterAI secret token from .env (same lookup as KatHubApp.cpp).
// Priority: <exe_dir>/.env, then %HOME%/AppData/Local/hermes/.env.
// ---------------------------------------------------------------------------
std::string CreditsHandler::loadRouterAiKey()
{
    QStringList envPaths;
    const QString exeDir = QCoreApplication::applicationDirPath();
    envPaths << QDir(exeDir).absoluteFilePath(".env");
    envPaths << QDir(QDir::homePath()).absoluteFilePath("AppData/Local/hermes/.env");

    for (const QString& path : envPaths) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;
        while (!f.atEnd()) {
            QString line = QString::fromUtf8(f.readLine()).trimmed();
            if (line.startsWith(QLatin1String("HERMES_CUSTOM_ROUTERAI_RU_API_KEY="))) {
                return line.mid(34).trimmed().toStdString();  // len("HERMES_CUSTOM_ROUTERAI_RU_API_KEY=") == 34
            }
        }
    }
    return {};
}

// ---------------------------------------------------------------------------
// GET /api/credits → {"credits": <number>}
// Uses QNetworkAccessManager (Qt ships its own TLS backends), so HTTPS works
// even though cpp-httplib in this project is built without OpenSSL support.
// On failure → 502 with {"error": ...}.
// ---------------------------------------------------------------------------
namespace {

void handleImpl(httplib::Response* res)
{
    const std::string token = CreditsHandler::loadRouterAiKey();
    if (token.empty()) {
        res->status = 502;
        res->set_content("{\"error\":\"RouterAI API key not found in .env (HERMES_CUSTOM_ROUTERAI_RU_API_KEY)\"}", "application/json; charset=utf-8");
        return;
    }

    QNetworkAccessManager manager;
    // RouterAI is reachable directly; system proxy (WinHTTP/WinINET from
    // registry) would answer 401 "Host requires authentication" here.
    manager.setProxy(QNetworkProxy::NoProxy);

    QNetworkRequest req(QUrl("https://routerai.ru/api/v1/credits"));
    req.setRawHeader("Authorization", QByteArray("Bearer ") + QByteArray::fromStdString(token));
    req.setRawHeader("Accept", "application/json");

    QNetworkReply* reply = manager.get(req);

    // Synchronous wait (this handler runs on the HTTP server's worker thread;
    // a nested event loop is fine here — Qt6Network needs one to emit signals).
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer::singleShot(15000, &loop, &QEventLoop::quit);  // hard timeout 15s
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        const std::string errText = reply->errorString().toStdString();
        res->status = 502;
        res->set_content("{\"error\":\"RouterAI credits request failed: " + errText + "\"}", "application/json; charset=utf-8");
        reply->deleteLater();
        return;
    }

    const QByteArray body = reply->readAll();
    reply->deleteLater();

    // Parse {"data":{"credits": <number>}} → respond {"credits": <number>}
    // (frontend DashboardView reads creditsResp.credits directly).
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(body, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        res->status = 502;
        res->set_content("{\"error\":\"RouterAI credits: invalid JSON response\"}", "application/json; charset=utf-8");
        return;
    }

    QJsonObject out;
    const QJsonObject dataObj = doc.object().value("data").toObject();
    if (dataObj.contains("credits"))
        out.insert("credits", dataObj.value("credits"));
    else
        out = doc.object();  // pass through unknown shape as-is

    res->set_content(QJsonDocument(out).toJson(QJsonDocument::Compact).toStdString(),
                     "application/json; charset=utf-8");
}

} // namespace

void CreditsHandler::handle(const char*, void* response)
{
    auto* res = static_cast<httplib::Response*>(response);

    try {
        handleImpl(res);
    } catch (const std::exception& e) {
        std::cerr << "[Credits] EXCEPTION: " << e.what() << std::endl;
        res->status = 502;
        res->set_content("{\"error\":\"credits handler exception: " + std::string(e.what()) + "\"}",
                         "application/json; charset=utf-8");
    } catch (...) {
        std::cerr << "[Credits] UNKNOWN EXCEPTION" << std::endl;
        res->status = 502;
        res->set_content("{\"error\":\"credits handler unknown exception\"}",
                         "application/json; charset=utf-8");
    }
}

