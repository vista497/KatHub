#include "TTSWrapper.h"
#include "AudioPlayer.h"
#ifdef KATHUB_ENABLE_POCKET_TTS
#include "PocketTTSEngine.h"
#endif
#include "../core/AppPaths.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QFileInfo>
#include <QCoreApplication>
#include <QTcpSocket>
#include <QHostAddress>
#include <QProcess>
#include <QDir>
#include <QStandardPaths>
#include <QEventLoop>
#include <QTimer>
#include <QThread>
#include <QDateTime>

using namespace KatHub;  // KatHub::AppPaths

#ifdef Q_OS_WIN
#  include <windows.h>
#  include <mmdeviceapi.h>
#  include <audiopolicy.h>
#  include <endpointvolume.h>
#endif

TTSWrapper::TTSWrapper(QObject *parent)
    : QObject(parent)
    , m_serverProcess(new QProcess(this))
    , m_pythonExecutable(AppPaths::ttsPythonExePath())
    , m_pythonScriptPath(AppPaths::ttsNodeServerPath())
    , m_autoStartServer(true)
    , m_serverStartedByUs(false)
    , m_audioPlayer(AudioPlayer::instance())
#ifdef KATHUB_ENABLE_POCKET_TTS
    , m_pocketTTS(new PocketTTSEngine(this))
#endif
    , m_socket(new QTcpSocket(this))
    , m_host("127.0.0.1")
    , m_port(5556)
{
    // Настройка Python процесса
    connect(m_serverProcess, &QProcess::started, this, &TTSWrapper::onServerStarted);
    connect(m_serverProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &TTSWrapper::onServerFinished);
    connect(m_serverProcess, &QProcess::errorOccurred, this, &TTSWrapper::onServerErrorOccurred);
    connect(m_serverProcess, &QProcess::readyReadStandardOutput,
            this, &TTSWrapper::onServerReadyReadStandardOutput);
    connect(m_serverProcess, &QProcess::readyReadStandardError,
            this, &TTSWrapper::onServerReadyReadStandardError);

    // Настройка сокета
    connect(m_socket, &QTcpSocket::readyRead, this, &TTSWrapper::onSocketReadyRead);
    connect(m_socket, &QTcpSocket::connected, this, &TTSWrapper::onSocketConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &TTSWrapper::onSocketDisconnected);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &TTSWrapper::onSocketErrorOccurred);

    // Подключаем AudioPlayer
    connect(m_audioPlayer, &AudioPlayer::playbackStarted, this, &TTSWrapper::onAudioPlaybackStarted);
    connect(m_audioPlayer, &AudioPlayer::playbackFinished, this, &TTSWrapper::onAudioPlaybackFinished);
    connect(m_audioPlayer, &AudioPlayer::playbackStopped, this, &TTSWrapper::onAudioPlaybackStopped);
    connect(m_audioPlayer, &AudioPlayer::errorOccurred, this, [this](int playId, const QString &err) {
        int speakId = findSpeakIdByPlayId(playId);
        if (speakId != -1)
            emit ttsError(speakId, "Audio error: " + err);
    });

    // Инициализация (автозапуск сервера и подключение)
    initialize();
}

TTSWrapper::~TTSWrapper()
{
    shutdown();

    // Останавливаем Python сервер если мы его запустили
    if (m_serverStartedByUs) {
        stopPythonServer();
    }
}

bool TTSWrapper::initialize()
{
    qDebug() << "TTSWrapper: Initializing...";
    qDebug() << "  Server:" << m_host << ":" << m_port;
    qDebug() << "  Python script:" << m_pythonScriptPath;
    qDebug() << "  Auto-start:" << m_autoStartServer;

    // Ищем Python исполняемый файл
    if (!findPythonExecutable()) {
        qWarning() << "TTSWrapper: Python not found";
        emit serverError("Python not found");
    }

    // Проверяем наличие скрипта
    if (m_autoStartServer && !checkScriptExists()) {
        qWarning() << "TTSWrapper: Python script not found:" << m_pythonScriptPath;
        emit serverError("Python script not found: " + m_pythonScriptPath);
    }

    // Если автозапуск включен, запускаем сервер
    if (m_autoStartServer) {
        if (!startPythonServer()) {
            qWarning() << "TTSWrapper: Failed to start Python server";
        } else {
            QThread::msleep(3000);
        }
    }

    // Подключаемся к серверу
    m_socket->connectToHost(m_host, m_port);

    qDebug() << "TTSWrapper: Initialized successfully";
    return true;
}

