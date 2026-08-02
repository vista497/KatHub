#pragma once

#include <QObject>
#include <QString>
#include <QByteArray>
#include <memory>
#include <vector>
#include <atomic>

namespace pocket_tts {
    struct Config;
    struct AudioData;
    class PocketTTS;
}

/**
 * @brief Обёртка над PocketTTS.cpp — нативный C++ TTS с клонированием голоса
 *
 * Использует ONNX Runtime. Модели (~500 MB) загружаются один раз при initialize().
 * Голос клонируется из аудиофайла через loadSpeaker().
 * Синтез — синхронный (synthesize) или потоковый (synthesizeStreaming → signal audioChunk).
 */
class PocketTTSEngine : public QObject
{
    Q_OBJECT

public:
    explicit PocketTTSEngine(QObject *parent = nullptr);
    ~PocketTTSEngine();

    /// Загружает ONNX-модели.
    /// @param modelsDir  папка с mimi_encoder.onnx, text_conditioner.onnx и т.д.
    /// @param tokenizerPath  путь к tokenizer.model (по умолчанию modelsDir/tokenizer.model)
    bool initialize(const QString &modelsDir, const QString &tokenizerPath = {});

    /// Клонирует голос из аудиофайла (WAV/MP3/FLAC, короткий образец)
    /// @return true если голос успешно извлечён
    bool loadSpeaker(const QString &audioPath);

    /// Есть ли загруженный голос
    bool hasSpeaker() const { return m_hasSpeaker; }

    /// Результат синтеза — PCM float32 24kHz mono
    struct AudioResult {
        std::vector<float> samples;
        int sampleRate = 24000;
        float durationSec() const { return samples.size() / float(sampleRate); }
    };

    /// Синхронный синтез речи. Блокирует поток до завершения.
    AudioResult synthesize(const QString &text);

    /// Потоковый синтез — сигнал audioChunk() на каждый кусок аудио.
    /// Не блокирует вызывающий поток (запускается в QThread).
    void synthesizeStreaming(const QString &text);

    /// Остановить текущий синтез
    void stop();

    bool isInitialized() const { return m_initialized; }
    bool isSynthesizing() const { return m_synthesizing; }

signals:
    /// Кусок PCM-аудио (float32, 24kHz, mono) при потоковом синтезе
    void audioChunk(QByteArray pcmData, int sampleRate);
    /// Потоковый синтез завершён
    void streamingFinished();
    /// Ошибка
    void errorOccurred(const QString &error);

private:
    bool m_initialized = false;
    bool m_hasSpeaker = false;
    std::atomic<bool> m_synthesizing{false};
    std::atomic<bool> m_stopRequested{false};

    std::unique_ptr<pocket_tts::Config> m_config;
    std::unique_ptr<pocket_tts::PocketTTS> m_tts;

    QString m_speakerPath;                 // путь к аудио-образцу голоса
    std::vector<float> m_speakerEmbedding; // кеш embedding'а для быстрого синтеза
};
