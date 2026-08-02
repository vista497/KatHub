#include "WhisperWrapper.h"
#include "MicrophoneWrapper.h"
#include "PythonEnvironment.h"
#include "../core/AppPaths.h"

using namespace KatHub;  // KatHub::AppPaths

#include <QDebug>
#include <QDataStream>
#include <QJsonDocument>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QEventLoop>
#include <QTimer>
#include <QDateTime>

WhisperWrapper::WhisperWrapper(QObject *parent)
    : QObject(parent)
    , m_serverProcess(nullptr)
    , m_pythonEnv(new PythonEnvironment(this))
    , m_pythonScriptPath(AppPaths::sttStreamingServerPath())
    , m_autoStartServer(true)
    , m_serverStartedByUs(false)
    , m_socket(new QTcpSocket(this))
    , m_serverHost("127.0.0.1")
    , m_serverPort(5555)
    , m_expectedSize(0)
    , m_readingSize(true)
    , m_isInitialized(false)
    , m_isProcessing(false)
    , m_isConnected(false)
    , m_streamingActive(false)
    , m_language("ru")
    , m_reconnectTimer(new QTimer(this))
    , m_reconnectAttempts(0)
    , m_fileTranscriptionPending(false)
{
    // Настройка сокета
    connect(m_socket, &QTcpSocket::connected, this, &WhisperWrapper::onSocketConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &WhisperWrapper::onSocketDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &WhisperWrapper::onSocketReadyRead);
    // connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error),
    //         this, &WhisperWrapper::onSocketError);

    // Настройка таймера переподключения
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, &WhisperWrapper::tryReconnect);

    // Создаем микрофон
    m_microphone = std::make_unique<MicrophoneWrapper>(this);

    qDebug() << "WhisperWrapper: Initialized (Python server mode with auto-start)";
}

WhisperWrapper::~WhisperWrapper()
{
    // 1. Разрываем ВСЕ сигнально-слотовые связи до остановки процессов.
    //    Это предотвращает вылеты из-за сигналов, прилетающих во время/после
    //    удаления дочерних объектов (MicrophoneWrapper, QProcess).
    disconnect();

    // 2. Останавливаем таймер переподключения — чтобы не сработал во время shutdown
    if (m_reconnectTimer) {
        m_reconnectTimer->stop();
    }

    // 3. Корректная остановка
    shutdown();

    // 4. Останавливаем Python сервер если мы его запустили
    if (m_serverStartedByUs) {
        stopPythonServer();
    }
}

bool WhisperWrapper::initialize(const QString& modelPath)
{
    // modelPath теперь может быть путем к Python скрипту
    if (!modelPath.isEmpty()) {
       // m_pythonScriptPath = modelPath;
    }

    if (m_isInitialized) {
        qDebug() << "WhisperWrapper: Already initialized";
        return true;
    }

    qDebug() << "WhisperWrapper: Initializing...";
    qDebug() << "  Server:" << m_serverHost << ":" << m_serverPort;
    qDebug() << "  Language:" << m_language;
    qDebug() << "  Python script:" << m_pythonScriptPath;
    qDebug() << "  Auto-start:" << m_autoStartServer;

    // Настраиваем Python окружение через PythonEnvironment
    if (!m_pythonEnv->isReady() && !m_pythonEnv->setup()) {
        qWarning() << "WhisperWrapper: Python environment setup failed";
        emit errorOccurred("Python environment setup failed");
    }

    // Проверяем наличие скрипта
    if (m_autoStartServer && !checkScriptExists()) {
        qWarning() << "WhisperWrapper: Python script not found:" << m_pythonScriptPath;
        emit errorOccurred("Python script not found: " + m_pythonScriptPath);
    }

    // Если автозапуск включен, запускаем сервер
    if (m_autoStartServer) {
        if (!startPythonServer()) {
            qWarning() << "WhisperWrapper: Failed to start Python server";
            // Не критично, попробуем подключиться к существующему
        } else {
            // Даем серверу время запуститься
            QThread::msleep(2000);
        }
    }

    // Подключаемся к серверу
    connectToServer();

    // Даем время на подключение
    if (!m_socket->waitForConnected(3000)) {
        qWarning() << "WhisperWrapper: Failed to connect to server:" << m_socket->errorString();

        // Запускаем автоматическое переподключение
        m_reconnectTimer->start(RECONNECT_INTERVAL_MS);
    }

    m_isInitialized = true;
    qDebug() << "WhisperWrapper: Initialized successfully";

    return true;
}