void TTSWrapper::shutdown()
{
    qDebug() << "TTSWrapper: Shutting down...";

    stopAll();

    if (m_socket->isOpen()) {
        m_socket->disconnectFromHost();
        if (m_socket->state() != QAbstractSocket::UnconnectedState) {
            m_socket->waitForDisconnected(1000);
        }
    }

    // Останавливаем Python сервер если мы его запустили
    if (m_serverStartedByUs) {
        stopPythonServer();
    }

    qDebug() << "TTSWrapper: Shutdown complete";
}

void TTSWrapper::onSocketConnected()
{
    qDebug() << "TTS: connected to server";
    emit serverStarted();
}

void TTSWrapper::onSocketDisconnected()
{
    qDebug() << "TTS: disconnected from server";
    emit serverStopped();
}

void TTSWrapper::onSocketErrorOccurred(QAbstractSocket::SocketError socketError)
{
    qDebug() << "TTS socket error:" << socketError << m_socket->errorString();
}

int TTSWrapper::speak(const QString &text)
{
    if (text.isEmpty()) {
        emit ttsError(-1, "Пустой текст");
        return -1;
    }

    int speakId = m_nextSpeakId++;
    m_synthesisQueue.push_back({speakId, text});

    qDebug() << "Добавлен в очередь синтеза:" << speakId << text.left(80);

    if (!m_isSynthesizing)
        processNextInQueue();

    return speakId;
}

void TTSWrapper::processNextInQueue()
{
    if (m_isSynthesizing || m_synthesisQueue.empty())
        return;

    auto [speakId, text] = m_synthesisQueue.front();
    m_synthesisQueue.pop_front();

    m_currentSpeakId = speakId;
    m_isSynthesizing = true;

#ifdef KATHUB_ENABLE_POCKET_TTS
    // ── Нативный PocketTTS ──
    if (m_useNativeTTS && m_pocketTTS && m_pocketTTS->isInitialized() && m_pocketTTS->hasSpeaker()) {
        emit ttsStarted(speakId, text);

        auto audio = m_pocketTTS->synthesize(text);
        if (audio.samples.empty()) {
            emit ttsError(speakId, QStringLiteral("PocketTTS: пустой результат синтеза"));
            m_isSynthesizing = false;
            m_currentSpeakId = -1;
            processNextInQueue();
            return;
        }

        // Сохраняем PCM → WAV и отдаём AudioPlayer
        QString wavPath = savePcmToWav(audio.samples, audio.sampleRate);
        if (wavPath.isEmpty()) {
            emit ttsError(speakId, QStringLiteral("PocketTTS: не удалось сохранить WAV"));
            m_isSynthesizing = false;
            m_currentSpeakId = -1;
            processNextInQueue();
            return;
        }

        int playId = m_audioPlayer->deleteAfterPlay(wavPath, false);
        if (playId != -1) {
            m_speakIdToPlayId[speakId] = playId;
            m_playIdToSpeakId[playId] = speakId;
            emit ttsFinished(speakId, wavPath);
        }

        m_isSynthesizing = false;
        m_currentSpeakId = -1;
        processNextInQueue();
        return;
    }
#endif

    // ── Python-сервер (TCP) ──
    QJsonObject obj;
    obj["event"] = "input";
    obj["payload"] = text;

    sendJson(obj);

    emit ttsStarted(speakId, text);
}

