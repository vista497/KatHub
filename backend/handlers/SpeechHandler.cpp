#include "SpeechHandler.h"

#include "../speech/SpeechManager.h"
#include "../speech/WhisperWrapper.h"

#include <httplib.h>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

SpeechHandler::SpeechHandler() = default;

const char* SpeechHandler::route()
{
    return "/api/speech";
}

IHttpHandler::HttpMethod SpeechHandler::method()
{
    return HttpMethod::POST;
}

void SpeechHandler::handle(const char* request, void* response)
{
    handleWithContext(request, "/api/speech", nullptr, response, "POST");
}

void SpeechHandler::handleWithContext(const char* body, const char* path,
                                      const char* query, void* response,
                                      const char* method)
{
    auto* res = static_cast<httplib::Response*>(response);

    if (!speechManager_) {
        res->status = 503;
        res->set_content(R"({"error":"Speech layer not available"})",
                         "application/json");
        return;
    }

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(
        QByteArray::fromRawData(body, static_cast<int>(std::char_traits<char>::length(body))),
        &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        res->status = 400;
        res->set_content(R"({"error":"Invalid JSON"})", "application/json");
        return;
    }

    const QJsonObject req = doc.object();
    const QString command = req.value(QStringLiteral("command")).toString();

    if (command == QStringLiteral("capture")) {
        const int seconds = req.value(QStringLiteral("seconds")).toInt(30);
        WhisperWrapper *w = speechManager_->GetWhisperWrapper();
        const QString text = speechManager_->transcribeLastSeconds(seconds);
        QJsonObject out;
        out[QStringLiteral("text")] = text;
        out[QStringLiteral("seconds")] = seconds;
        if (w) {
            out[QStringLiteral("bufferSeconds")] = w->bufferSeconds();
            out[QStringLiteral("initialized")] = w->isInitialized();
            out[QStringLiteral("connected")] = w->isConnected();
        }
        res->set_content(QJsonDocument(out).toJson(QJsonDocument::Compact)
                             .toStdString(),
                         "application/json; charset=utf-8");
        return;
    }

    if (command == QStringLiteral("ptt")) {
        const bool pressed = req.value(QStringLiteral("pressed")).toBool(false);
        if (pressed) {
            speechManager_->startSpeech();
        } else {
            speechManager_->stopSpeech();
        }
        QJsonObject out;
        out[QStringLiteral("ok")] = true;
        res->set_content(QJsonDocument(out).toJson(QJsonDocument::Compact)
                             .toStdString(),
                         "application/json; charset=utf-8");
        return;
    }

    if (command == QStringLiteral("speak")) {
        const QString text = req.value(QStringLiteral("text")).toString();
        if (text.isEmpty()) {
            res->status = 400;
            res->set_content(R"({"error":"Missing text"})", "application/json");
            return;
        }
        speechManager_->StartTTS(text);
        QJsonObject out;
        out[QStringLiteral("ok")] = true;
        res->set_content(QJsonDocument(out).toJson(QJsonDocument::Compact)
                             .toStdString(),
                         "application/json; charset=utf-8");
        return;
    }

    if (command == QStringLiteral("status")) {
        QJsonObject out;
        out[QStringLiteral("enabled")] = speechManager_->isEnabled();
        WhisperWrapper *w = speechManager_->GetWhisperWrapper();
        out[QStringLiteral("streaming")] =
            w ? w->streamingEnabled() : false;
        if (w) {
            out[QStringLiteral("bufferSeconds")] = w->bufferSeconds();
            out[QStringLiteral("initialized")] = w->isInitialized();
            out[QStringLiteral("connected")] = w->isConnected();
            out[QStringLiteral("lastText")] = w->lastTranscription();
        }
        res->set_content(QJsonDocument(out).toJson(QJsonDocument::Compact)
                             .toStdString(),
                         "application/json; charset=utf-8");
        return;
    }

    res->status = 400;
    res->set_content(R"({"error":"Unknown command"})", "application/json");
}