void WhisperWrapper::shutdown()
{
    qDebug() << "WhisperWrapper: Shutting down...";

    // Разрываем сигналы до остановки процессов (защита от use-after-free)
    if (m_serverProcess) {
        disconnect(m_serverProcess, nullptr, this, nullptr);
    }
    disconnect(m_socket, nullptr, this, nullptr);
    disconnect(m_reconnectTimer, nullptr, this, nullptr);

    if (m_isProcessing) {
        stopRealtimeTranscription();
    }

    if (m_socket->isOpen()) {
        // Останавливаем поток если активен
        if (m_streamingActive) {
            QJsonObject message;
            message["command"] = "stop_stream";
            sendMessage(message);
            m_streamingActive = false;
        }

        m_socket->disconnectFromHost();
        if (m_socket->state() != QAbstractSocket::UnconnectedState) {
            m_socket->waitForDisconnected(1000);
        }
    }

    // Останавливаем Python сервер если мы его запустили
    if (m_serverStartedByUs) {
        stopPythonServer();
    }

    m_reconnectTimer->stop();
    m_isInitialized = false;
    m_isConnected = false;

    qDebug() << "WhisperWrapper: Shutdown complete";
}

bool WhisperWrapper::startRealtimeTranscription()
{
    if (!m_isInitialized) {
        qWarning() << "WhisperWrapper: Not initialized";
        emit errorOccurred("Not initialized");
        return false;
    }

    if (m_isProcessing) {
        qDebug() << "WhisperWrapper: Already processing";
        return true;
    }

    if (!m_isConnected) {
        qWarning() << "WhisperWrapper: Not connected to server";
        emit errorOccurred("Not connected to server");

        // Пробуем переподключиться
        connectToServer();
        return false;
    }

    qDebug() << "WhisperWrapper: Starting realtime transcription...";

    // Запускаем потоковую сессию на сервере
    QJsonObject message;
    message["command"] = "start_stream";
    message["language"] = m_language;
    message["sample_rate"] = SAMPLE_RATE;

    sendMessage(message);
    m_streamingActive = true;

    // Подключаем микрофон
    if (!m_microphone->isRecording()) {
        connect(m_microphone.get(), &MicrophoneWrapper::audioBufferReady,
                this, &WhisperWrapper::processAudioBuffer,
                Qt::UniqueConnection);

        if (!m_microphone->startRecording()) {
            qWarning() << "WhisperWrapper: Failed to start microphone";
            emit errorOccurred("Failed to start microphone");
            return false;
        }
    }

    m_isProcessing = true;
    emit processingStateChanged(true);

    qDebug() << "WhisperWrapper: Realtime transcription started";

    return true;
}

void WhisperWrapper::stopRealtimeTranscription()
{
    if (!m_isProcessing) {
        return;
    }

    qDebug() << "WhisperWrapper: Stopping realtime transcription...";

    // Останавливаем микрофон (с проверкой — на случай если shutdown вызван
    // после частичного разрушения)
    if (m_microphone && m_microphone->isRecording()) {
        m_microphone->stopRecording();
        disconnect(m_microphone.get(), &MicrophoneWrapper::audioBufferReady,
                   this, &WhisperWrapper::processAudioBuffer);
    }

    // Останавливаем потоковую сессию на сервере
    if (m_streamingActive && m_isConnected) {
        QJsonObject message;
        message["command"] = "stop_stream";
        sendMessage(message);
        m_streamingActive = false;
    }

    // Очищаем аккумулятор
    {
        QMutexLocker locker(&m_accumulatorMutex);
        m_audioAccumulator.clear();
    }

    m_isProcessing = false;
    emit processingStateChanged(false);

    qDebug() << "WhisperWrapper: Realtime transcription stopped";
}