void TTSWrapper::sendJson(const QJsonObject &obj)
{
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState) {
        qWarning() << "TTS: not connected, cannot send";
        return;
    }

    QJsonDocument doc(obj);
    QByteArray data = doc.toJson(QJsonDocument::Compact);
    quint32 len = data.size();

    // Ручная упаковка 4 байт big-endian
    QByteArray sizeData;
    sizeData.append(static_cast<char>((len >> 24) & 0xFF));
    sizeData.append(static_cast<char>((len >> 16) & 0xFF));
    sizeData.append(static_cast<char>((len >> 8) & 0xFF));
    sizeData.append(static_cast<char>(len & 0xFF));

    m_socket->write(sizeData);
    m_socket->write(data);
}

void TTSWrapper::sendInterrupt()
{
    QJsonObject obj{{"event", "interrupt"}};
    sendJson(obj);
}

void TTSWrapper::onSocketReadyRead()
{
    m_recvBuffer.append(m_socket->readAll());

    while (m_recvBuffer.size() >= 4) {
        quint32 msgSize = qFromBigEndian<quint32>(m_recvBuffer.constData());
        if (m_recvBuffer.size() < 4 + msgSize)
            break;

        QByteArray msgData = m_recvBuffer.mid(4, msgSize);
        m_recvBuffer.remove(0, 4 + msgSize);

        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(msgData, &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) {
            qDebug() << "TTS: invalid JSON:" << msgData;
            continue;
        }

        QJsonObject obj = doc.object();
        QString type = obj["type"].toString();

        if (type == "audio_file") {
            QString path = obj["path"].toString();
            if (!path.isEmpty() && QFile::exists(path)) {
                int playId = m_audioPlayer->deleteAfterPlay(path, false);

                if (playId != -1 && m_currentSpeakId != -1) {
                    m_speakIdToPlayId[m_currentSpeakId] = playId;
                    m_playIdToSpeakId[playId] = m_currentSpeakId;

                    emit ttsFinished(m_currentSpeakId, path);
                }
            }

            // Завершаем текущий синтез (файл получен)
            m_isSynthesizing = false;
            m_currentSpeakId = -1;
            processNextInQueue();
        }
        else if (type == "final") {
            // Синтез завершён без сохранения файла
            m_isSynthesizing = false;
            m_currentSpeakId = -1;
            processNextInQueue();
        }
        else if (type == "error") {
            QString msg = obj["text"].toString();
            emit ttsError(m_currentSpeakId, msg);
            qDebug() << "TTS err:" << msg;
            m_isSynthesizing = false;
            m_currentSpeakId = -1;
            processNextInQueue();
        }
        else if (type == "log") {
            qDebug() << "TTS log:" << obj["text"].toString();
        }
        // Другие типы (audio, ready) игнорируем или логируем
    }
}

void TTSWrapper::stop(int id)
{
    // Это playId от AudioPlayer?
    auto it = m_playIdToSpeakId.find(id);
    if (it != m_playIdToSpeakId.end()) {
        m_audioPlayer->stop(id);
        return;
    }

    // Это speakId — текущий синтез
    if (id == m_currentSpeakId && m_isSynthesizing) {
#ifdef KATHUB_ENABLE_POCKET_TTS
        if (m_useNativeTTS && m_pocketTTS) {
            m_pocketTTS->stop();
        } else {
            sendInterrupt();
        }
#else
        sendInterrupt();
#endif
        m_isSynthesizing = false;
        m_currentSpeakId = -1;
        return;
    }

    // Ищем в очереди и удаляем
    for (auto itQ = m_synthesisQueue.begin(); itQ != m_synthesisQueue.end(); ++itQ) {
        if (itQ->first == id) {
            m_synthesisQueue.erase(itQ);
            emit ttsError(id, "Запрос удалён из очереди");
            return;
        }
    }
}

