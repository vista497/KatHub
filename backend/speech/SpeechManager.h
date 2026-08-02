#pragma once

#include <QObject>
#include "TTSWrapper.h"
#include "WhisperWrapper.h"
#include <memory>

class SpeechManager : public QObject
{
    Q_OBJECT
public:
    explicit SpeechManager(QObject *parent = nullptr);
    ~SpeechManager() override;

    void StartTTS(const QString &text);
    void connectTTSSignals();  // Подключить сигналы TTS (вызывать после создания MainWindow)
    WhisperWrapper *GetWhisperWrapper();
    TTSWrapper *GetTTSWrapper();

    /// Включить/выключить речевой слой (STT + TTS).
    void setEnabled(bool enabled);
    bool isEnabled() const;

    /// Запустить распознавание микрофона (асинхронно поднимает STT-сервер).
    bool startSpeech();
    /// Остановить распознавание микрофона.
    void stopSpeech();

    /// Запустить буферизованный захват: микрофон пишет в кольцевой буфер,
    /// в STT ничего не отправляется до явного вызова transcribeLastSeconds().
    bool startBufferedCapture();
    /// Остановить буферизованный захват.
    void stopBufferedCapture();

    /// Распознать последние N секунд из кольцевого буфера (синхронно).
    /// Пустая строка — ошибка/таймаут/пустой буфер.
    QString transcribeLastSeconds(int seconds);

signals:
    void speechRecognized(const QString &text);
    void speechRecognizedWithTimestamp(const QString &text, qint64 timestampMs);
    void speechError(const QString &error);
    void ttsPlaybackStarted();
    void ttsPlaybackFinished();
    /// Результат распознавания окна из буфера (текст + длительность окна, сек)
    void bufferedTranscriptionReady(const QString &text, int windowSeconds);

private:
    std::unique_ptr<TTSWrapper> m_tts;
    std::unique_ptr<WhisperWrapper> m_whisper;
    bool m_enabled;    // Речевой слой включён пользователем
    bool m_ttsPlaying; // TTS сейчас воспроизводит — игнорируем распознавание

private slots:
    void onTranscriptReceived(const QString &text);
    void onTranscriptReceivedWithTimestamp(const QString &text, qint64 timestampMs);
};