bool WhisperWrapper::isProcessing() const
{
    return m_isProcessing;
}

QString WhisperWrapper::transcribeAudioFile(const QString& audioFilePath)
{
    if (!m_isInitialized) {
        qWarning() << "WhisperWrapper: Not initialized";
        emit errorOccurred("Not initialized");
        return QString();
    }

    if (!m_isConnected) {
        qWarning() << "WhisperWrapper: Not connected to server";
        emit errorOccurred("Not connected to server");
        return QString();
    }

    qDebug() << "WhisperWrapper: Transcribing file:" << audioFilePath;

    // Флаг ожидания результата
    m_fileTranscriptionPending = true;
    m_fileTranscriptionResult.clear();

    // Отправляем команду на транскрипцию файла
    QJsonObject message;
    message["command"] = "transcribe_file";
    message["file_path"] = audioFilePath;
    message["language"] = m_language;

    sendMessage(message);

    // Ждем результат с таймаутом (синхронно)
    QEventLoop loop;
    QTimer timeoutTimer(this);
    timeoutTimer.setSingleShot(true);
    
    connect(this, &WhisperWrapper::fileTranscriptionComplete,
            &loop, &QEventLoop::quit, Qt::QueuedConnection);
    connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
    
    timeoutTimer.start(1200);  // 2 минуты на транскрипцию
    
    loop.exec(QEventLoop::ExcludeUserInputEvents);
    
    timeoutTimer.stop();
    
    // Отключаемся от сигналов
    disconnect(this, &WhisperWrapper::fileTranscriptionComplete,
               &loop, &QEventLoop::quit);
    disconnect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
    
    m_fileTranscriptionPending = false;

    if (m_fileTranscriptionResult.isEmpty()) {
        qWarning() << "WhisperWrapper: File transcription timeout or error";
        emit errorOccurred("File transcription timeout");
        return QString();
    }

    qDebug() << "WhisperWrapper: File transcription completed:" << m_fileTranscriptionResult;
    return m_fileTranscriptionResult;
}

QString WhisperWrapper::lastTranscription() const
{
    return m_lastTranscription;
}

// ============================================================================
//  Буферизованный захват: микрофон пишет в кольцевой буфер,
//  в STT ничего не уходит до явного вызова transcribeLastSeconds().
// ============================================================================

void WhisperWrapper::setStreamingEnabled(bool enabled)
{
    m_streamingEnabled = enabled;
    qDebug() << "WhisperWrapper: streaming enabled =" << enabled;
}

bool WhisperWrapper::startBufferedCapture()
{
    // Глубина буфера: 5 минут по умолчанию (кольцевой буфер микрофона
    // пишется ВСЕГДА в processAudioData, независимо от стриминга).
    m_microphone->setHistorySeconds(300);

    // STT расшифровывает непрерывно: микрофон -> STT-сервер -> текст.
    // Гейт на передаче в LLM ставит вызывающий слой (wake-word).
    setStreamingEnabled(true);

    // Поднимаем Python-сервер и подключаемся (если ещё не сделано).
    if (!m_isInitialized) {
        initialize(QString());
    }
    if (!m_isConnected) {
        connectToServer();
        if (!m_socket->waitForConnected(3000)) {
            qWarning() << "WhisperWrapper: No STT server for continuous capture";
            emit errorOccurred("STT server not connected");
            return false;
        }
    }

    // startRealtimeTranscription() отправляет start_stream, подключает
    // audioBufferReady -> processAudioBuffer и запускает микрофон.
    // Буфер истории (m_floatBuffer) пишется параллельно в MicrophoneWrapper.
    if (!startRealtimeTranscription()) {
        qWarning() << "WhisperWrapper: Failed to start continuous STT stream";
        return false;
    }

    qDebug() << "WhisperWrapper: Continuous capture started (STT streaming + ring buffer)";
    return true;
}