void TTSWrapper::stopAll()
{
    clearQueue();
    if (m_isSynthesizing) {
#ifdef KATHUB_ENABLE_POCKET_TTS
        if (m_useNativeTTS && m_pocketTTS) {
            m_pocketTTS->stop();
        } else {
            sendInterrupt();
        }
#else
        sendInterrupt();
#endif
        m_isSynthesizing = false;
        m_currentSpeakId = -1;
    }
    m_audioPlayer->stopAll();
    m_speakIdToPlayId.clear();
    m_playIdToSpeakId.clear();
#ifdef Q_OS_WIN
    restoreAudio();
#endif
}

void TTSWrapper::clearQueue()
{
    m_synthesisQueue.clear();
}

void TTSWrapper::resetTTS()
{
    QJsonObject obj{{"event", "reset"}};
    sendJson(obj);
    clearQueue();
}

void TTSWrapper::interruptCurrentSynthesis()
{
    if (m_isSynthesizing) {
#ifdef KATHUB_ENABLE_POCKET_TTS
        if (m_useNativeTTS && m_pocketTTS) {
            m_pocketTTS->stop();
        } else {
            sendInterrupt();
        }
#else
        sendInterrupt();
#endif
        m_isSynthesizing = false;
        m_currentSpeakId = -1;
    }
}

void TTSWrapper::stopCurrent()
{
    if (m_isSynthesizing && m_currentSpeakId != -1)
        stop(m_currentSpeakId);
    else if (!m_synthesisQueue.empty())
        stop(m_synthesisQueue.front().first);
}

bool TTSWrapper::isSpeaking(int id) const
{
    if (id == m_currentSpeakId && m_isSynthesizing)
        return true;
    for (const auto& p : m_synthesisQueue)
        if (p.first == id) return true;
    return m_audioPlayer->isPlaying(id);
}

bool TTSWrapper::isSpeakingAny() const
{
    return m_isSynthesizing || !m_synthesisQueue.empty() || m_audioPlayer->isPlayingAny();
}

void TTSWrapper::setServerAddress(const QString &host, quint16 port)
{
    m_host = host;
    m_port = port;
}

int TTSWrapper::findSpeakIdByPlayId(int playId) const
{
    auto it = m_playIdToSpeakId.find(playId);
    return it != m_playIdToSpeakId.end() ? it->second : -1;
}

void TTSWrapper::onAudioPlaybackFinished(int playId, const QString &)
{
    int speakId = findSpeakIdByPlayId(playId);
    if (speakId != -1) {
        m_speakIdToPlayId.erase(speakId);
        m_playIdToSpeakId.erase(playId);
    }
#ifdef Q_OS_WIN
    restoreAudio();
#endif
    emit playbackFinished(playId);
}

void TTSWrapper::onAudioPlaybackStopped(int playId, const QString &)
{
    int speakId = findSpeakIdByPlayId(playId);
    if (speakId != -1) {
        m_speakIdToPlayId.erase(speakId);
        m_playIdToSpeakId.erase(playId);
    }
#ifdef Q_OS_WIN
    restoreAudio();
#endif
    emit playbackStopped(playId);
}

void TTSWrapper::onAudioPlaybackStarted(int playId, const QString &)
{
#ifdef Q_OS_WIN
    duckOtherAudio();
#endif
    emit playbackStarted(playId);
}

// ============================================================================
// Управление Python сервером
// ============================================================================

bool TTSWrapper::startPythonServer()
{
    if (m_serverProcess->state() != QProcess::NotRunning) {
        qDebug() << "TTSWrapper: Python server already running";
        return true;
    }

    qDebug() << "TTSWrapper: Starting Python server...";
    qDebug() << "  Executable:" << m_pythonExecutable;
    qDebug() << "  Script:" << m_pythonScriptPath;
    qDebug() << "  Host:" << m_host;
    qDebug() << "  Port:" << m_port;

    // Аргументы для Python скрипта
    QStringList arguments;
    arguments << m_pythonScriptPath;
    arguments << "--host" << m_host;
    arguments << "--port" << QString::number(m_port);

    // Запускаем процесс
    m_serverProcess->start(m_pythonExecutable, arguments);

    if (!m_serverProcess->waitForStarted(5000)) {
        qWarning() << "TTSWrapper: Failed to start Python server:"
                   << m_serverProcess->errorString();
        return false;
    }

    m_serverStartedByUs = true;
    qDebug() << "TTSWrapper: Python server started successfully";

    return true;
}

