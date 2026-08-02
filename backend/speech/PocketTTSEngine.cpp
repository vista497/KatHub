#include "PocketTTSEngine.h"
#include "pocket_tts_api.h"

#include <QDebug>
#include <QFileInfo>
#include <QThread>
#include <QtConcurrent/QtConcurrent>

// ═════════════════════════════════════════════════════════════════════════════

PocketTTSEngine::PocketTTSEngine(QObject *parent) : QObject(parent) {}

PocketTTSEngine::~PocketTTSEngine()
{
    stop();
    m_tts.reset();
    m_config.reset();
}

bool PocketTTSEngine::initialize(const QString &modelsDir, const QString &tokenizerPath)
{
    if (m_initialized) return true;

    QFileInfo dirInfo(modelsDir);
    if (!dirInfo.exists() || !dirInfo.isDir()) {
        emit errorOccurred(QStringLiteral("Папка моделей не найдена: %1").arg(modelsDir));
        return false;
    }

    try {
        auto cfg = new pocket_tts::Config;
        cfg->models_dir       = modelsDir.toStdString();
        cfg->tokenizer_path   = tokenizerPath.isEmpty()
            ? (modelsDir + "/tokenizer.model").toStdString()
            : tokenizerPath.toStdString();
        cfg->voices_dir       = (modelsDir + "/voices").toStdString();
        cfg->precision        = "int8";
        cfg->temperature      = 0.7f;
        cfg->num_threads      = 0;
        cfg->verbose          = false;
        cfg->lsd_steps        = 1;

        m_config.reset(cfg);

        // Загружает 5 ONNX-моделей: mimi_encoder, text_conditioner,
        // flow_lm_main_int8, flow_lm_flow_int8, mimi_decoder_int8
        m_tts.reset(new pocket_tts::PocketTTS(*cfg));

        m_initialized = true;
        qDebug() << "[PocketTTSEngine] Инициализирован, модели:" << modelsDir;
        return true;

    } catch (const std::exception &e) {
        emit errorOccurred(QStringLiteral("Ошибка инициализации PocketTTS: %1").arg(e.what()));
        return false;
    } catch (...) {
        emit errorOccurred(QStringLiteral("Неизвестная ошибка инициализации PocketTTS"));
        return false;
    }
}

bool PocketTTSEngine::loadSpeaker(const QString &audioPath)
{
    if (!m_initialized || !m_tts) {
        emit errorOccurred(QStringLiteral("PocketTTS не инициализирован"));
        return false;
    }

    QFileInfo fi(audioPath);
    if (!fi.exists()) {
        emit errorOccurred(QStringLiteral("Аудиофайл не найден: %1").arg(audioPath));
        return false;
    }

    try {
        // load_audio читает WAV/MP3/FLAC → PCM float32 24kHz
        auto audio = pocket_tts::PocketTTS::load_audio(audioPath.toStdString());
        if (audio.samples.empty()) {
            emit errorOccurred(QStringLiteral("Не удалось загрузить аудио: %1").arg(audioPath));
            return false;
        }

        m_speakerPath = audioPath;
        m_speakerEmbedding = std::move(audio.samples);
        m_hasSpeaker = true;

        qDebug() << "[PocketTTSEngine] Голос загружен:" << audioPath
                 << "|" << audio.duration_sec() << "с";
        return true;

    } catch (const std::exception &e) {
        emit errorOccurred(QStringLiteral("Ошибка загрузки голоса: %1").arg(e.what()));
        return false;
    }
}

PocketTTSEngine::AudioResult PocketTTSEngine::synthesize(const QString &text)
{
    AudioResult result;
    if (!m_initialized || !m_tts || !m_hasSpeaker || text.isEmpty()) {
        return result;
    }

    m_synthesizing = true;
    m_stopRequested = false;

    try {
        std::vector<float> allSamples;
        std::string voicePath = m_speakerPath.toStdString();

        m_tts->stream(
            text.toStdString(),
            voicePath,
            [&](const float* samples, size_t count) -> bool {
                if (m_stopRequested) return false;
                allSamples.insert(allSamples.end(), samples, samples + count);
                return true;
            }
        );

        result.samples    = std::move(allSamples);
        result.sampleRate = pocket_tts::PocketTTS::SR;

    } catch (const std::exception &e) {
        qWarning() << "[PocketTTSEngine] Ошибка синтеза:" << e.what();
        emit errorOccurred(QStringLiteral("Ошибка синтеза: %1").arg(e.what()));
    }

    m_synthesizing = false;
    return result;
}

void PocketTTSEngine::synthesizeStreaming(const QString &text)
{
    if (!m_initialized || !m_tts || !m_hasSpeaker || text.isEmpty()) {
        return;
    }

    if (m_synthesizing) stop();

    m_synthesizing  = true;
    m_stopRequested = false;

    QtConcurrent::run([this, text]() {
        try {
            std::string voicePath = m_speakerPath.toStdString();
            m_tts->stream(
                text.toStdString(),
                voicePath,
                [this](const float* samples, size_t count) -> bool {
                    if (m_stopRequested) return false;
                    QByteArray chunk(reinterpret_cast<const char*>(samples),
                                     count * sizeof(float));
                    emit audioChunk(chunk, pocket_tts::PocketTTS::SR);
                    return true;
                }
            );
        } catch (const std::exception &e) {
            emit errorOccurred(QStringLiteral("Ошибка потокового синтеза: %1").arg(e.what()));
        }

        m_synthesizing = false;
        emit streamingFinished();
    });
}

void PocketTTSEngine::stop()
{
    m_stopRequested = true;
    for (int i = 0; i < 20 && m_synthesizing; ++i) {
        QThread::msleep(100);
    }
}