void WhisperWrapper::stopBufferedCapture()
{
    if (m_microphone && m_microphone->isRecording()) {
        m_microphone->stopRecording();
    }
}

int WhisperWrapper::bufferSeconds() const
{
    if (!m_microphone)
        return 0;
    const std::vector<float> history = m_microphone->getAudioData();
    if (history.empty() || m_microphone->sampleRate() <= 0)
        return 0;
    return static_cast<int>(history.size() / m_microphone->sampleRate());
}

QString WhisperWrapper::transcribeLastSeconds(int seconds)
{
    if (!m_microphone) {
        emit errorOccurred("Microphone not available");
        return QString();
    }

    // Хвост кольцевого буфера: последние N секунд
    const std::vector<float> history = m_microphone->getAudioData();
    if (history.empty()) {
        qWarning() << "WhisperWrapper: Buffer is empty, nothing to transcribe";
        return QString();
    }

    const int sampleRate = m_microphone->sampleRate();
    const size_t windowSamples = static_cast<size_t>(sampleRate) * static_cast<size_t>(seconds);
    const size_t startIdx = history.size() > windowSamples ? history.size() - windowSamples : 0;
    const std::vector<float> window(history.begin() + static_cast<long>(startIdx), history.end());
    if (window.empty()) {
        return QString();
    }

    // Пишем окно во временный WAV (16-bit PCM mono)
    const QString wavPath = writeWindowToWav(window, sampleRate);
    if (wavPath.isEmpty()) {
        emit errorOccurred("Failed to write WAV for buffered window");
        return QString();
    }

    qDebug() << "WhisperWrapper: Transcribing last" << seconds << "s window:" << wavPath;

    // Убеждаемся, что сервер поднят и подключены
    if (!m_isInitialized) {
        initialize(QString());
    }
    if (!m_isConnected) {
        connectToServer();
        if (!m_socket->waitForConnected(3000)) {
            qWarning() << "WhisperWrapper: No STT server for buffered transcription";
            QFile::remove(wavPath);
            emit errorOccurred("STT server not connected");
            return QString();
        }
    }

    const QString text = transcribeAudioFile(wavPath);
    QFile::remove(wavPath);
    return text;
}

QString WhisperWrapper::writeWindowToWav(const std::vector<float>& samples, int sampleRate)
{
    const QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const QString path = tempDir + QStringLiteral("/kathub_buffered_%1.wav")
        .arg(QDateTime::currentMSecsSinceEpoch());

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        qWarning() << "WhisperWrapper: Cannot create WAV:" << path;
        return QString();
    }

    const uint32_t dataSize = static_cast<uint32_t>(samples.size() * sizeof(int16_t));
    const uint32_t chunkSize = 36 + dataSize;

    auto w32 = [&f](uint32_t v) {
        char b[4];
        b[0] = static_cast<char>(v & 0xFF);
        b[1] = static_cast<char>((v >> 8) & 0xFF);
        b[2] = static_cast<char>((v >> 16) & 0xFF);
        b[3] = static_cast<char>((v >> 24) & 0xFF);
        f.write(b, 4);
    };
    auto w16 = [&f](uint16_t v) {
        char b[2];
        b[0] = static_cast<char>(v & 0xFF);
        b[1] = static_cast<char>((v >> 8) & 0xFF);
        f.write(b, 2);
    };

    // RIFF header
    f.write("RIFF", 4);
    w32(chunkSize);
    f.write("WAVE", 4);

    // fmt chunk (PCM int16 mono)
    f.write("fmt ", 4);
    w32(16);
    w16(1);
    w16(1);
    w32(static_cast<uint32_t>(sampleRate));
    w32(static_cast<uint32_t>(sampleRate * 2));
    w16(2);
    w16(16);

    // data chunk
    f.write("data", 4);
    w32(dataSize);

    QByteArray pcm;
    pcm.reserve(static_cast<int>(samples.size() * 2));
    for (float s : samples) {
        int16_t v = static_cast<int16_t>(qBound(-1.0f, s, 1.0f) * 32767.0f);
        pcm.append(static_cast<char>(v & 0xFF));
        pcm.append(static_cast<char>((v >> 8) & 0xFF));
    }
    f.write(pcm);
    f.close();
    return path;
}