void TTSWrapper::stopPythonServer()
{
    if (!m_serverStartedByUs || m_serverProcess->state() == QProcess::NotRunning) {
        return;
    }

    qDebug() << "TTSWrapper: Stopping Python server...";

    // Пытаемся завершить корректно
    m_serverProcess->terminate();

    if (!m_serverProcess->waitForFinished(3000)) {
        qWarning() << "TTSWrapper: Server didn't stop gracefully, killing...";
        m_serverProcess->kill();
        m_serverProcess->waitForFinished(1000);
    }

    m_serverStartedByUs = false;
    qDebug() << "TTSWrapper: Python server stopped";
}

bool TTSWrapper::findPythonExecutable()
{
    // Проверяем указанный путь
    if (!m_pythonExecutable.isEmpty() && QFile::exists(m_pythonExecutable)) {
        qDebug() << "TTSWrapper: Found Python:" << m_pythonExecutable;
        return true;
    }

    // Список возможных имен Python исполняемого файла
    QStringList candidates = {"python3", "python", "py"};

    for (const QString& candidate : candidates) {
        QProcess testProcess;
        testProcess.start(candidate, QStringList() << "--version");

        if (testProcess.waitForFinished(2000)) {
            QString output = testProcess.readAllStandardOutput();
            if (output.contains("Python")) {
                m_pythonExecutable = candidate;
                qDebug() << "TTSWrapper: Found Python:" << candidate;
                qDebug() << "  Version:" << output.trimmed();
                return true;
            }
        }
    }

    qWarning() << "TTSWrapper: Python executable not found";
    return false;
}

bool TTSWrapper::checkScriptExists()
{
    // Проверяем в текущей директории
    if (QFile::exists(m_pythonScriptPath)) {
        qDebug() << "TTSWrapper: Found script:" << m_pythonScriptPath;
        return true;
    }

    // Проверяем рядом с исполняемым файлом приложения
    QString appDir = QCoreApplication::applicationDirPath();
    QString scriptInAppDir = QDir(appDir).filePath(m_pythonScriptPath);

    if (QFile::exists(scriptInAppDir)) {
        m_pythonScriptPath = scriptInAppDir;
        qDebug() << "TTSWrapper: Found script in app dir:" << m_pythonScriptPath;
        return true;
    }

    // Проверяем в поддиректории scripts
    QString scriptInScriptsDir = QDir(appDir).filePath("scripts/" + m_pythonScriptPath);

    if (QFile::exists(scriptInScriptsDir)) {
        m_pythonScriptPath = scriptInScriptsDir;
        qDebug() << "TTSWrapper: Found script in scripts dir:" << m_pythonScriptPath;
        return true;
    }

    qWarning() << "TTSWrapper: Python script not found:" << m_pythonScriptPath;
    qWarning() << "  Tried paths:";
    qWarning() << "   " << m_pythonScriptPath;
    qWarning() << "   " << scriptInAppDir;
    qWarning() << "   " << scriptInScriptsDir;

    return false;
}

void TTSWrapper::onServerStarted()
{
    qDebug() << "TTSWrapper: Python server process started";
    emit serverStarted();
}

void TTSWrapper::onServerFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    qDebug() << "TTSWrapper: Python server process finished";
    qDebug() << "  Exit code:" << exitCode;
    qDebug() << "  Exit status:" << (exitStatus == QProcess::NormalExit ? "Normal" : "Crashed");

    m_serverStartedByUs = false;
    emit serverStopped();

    if (exitStatus == QProcess::CrashExit) {
        emit serverError("Python server crashed");
    }
}

