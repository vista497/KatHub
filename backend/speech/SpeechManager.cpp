#include "SpeechManager.h"
#include <QDebug>
#include <QDir>
#include "AppPaths.h"

using namespace KatHub;  // KatHub::AppPaths

SpeechManager::SpeechManager(QObject *parent)
    : QObject(parent)
    , m_enabled(false)
    , m_ttsPlaying(false)
{
    m_tts = std::make_unique<TTSWrapper>();
    m_whisper = std::make_unique<WhisperWrapper>();

    // Подключаем сигнал с timestamp (ОСНОВНОЙ путь)
    connect(m_whisper.get(), &WhisperWrapper::realtimeTranscriptWithTimestamp,
            this, &SpeechManager::onTranscriptReceivedWithTimestamp);

    // Не создаём TTS-wrapper'ы синхронно на старте — оба Python-сервера
    // (STT :5555, TTS :5556) поднимаются асинхронно при первом использовании.
}

SpeechManager::~SpeechManager() = default;

void SpeechManager::setEnabled(bool enabled)
{
    m_enabled = enabled;
    qDebug() << "[SpeechManager] enabled =" << m_enabled;
}

bool SpeechManager::isEnabled() const
{
    return m_enabled;
}

bool SpeechManager::startSpeech()
{
    if (!m_enabled) {
        qWarning() << "[SpeechManager] disabled — startSpeech ignored";
        return false;
    }

    // Асинхронный старт STT: Python-сервер запускается враппером при
    // необходимости (ensureServerStarted), микрофон стартует после коннекта.
    bool ok = m_whisper->startRealtimeTranscription();
    if (!ok)
        qWarning() << "[SpeechManager] Failed to start speech recognition";
    return ok;
}

void SpeechManager::stopSpeech()
{
    m_whisper->stopRealtimeTranscription();
}

bool SpeechManager::startBufferedCapture()
{
    // Буферизованный захват не требует включённого речевого слоя:
    // он работает по явной команде (PTT / «распознай последние N сек»).
    return m_whisper->startBufferedCapture();
}

void SpeechManager::stopBufferedCapture()
{
    m_whisper->stopBufferedCapture();
}

QString SpeechManager::transcribeLastSeconds(int seconds)
{
    return m_whisper->transcribeLastSeconds(seconds);
}

void SpeechManager::StartTTS(const QString &text)
{
    if (!m_enabled) {
        qWarning() << "[SpeechManager] disabled — TTS ignored";
        return;
    }
    m_tts->speak(text);
}

void SpeechManager::connectTTSSignals()
{
    // Подключаем сигналы воспроизведения TTS
    connect(m_tts.get(), &TTSWrapper::playbackStarted,
            this, [this](int) {
                m_ttsPlaying = true;
                qDebug() << "[SpeechManager] TTS started — ignoring microphone recognition";
                emit ttsPlaybackStarted();
            });
    connect(m_tts.get(), &TTSWrapper::playbackFinished,
            this, [this](int) {
                m_ttsPlaying = false;
                qDebug() << "[SpeechManager] TTS finished — resuming microphone recognition";
                emit ttsPlaybackFinished();
            });
}

WhisperWrapper *SpeechManager::GetWhisperWrapper()
{
    return m_whisper.get();
}

TTSWrapper *SpeechManager::GetTTSWrapper()
{
    return m_tts.get();
}

void SpeechManager::onTranscriptReceived(const QString &text)
{
    if (text.trimmed().isEmpty())
        return;
    emit speechRecognized(text);
}

void SpeechManager::onTranscriptReceivedWithTimestamp(const QString &text, qint64 timestampMs)
{
    if (text.trimmed().isEmpty())
        return;

    // ИГНОРИРУЕМ распознавание если TTS воспроизводит — чтобы не слышать себя
    if (m_ttsPlaying) {
        qDebug() << "[SpeechManager] Skipping recognition during TTS playback";
        return;
    }

    // Отправляем с timestamp
    emit speechRecognizedWithTimestamp(text, timestampMs);
}
