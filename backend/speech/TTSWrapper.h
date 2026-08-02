#pragma once

#include <QObject>
#include <QString>
#include <QTcpSocket>
#include <QJsonObject>
#include <QProcess>
#include <QTimer>
#include <memory>
#include <map>
#include <deque>

class AudioPlayer;
#ifdef KATHUB_ENABLE_POCKET_TTS
class PocketTTSEngine;
#endif

/**
 * @brief TTSWrapper - клиент для Python TTS сервера
 *
 * Совместимость с оригинальным API:
 * - Все сигналы сохранены
 * - Методы speak/stop/stopAll работают так же
 * - Внутри использует TCP подключение к Python серверу вместо локального TTS
 */
class TTSWrapper : public QObject
{
    Q_OBJECT

public:
    explicit TTSWrapper(QObject *parent = nullptr);
    ~TTSWrapper();

    // Возвращает speakId (уникальный идентификатор запроса)
    int speak(const QString &text);

    // Управление
    void stop(int id);                    // можно передавать speakId или playId
    void stopAll();
    void stopCurrent();

    void clearQueue();                    // сбросить очередь синтеза
    void resetTTS();                      // отправить reset в TTS-сервер
    void interruptCurrentSynthesis();     // прервать текущий синтез

    bool isSpeaking(int id) const;
    bool isSpeakingAny() const;

    // Настройки подключения к TTS-серверу
    void setServerAddress(const QString &host, quint16 port);
    QString serverHost() const { return m_host; }
    quint16 serverPort() const { return m_port; }

    // Управление Python сервером
    void setPythonScriptPath(const QString& path) { m_pythonScriptPath = path; }
    void setAutoStartServer(bool autoStart) { m_autoStartServer = autoStart; }
    bool isServerRunning() const { return m_serverProcess && m_serverProcess->state() == QProcess::Running; }

    // Инициализация (запуск сервера и подключение)
    bool initialize();
    void shutdown();

#ifdef KATHUB_ENABLE_POCKET_TTS
    // ── PocketTTS (нативный TTS) ──
    /// Инициализировать нативный PocketTTS (вместо Python-сервера)
    bool initPocketTTS(const QString &modelsDir);
    /// Загрузить голос для PocketTTS
    bool loadPocketTTSSpeaker(const QString &audioPath);
    /// Использовать нативный TTS вместо Python-сервера
    bool isNativeTTSReady() const;
#endif
    void setUseNativeTTS(bool use) { m_useNativeTTS = use; }

signals:
    void ttsStarted(int speakId, const QString &text);
    void ttsFinished(int speakId, const QString &audioFilePath);
    void ttsError(int speakId, const QString &errorMessage);

    void playbackStarted(int playId);
    void playbackFinished(int playId);
    void playbackStopped(int playId);

    // Сигналы Python сервера
    void serverStarted();
    void serverStopped();
    void serverOutput(const QString& output);
    void serverError(const QString& error);

private slots:
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketErrorOccurred(QAbstractSocket::SocketError socketError);
    void onSocketReadyRead();

    void onAudioPlaybackStarted(int playId, const QString &filePath);
    void onAudioPlaybackFinished(int playId, const QString &filePath);
    void onAudioPlaybackStopped(int playId, const QString &filePath);

    // Слоты для Python процесса
    void onServerStarted();
    void onServerFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onServerErrorOccurred(QProcess::ProcessError error);
    void onServerReadyReadStandardOutput();
    void onServerReadyReadStandardError();

private:
    void sendJson(const QJsonObject &obj);
    void processNextInQueue();
    void sendInterrupt();
    int findSpeakIdByPlayId(int playId) const;

    // Управление Python сервером
    bool startPythonServer();
    void stopPythonServer();
    bool findPythonExecutable();
    bool checkScriptExists();

    // Python процесс
    QProcess* m_serverProcess;
    QString m_pythonExecutable;
    QString m_pythonScriptPath;
    bool m_autoStartServer;
    bool m_serverStartedByUs;

    // Сетевое подключение
    QTcpSocket* m_socket;
    QString m_host;
    quint16 m_port;
    QByteArray m_recvBuffer;               // буфер для приёма сообщений

    std::deque<std::pair<int, QString>> m_synthesisQueue;   // <speakId, text>
    bool m_isSynthesizing = false;
    int m_currentSpeakId = -1;
    int m_nextSpeakId = 1;

    std::map<int, int> m_speakIdToPlayId;   // speakId → playId (AudioPlayer)
    std::map<int, int> m_playIdToSpeakId;   // playId → speakId

    AudioPlayer* m_audioPlayer;

#ifdef KATHUB_ENABLE_POCKET_TTS
    // PocketTTS (нативный движок)
    PocketTTSEngine* m_pocketTTS = nullptr;
#endif
    bool m_useNativeTTS = false;

#ifdef KATHUB_ENABLE_POCKET_TTS
    // ── WAV-запись (для PocketTTS) ──
    /// Сохраняет PCM float32 в WAV-файл, возвращает путь
    static QString savePcmToWav(const std::vector<float>& samples, int sampleRate);
#endif

#ifdef Q_OS_WIN
    void duckOtherAudio();
    void restoreAudio();
    std::map<unsigned long, float> m_originalVolumes;
    bool m_audioDucked = false;
#endif

    // Константы
    static constexpr int RECONNECT_INTERVAL_MS = 3000;
    static constexpr int MAX_RECONNECT_ATTEMPTS = 10;
};