void WhisperWrapper::connectToServer()
{
    if (m_socket->state() == QAbstractSocket::ConnectedState ||
        m_socket->state() == QAbstractSocket::ConnectingState) {
        return;
    }

    qDebug() << "WhisperWrapper: Connecting to" << m_serverHost << ":" << m_serverPort;

    m_socket->connectToHost(m_serverHost, m_serverPort);
}

void WhisperWrapper::sendMessage(const QJsonObject& message)
{
    if (!m_isConnected) {
        qWarning() << "WhisperWrapper: Cannot send message - not connected";
        return;
    }

    QJsonDocument doc(message);
    QByteArray data = doc.toJson(QJsonDocument::Compact);

    // Отправляем размер (4 байта, big-endian)
    qint32 size = data.size();
    QByteArray sizeData(4, 0);
    QDataStream sizeStream(&sizeData, QIODevice::WriteOnly);
    sizeStream.setByteOrder(QDataStream::BigEndian);
    sizeStream << size;

    // Отправляем размер + данные
    m_socket->write(sizeData);
    m_socket->write(data);
    m_socket->flush();
}

void WhisperWrapper::processEvent(const QJsonObject& event)
{
    QString eventType = event["event"].toString();
    QString status = event["status"].toString();

    if (status == "error") {
        QString errorMsg = event["message"].toString();
        qWarning() << "WhisperWrapper: Server error:" << errorMsg;
        emit errorOccurred(errorMsg);
        return;
    }

    // Обработка событий
    if (eventType == "stream_started") {
        qDebug() << "WhisperWrapper: Stream started on server";
    }
    else if (eventType == "stream_stopped") {
        qDebug() << "WhisperWrapper: Stream stopped on server";
        m_streamingActive = false;
    }
    else if (eventType == "speech_start") {
        qDebug() << "WhisperWrapper: Speech detected";
        emit speechStarted();
    }
    else if (eventType == "speech_end") {
        qDebug() << "WhisperWrapper: Speech ended";
        emit speechEnded();
    }
    else if (eventType == "transcription") {
        QJsonArray results = event["results"].toArray();
        qint64 serverTimestamp = 0;
        if (event.contains("timestamp")) {
            serverTimestamp = static_cast<qint64>(event["timestamp"].toDouble() * 1000);
        }

        if (!results.isEmpty()) {
            QString text;

            // Собираем весь текст из результатов
            for (const QJsonValue& val : results) {
                QJsonObject obj = val.toObject();
                QString segment = obj["text"].toString();

                if (!segment.isEmpty()) {
                    if (!text.isEmpty()) {
                        text += " ";
                    }
                    text += segment;
                }
            }

            if (!text.isEmpty()) {
                qDebug() << "WhisperWrapper: Transcription:" << text;
                if (serverTimestamp > 0) {
                    qDebug() << "  Timestamp:" << QDateTime::fromMSecsSinceEpoch(serverTimestamp).toString("HH:mm:ss.zzz");
                }

                m_lastTranscription = text;

                // Эмулируем оригинальные сигналы
                emit realtimeTranscript(text);
                emit transcriptionChanged(text);
                
                // Новый сигнал с timestamp
                emit realtimeTranscriptWithTimestamp(text, serverTimestamp);
            }
        }
    }
    else if (eventType == "status") {
        qDebug() << "WhisperWrapper: Server status received";
    }
    else if (eventType == "pong") {
        qDebug() << "WhisperWrapper: Pong received";
    }
    else if (eventType == "file_transcription_complete") {
        QJsonArray results = event["results"].toArray();
        QString filePath = event["file_path"].toString();
        QString language = event["language"].toString();

        qDebug() << "WhisperWrapper: File transcription complete:" << filePath;
        qDebug() << "  Language:" << language;
        qDebug() << "  Results count:" << results.size();

        if (!results.isEmpty()) {
            QString text;

            // Собираем весь текст из результатов
            for (const QJsonValue& val : results) {
                QJsonObject obj = val.toObject();
                QString segment = obj["text"].toString();

                if (!segment.isEmpty()) {
                    if (!text.isEmpty()) {
                        text += " ";
                    }
                    text += segment;
                }
            }

            if (!text.isEmpty()) {
                qDebug() << "WhisperWrapper: File transcription:" << text;

                m_lastTranscription = text;
                m_fileTranscriptionResult = text;  // Сохраняем для синхронного возврата

                // Эмулируем оригинальные сигналы
                emit fileTranscriptionComplete(text, filePath, results.size());
            }
        }
    }
    else if (eventType == "file_error") {
        QString error = event["error"].toString();
        qWarning() << "WhisperWrapper: File transcription error:" << error;
        emit errorOccurred("File transcription error: " + error);
    }
}

