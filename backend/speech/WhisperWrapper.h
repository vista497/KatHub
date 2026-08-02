#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QMutex>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QProcess>
#include <memory>
#include <vector>
#include <atomic>

// Forward declarations
class MicrophoneWrapper;
class PythonEnvironment;

/**
 * @brief WhisperWrapper - клиент для Python STT сервера
 *
 * Совместимость с оригинальным API:
 * - Все сигналы сохранены
 * - Методы initialize/startRealtimeTranscription/stopRealtimeTranscription работают так же
 * - Внутри использует TCP подключение к Python серверу вместо локального Whisper
 */
class WhisperWrapper : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isProcessing READ isProcessing NOTIFY processingStateChanged)
    Q_PROPERTY(QString lastTranscription READ lastTranscription NOTIFY transcriptionChanged)

public:
    explicit WhisperWrapper(QObject *parent = nullptr);
    ~WhisperWrapper();

    // API совместимый с оригиналом
    bool initialize(const QString& modelPath);  // modelPath = путь к Python скрипту или пустая строка
    void shutdown();

    bool startRealtimeTranscription();
    void stopRealtimeTranscription();
    bool isProcessing() const;

    /// Включить/выключить потоковую отправку чанков в STT.
    /// false = микрофон пишет только в кольцевой буфер (ничего не уходит в модель).
    void setStreamingEnabled(bool enabled);
    bool streamingEnabled() const { return m_streamingEnabled; }

    /// Сколько секунд аудио накоплено в кольцевом буфере микрофона.
    int bufferSeconds() const;
    /// Сервер инициализирован (initialize вызван)?
    bool isInitialized() const { return m_isInitialized; }
    /// Есть TCP-соединение с STT-сервером?
    bool isConnected() const { return m_isConnected; }

    /// Старт микрофона БЕЗ отправки в STT — только запись в кольцевой буфер.
    bool startBufferedCapture();
    void stopBufferedCapture();

    /// Распознать последние N секунд из кольцевого буфера (синхронно).
    /// Возвращает текст; пустая строка — ошибка/таймаут.
    QString transcribeLastSeconds(int seconds);

    QString transcribeAudioFile(const QString& audioFilePath);
    QString lastTranscription() const;

    // Дополнительные методы для настройки
    void setServerHost(const QString& host) { m_serverHost = host; }
    void setServerPort(quint16 port) { m_serverPort = port; }
    void setLanguage(const QString& language) { m_language = language; }
    QString language() const { return m_language; }

    // Управление Python сервером
    void setPythonScriptPath(const QString& path) { m_pythonScriptPath = path; }
    void setAutoStartServer(bool autoStart) { m_autoStartServer = autoStart; }
    bool isServerRunning() const { return m_serverProcess && m_serverProcess->state() == QProcess::Running; }

signals:
    // Оригинальные сигналы (совместимость)
    void transcriptionChanged(const QString& text);
    void processingStateChanged(bool isProcessing);
    void errorOccurred(const QString& errorMessage);
    void realtimeTranscript(const QString& text);
    void realtimeTranscriptWithTimestamp(const QString& text, qint64 timestampMs);
    void partialTranscript(const QString& text);

    // Дополнительные сигналы
    void connected();
    void disconnected();
    void speechStarted();
    void speechEnded();
    void fileTranscriptionComplete(const QString& text, const QString& filePath, int segmentsCount);

    // Сигналы Python сервера
    void serverStarted();
    void serverStopped();
    void serverOutput(const QString& output);
    void serverError(const QString& error);

private slots:
    void processAudioBuffer(const std::vector<float>& audioData);
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError socketError);
    void onSocketReadyRead();
    void tryReconnect();

    // Слоты для Python процесса
    void onServerStarted();
    void onServerFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onServerErrorOccurred(QProcess::ProcessError error);
    void onServerReadyReadStandardOutput();
    void onServerReadyReadStandardError();

private:
    void connectToServer();
    void sendMessage(const QJsonObject& message);
    void processEvent(const QJsonObject& event);
    void sendAudioChunk(const std::vector<float>& audioData);

    // Управление Python сервером
    bool startPythonServer();
    void stopPythonServer();
    bool checkScriptExists();

    // Конвертация float → int16 для отправки
    QByteArray convertToInt16(const std::vector<float>& audioData);

    // Записать окно float-сэмплов во временный WAV (16-bit PCM mono)
    QString writeWindowToWav(const std::vector<float>& samples, int sampleRate);

    // Python процесс
    QProcess* m_serverProcess;
    PythonEnvironment* m_pythonEnv;
    QString m_pythonScriptPath;
    bool m_autoStartServer;
    bool m_serverStartedByUs;

    // Сетевое подключение
    QTcpSocket* m_socket;
    QString m_serverHost;
    quint16 m_serverPort;

    // Буферизация сообщений
    QByteArray m_receiveBuffer;
    qint32 m_expectedSize;
    bool m_readingSize;

    // Микрофон
    std::unique_ptr<MicrophoneWrapper> m_microphone;

    // Состояние
    bool m_isInitialized;
    bool m_isProcessing;
    bool m_isConnected;
    bool m_streamingActive;
    bool m_streamingEnabled = true;  // false = только буфер, без отправки в STT
    QString m_lastTranscription;
    QString m_language;

    // Переподключение
    QTimer* m_reconnectTimer;
    int m_reconnectAttempts;

    // Буферизация аудио для отправки
    std::vector<float> m_audioAccumulator;
    QMutex m_accumulatorMutex;

    // Для синхронной транскрипции файлов
    bool m_fileTranscriptionPending;
    QString m_fileTranscriptionResult;

    // Константы
    static constexpr int SAMPLE_RATE = 16000;
    static constexpr size_t CHUNK_SIZE_SAMPLES = 16000;  // 0.5 секунды
    static constexpr int RECONNECT_INTERVAL_MS = 3000;
    static constexpr int MAX_RECONNECT_ATTEMPTS = 10;
};