void TTSWrapper::onServerErrorOccurred(QProcess::ProcessError error)
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

    qWarning() << "TTSWrapper: Python server error:" << errorStr;
    emit serverError(errorStr);
}

void TTSWrapper::onServerReadyReadStandardOutput()
{
    QString output = QString::fromUtf8(m_serverProcess->readAllStandardOutput());

    // Выводим в консоль
    qDebug().noquote() << "TTS server:" << output.trimmed();

    // Отправляем сигнал
    emit serverOutput(output);
}

void TTSWrapper::onServerReadyReadStandardError()
{
    QString errorOutput = QString::fromUtf8(m_serverProcess->readAllStandardError());

    // Выводим в консоль
    if (!errorOutput.trimmed().isEmpty()) {
        qWarning().noquote() << "TTS server (stderr):" << errorOutput.trimmed();
        emit serverError(errorOutput);
    }
}

// ============================================================================
// Windows Audio Ducking — приглушаем все посторонние звуки на время TTS
// ============================================================================
#ifdef Q_OS_WIN

void TTSWrapper::duckOtherAudio()
{
    if (m_audioDucked) return;

    HRESULT hr;
    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDevice* device = nullptr;
    IAudioSessionManager2* sessionManager = nullptr;

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                          CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                          (void**)&enumerator);
    if (FAILED(hr)) return;

    hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    if (FAILED(hr)) { enumerator->Release(); return; }

    hr = device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL,
                          nullptr, (void**)&sessionManager);
    if (FAILED(hr)) { device->Release(); enumerator->Release(); return; }

    IAudioSessionEnumerator* sessionEnum = nullptr;
    hr = sessionManager->GetSessionEnumerator(&sessionEnum);
    if (FAILED(hr)) {
        sessionManager->Release(); device->Release(); enumerator->Release();
        return;
    }

    int count = 0;
    sessionEnum->GetCount(&count);

    DWORD ourPid = GetCurrentProcessId();        
    static const float DUCK_VOLUME = 0.25f;      

    m_originalVolumes.clear();

    for (int i = 0; i < count; ++i) {
        IAudioSessionControl* ctrl = nullptr;
        if (FAILED(sessionEnum->GetSession(i, &ctrl))) continue;

        IAudioSessionControl2* ctrl2 = nullptr;
        if (SUCCEEDED(ctrl->QueryInterface(__uuidof(IAudioSessionControl2),
                                             (void**)&ctrl2))) {
            DWORD pid = 0;
            ctrl2->GetProcessId(&pid);

            if (pid != 0 && pid != ourPid) {
                ISimpleAudioVolume* vol = nullptr;
                if (SUCCEEDED(ctrl2->QueryInterface(__uuidof(ISimpleAudioVolume),
                                                      (void**)&vol))) {
                    float origVol = 0.0f;
                    vol->GetMasterVolume(&origVol);
                    if (origVol > DUCK_VOLUME + 0.01f) {
                        m_originalVolumes[pid] = origVol;
                        vol->SetMasterVolume(DUCK_VOLUME, nullptr);
                    }
                    vol->Release();
                }
            }
            ctrl2->Release();
        }
        ctrl->Release();
    }

    if (!m_originalVolumes.empty())
        m_audioDucked = true;

    sessionEnum->Release();
    sessionManager->Release();
    device->Release();
    enumerator->Release();
}