void WhisperWrapper::processAudioBuffer(const std::vector<float>& audioData)
{
    if (!m_isConnected || !m_streamingActive) {
        return;
    }

    // Добавляем в аккумулятор
    // {
    //     QMutexLocker locker(&m_accumulatorMutex);
    //     m_audioAccumulator.insert(m_audioAccumulator.end(),
    //                               audioData.begin(),
    //                               audioData.end());

    //     // Отправляем если накопилось достаточно (0.5 секунды)
    //     if (m_audioAccumulator.size() >= CHUNK_SIZE_SAMPLES) {
    //         sendAudioChunk(m_audioAccumulator);
    //         m_audioAccumulator.clear();
    //     }
    // }
    sendAudioChunk(audioData);
}

void WhisperWrapper::sendAudioChunk(const std::vector<float>& audioData)
{
    // Конвертируем float → int16
    QByteArray int16Data = convertToInt16(audioData);

    // Отправляем как audio_chunk команду
    QJsonObject message;
    message["command"] = "audio_chunk";
    message["audio_data"] = QString(int16Data.toBase64());

    sendMessage(message);
}

QByteArray WhisperWrapper::convertToInt16(const std::vector<float>& audioData)
{
    QByteArray result;
    result.reserve(audioData.size() * sizeof(int16_t));

    for (float sample : audioData) {
        // Ограничиваем диапазон [-1.0, 1.0]
        sample = std::max(-1.0f, std::min(1.0f, sample));

        // Конвертируем в int16
        int16_t value = static_cast<int16_t>(sample * 32767.0f);

        // Добавляем в QByteArray (little-endian)
        result.append(reinterpret_cast<const char*>(&value), sizeof(int16_t));
    }

    return result;
}

void WhisperWrapper::onSocketConnected()
{
    qDebug() << "WhisperWrapper: Connected to server";

    m_isConnected = true;
    m_reconnectAttempts = 0;
    m_reconnectTimer->stop();

    emit connected();

    // Если мы должны были обрабатывать, запускаем
    if (m_isProcessing && !m_streamingActive) {
        startRealtimeTranscription();
    }
}

void WhisperWrapper::onSocketDisconnected()
{
    qDebug() << "WhisperWrapper: Disconnected from server";

    bool wasConnected = m_isConnected;
    m_isConnected = false;
    m_streamingActive = false;

    m_receiveBuffer.clear();
    m_readingSize = true;

    if (wasConnected) {
        emit disconnected();

        // Пробуем переподключиться
        if (m_isInitialized && m_reconnectAttempts < MAX_RECONNECT_ATTEMPTS) {
            qDebug() << "WhisperWrapper: Scheduling reconnect...";
            m_reconnectTimer->start(RECONNECT_INTERVAL_MS);
        } else if (m_reconnectAttempts >= MAX_RECONNECT_ATTEMPTS) {
            qWarning() << "WhisperWrapper: Max reconnect attempts reached";
            emit errorOccurred("Connection lost - max reconnect attempts reached");
        }
    }
}

void WhisperWrapper::onSocketError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError);

    QString errorStr = m_socket->errorString();
    qWarning() << "WhisperWrapper: Socket error:" << errorStr;

    emit errorOccurred(errorStr);
}

void WhisperWrapper::onSocketReadyRead()
{
    while (m_socket->bytesAvailable() > 0) {
        if (m_readingSize) {
            // Читаем размер сообщения (4 байта)
            if (m_socket->bytesAvailable() < 4) {
                return;  // Ждем больше данных
            }

            QByteArray sizeData = m_socket->read(4);
            QDataStream sizeStream(sizeData);
            sizeStream.setByteOrder(QDataStream::BigEndian);
            sizeStream >> m_expectedSize;

            m_readingSize = false;
            m_receiveBuffer.clear();
        }

        // Читаем данные сообщения
        qint64 bytesNeeded = m_expectedSize - m_receiveBuffer.size();
        if (bytesNeeded > 0) {
            QByteArray chunk = m_socket->read(qMin(bytesNeeded, m_socket->bytesAvailable()));
            m_receiveBuffer.append(chunk);
        }

        // Если получили все данные
        if (m_receiveBuffer.size() >= m_expectedSize) {
            QJsonDocument doc = QJsonDocument::fromJson(m_receiveBuffer);

            if (!doc.isNull() && doc.isObject()) {
                processEvent(doc.object());
            } else {
                qWarning() << "WhisperWrapper: Invalid JSON received";
            }

            m_readingSize = true;
            m_receiveBuffer.clear();
        }
    }
}

void WhisperWrapper::tryReconnect()
{
    if (m_isConnected) {
        return;
    }

    m_reconnectAttempts++;

    qDebug() << "WhisperWrapper: Reconnect attempt" << m_reconnectAttempts
             << "/" << MAX_RECONNECT_ATTEMPTS;

    connectToServer();

    // Если не удалось подключиться, планируем следующую попытку
    // if (!m_socket->waitForConnected(2000)) {
    //     if (m_reconnectAttempts < MAX_RECONNECT_ATTEMPTS) {
    //         m_reconnectTimer->start(RECONNECT_INTERVAL_MS);
    //     }
    // }
}

// ============================================================================
// Управление Python сервером
// ============================================================================

bool WhisperWrapper::startPythonServer()
{
    if (m_serverProcess && m_serverProcess->state() != QProcess::NotRunning) {
        qDebug() << "WhisperWrapper: Python server already running";
        return true;
    }

    // Убираем мёртвый процесс от предыдущего запуска
    if (m_serverProcess) {
        m_serverProcess->deleteLater();
        m_serverProcess = nullptr;
    }

    qDebug() << "WhisperWrapper: Starting Python server (via PythonEnvironment)...";
    qDebug() << "  Script:" << m_pythonScriptPath;
    qDebug() << "  Host:" << m_serverHost;
    qDebug() << "  Port:" << m_serverPort;

    // Аргументы для Python скрипта
    QStringList args;
    args << "--host" << m_serverHost;
    args << "--port" << QString::number(m_serverPort);
    args << "--model" << "base";

    // Запускаем через PythonEnvironment (venv)
    m_serverProcess = m_pythonEnv->runScript(m_pythonScriptPath, args);

    // Подключаем сигналы к процессу
    connect(m_serverProcess, &QProcess::started, this, &WhisperWrapper::onServerStarted);
    connect(m_serverProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &WhisperWrapper::onServerFinished);
    connect(m_serverProcess, &QProcess::errorOccurred, this, &WhisperWrapper::onServerErrorOccurred);
    connect(m_serverProcess, &QProcess::readyReadStandardOutput,
            this, &WhisperWrapper::onServerReadyReadStandardOutput);
    connect(m_serverProcess, &QProcess::readyReadStandardError,
            this, &WhisperWrapper::onServerReadyReadStandardError);

    // runScript уже вызвал start(), ждём подтверждения
    if (!m_serverProcess->waitForStarted(5000)) {
        qWarning() << "WhisperWrapper: Failed to start Python server:"
                   << m_serverProcess->errorString();
        return false;
    }

    m_serverStartedByUs = true;
    qDebug() << "WhisperWrapper: Python server started successfully (via PythonEnvironment)";

    return true;
}