void TTSWrapper::restoreAudio()
{
    if (!m_audioDucked) return;

    HRESULT hr;
    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDevice* device = nullptr;
    IAudioSessionManager2* sessionManager = nullptr;

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                          CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                          (void**)&enumerator);
    if (FAILED(hr)) return;

    hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    if (FAILED(hr)) { enumerator->Release(); return; }

    hr = device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL,
                          nullptr, (void**)&sessionManager);
    if (FAILED(hr)) { device->Release(); enumerator->Release(); return; }

    IAudioSessionEnumerator* sessionEnum = nullptr;
    hr = sessionManager->GetSessionEnumerator(&sessionEnum);
    if (FAILED(hr)) {
        sessionManager->Release(); device->Release(); enumerator->Release();
        return;
    }

    int count = 0;
    sessionEnum->GetCount(&count);

    for (int i = 0; i < count; ++i) {
        IAudioSessionControl* ctrl = nullptr;
        if (FAILED(sessionEnum->GetSession(i, &ctrl))) continue;

        IAudioSessionControl2* ctrl2 = nullptr;
        if (SUCCEEDED(ctrl->QueryInterface(__uuidof(IAudioSessionControl2),
                                             (void**)&ctrl2))) {
            DWORD pid = 0;
            ctrl2->GetProcessId(&pid);

            auto it = m_originalVolumes.find(pid);
            if (it != m_originalVolumes.end()) {
                ISimpleAudioVolume* vol = nullptr;
                if (SUCCEEDED(ctrl2->QueryInterface(__uuidof(ISimpleAudioVolume),
                                                      (void**)&vol))) {
                    vol->SetMasterVolume(it->second, nullptr);
                    vol->Release();
                }
            }
            ctrl2->Release();
        }
        ctrl->Release();
    }

    m_originalVolumes.clear();
    m_audioDucked = false;

    sessionEnum->Release();
    sessionManager->Release();
    device->Release();
    enumerator->Release();
}

#endif

#ifdef KATHUB_ENABLE_POCKET_TTS
// ═════════════════════════════════════════════════════════════════════════════
// PocketTTS (нативный TTS) — методы
// ═════════════════════════════════════════════════════════════════════════════

bool TTSWrapper::initPocketTTS(const QString &modelsDir)
{
    if (!m_pocketTTS) {
        m_pocketTTS = new PocketTTSEngine(this);
    }
    return m_pocketTTS->initialize(modelsDir);
}

bool TTSWrapper::loadPocketTTSSpeaker(const QString &audioPath)
{
    if (!m_pocketTTS || !m_pocketTTS->isInitialized()) {
        qWarning() << "PocketTTS не инициализирован";
        return false;
    }
    return m_pocketTTS->loadSpeaker(audioPath);
}

bool TTSWrapper::isNativeTTSReady() const
 { return m_pocketTTS && m_pocketTTS->isInitialized(); }

QString TTSWrapper::savePcmToWav(const std::vector<float>& samples, int sampleRate)
{
    if (samples.empty()) return {};

    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString path = tempDir + QStringLiteral("/tts_pocket_%1.wav")
        .arg(QDateTime::currentMSecsSinceEpoch());

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        qWarning() << "Не могу создать WAV:" << path;
        return {};
    }

    // WAV header (PCM float32 mono)
    uint32_t dataSize = samples.size() * sizeof(float);
    uint32_t chunkSize = 36 + dataSize;

    auto w32 = [&](uint32_t v) {
        char b[4];
        b[0] = v & 0xFF; b[1] = (v >> 8) & 0xFF;
        b[2] = (v >> 16) & 0xFF; b[3] = (v >> 24) & 0xFF;
        f.write(b, 4);
    };
    auto w16 = [&](uint16_t v) {
        char b[2];
        b[0] = v & 0xFF; b[1] = (v >> 8) & 0xFF;
        f.write(b, 2);
    };

    // RIFF header
    f.write("RIFF", 4);
    w32(chunkSize);
    f.write("WAVE", 4);

    // fmt chunk
    f.write("fmt ", 4);
    w32(18);                        // chunk size (PCM + extension)
    w16(3);                         // format = IEEE float
    w16(1);                         // channels = mono
    w32(sampleRate);
    w32(sampleRate * sizeof(float));// byte rate
    w16(sizeof(float));             // block align
    w16(32);                        // bits per sample

    // data chunk
    f.write("data", 4);
    w32(dataSize);
    f.write(reinterpret_cast<const char*>(samples.data()), dataSize);
    f.close();

    return path;
}
#endif