void WhisperWrapper::stopPythonServer()
{
    if (!m_serverStartedByUs || !m_serverProcess
        || m_serverProcess->state() == QProcess::NotRunning) {
        return;
    }

    qDebug() << "WhisperWrapper: Stopping Python server...";

    // Разрываем сигналы до остановки — предотвращаем use-after-free
    disconnect(m_serverProcess, nullptr, this, nullptr);

    // Асинхронное завершение (без waitForFinished, чтобы не блокировать event loop)
    m_serverProcess->terminate();

    m_serverStartedByUs = false;
    qDebug() << "WhisperWrapper: Python server stop initiated";
}

bool WhisperWrapper::checkScriptExists()
{
    // Проверяем в текущей директории
    if (QFile::exists(m_pythonScriptPath)) {
        qDebug() << "WhisperWrapper: Found script:" << m_pythonScriptPath;
        return true;
    }

    // Проверяем рядом с исполняемым файлом приложения
    QString appDir = QCoreApplication::applicationDirPath();
    QString scriptInAppDir = QDir(appDir).filePath(m_pythonScriptPath);

    if (QFile::exists(scriptInAppDir)) {
        m_pythonScriptPath = scriptInAppDir;
        qDebug() << "WhisperWrapper: Found script in app dir:" << m_pythonScriptPath;
        return true;
    }

    // Проверяем в поддиректории scripts
    QString scriptInScriptsDir = QDir(appDir).filePath("scripts/" + m_pythonScriptPath);

    if (QFile::exists(scriptInScriptsDir)) {
        m_pythonScriptPath = scriptInScriptsDir;
        qDebug() << "WhisperWrapper: Found script in scripts dir:" << m_pythonScriptPath;
        return true;
    }

    qWarning() << "WhisperWrapper: Python script not found:" << m_pythonScriptPath;
    qWarning() << "  Tried paths:";
    qWarning() << "   " << m_pythonScriptPath;
    qWarning() << "   " << scriptInAppDir;
    qWarning() << "   " << scriptInScriptsDir;

    return false;
}

void WhisperWrapper::onServerStarted()
{
    qDebug() << "WhisperWrapper: Python server process started";
    emit serverStarted();
}

void WhisperWrapper::onServerFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    qDebug() << "WhisperWrapper: Python server process finished";
    qDebug() << "  Exit code:" << exitCode;
    qDebug() << "  Exit status:" << (exitStatus == QProcess::NormalExit ? "Normal" : "Crashed");

    m_serverStartedByUs = false;
    emit serverStopped();

    if (exitStatus == QProcess::CrashExit) {
        emit errorOccurred("Python server crashed");
    }
}

void WhisperWrapper::onServerErrorOccurred(QProcess::ProcessError error)
{
    QString errorStr;

    switch (error) {
    case QProcess::FailedToStart:
        errorStr = "Failed to start Python server";
        break;
    case QProcess::Crashed:
        errorStr = "Python server crashed";
        break;
    case QProcess::Timedout:
        errorStr = "Python server timed out";
        break;
    case QProcess::WriteError:
        errorStr = "Write error to Python server";
        break;
    case QProcess::ReadError:
        errorStr = "Read error from Python server";
        break;
    default:
        errorStr = "Unknown Python server error";
        break;
    }

    qWarning() << "WhisperWrapper: Python server error:" << errorStr;
   // emit serverError(errorStr);
   // emit errorOccurred(errorStr);
}

void WhisperWrapper::onServerReadyReadStandardOutput()
{
    QString output = QString::fromUtf8(m_serverProcess->readAllStandardOutput());

    // Выводим в консоль
    qDebug().noquote() << "Python server:" << output.trimmed();

    // Отправляем сигнал
    emit serverOutput(output);
}

void WhisperWrapper::onServerReadyReadStandardError()
{
    QString error = QString::fromUtf8(m_serverProcess->readAllStandardError());

    // Выводим в консоль
    qWarning().noquote() << "Python server error:" << error.trimmed();

    // Отправляем сигнал
    emit serverError(error);
}
