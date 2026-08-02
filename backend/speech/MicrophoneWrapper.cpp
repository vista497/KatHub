#include "MicrophoneWrapper.h"

#include <QMediaDevices>
#include <QAudioSource>
#include <QAudioOutput>
#include <QDebug>
#include <QDir>
#include <cmath>

MicrophoneWrapper::MicrophoneWrapper(QObject *parent)
    : QObject(parent)
    , m_audioSource(nullptr)
    , m_audioDevice(nullptr)
    , m_processingTimer(nullptr)
    , m_isRecording(false)
    , m_audioOutput(nullptr)
{
    initializeAudioFormat();
    m_processingTimer = new QTimer(this);
    connect(m_processingTimer, &QTimer::timeout, this, &MicrophoneWrapper::processAudioData);
}

MicrophoneWrapper::~MicrophoneWrapper()
{
    stopRecording();
    delete m_audioSource;
    delete m_audioOutput;
}

void MicrophoneWrapper::initializeAudioFormat()
{
    m_audioFormat.setSampleRate(SAMPLE_RATE);
    m_audioFormat.setChannelCount(CHANNEL_COUNT);
    m_audioFormat.setSampleFormat(QAudioFormat::Int16);  // 16-bit samples
}

bool MicrophoneWrapper::setupAudioInput()
{
    QMediaDevices mediaDevices;
    QAudioDevice inputDevice = mediaDevices.defaultAudioInput();

    if (!inputDevice.isFormatSupported(m_audioFormat)) {
        qWarning() << "Default format not supported, trying to use nearest";
        m_audioFormat = inputDevice.preferredFormat();
    }

    if (!inputDevice.isFormatSupported(m_audioFormat)) {
        emit errorOccurred("Audio format not supported by the device");
        return false;
    }

    m_audioSource = new QAudioSource(inputDevice, m_audioFormat, this);
    return true;
}

bool MicrophoneWrapper::openOutputFile(const QString &filePath)
{
    m_outputFilePath = filePath;
    
    // Создаем директорию если не существует
    QFileInfo fileInfo(filePath);
    QDir dir = fileInfo.absoluteDir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    m_outputFile.setFileName(filePath);
    if (!m_outputFile.open(QIODevice::WriteOnly)) {
        emit errorOccurred("Failed to open output file: " + filePath);
        return false;
    }

    // Записываем WAV заголовок (44 байта)
    // Заголовок будет обновлен после записи с правильными размерами
    QByteArray header(44, 0);
    m_outputFile.write(header);

    return true;
}

void MicrophoneWrapper::closeOutputFile()
{
    if (!m_outputFile.isOpen()) {
        return;
    }

    qint64 dataSize = m_outputFile.size() - 44;  // Размер данных без заголовка
    qint64 totalSize = m_outputFile.size();
    
    m_outputFile.seek(0);

    // RIFF заголовок
    m_outputFile.write("RIFF", 4);
    
    // Размер файла - 8
    quint32 fileSize = static_cast<quint32>(totalSize - 8);
    m_outputFile.write(reinterpret_cast<const char*>(&fileSize), 4);
    
    // WAVE формат
    m_outputFile.write("WAVE", 4);
    
    // fmt подзаголовок
    m_outputFile.write("fmt ", 4);
    
    // Размер fmt подзаголовка (16 для PCM)
    quint32 fmtSize = 16;
    m_outputFile.write(reinterpret_cast<const char*>(&fmtSize), 4);
    
    // Формат аудио (1 = PCM)
    quint16 audioFormat = 1;
    m_outputFile.write(reinterpret_cast<const char*>(&audioFormat), 2);
    
    // Количество каналов
    quint16 channels = static_cast<quint16>(CHANNEL_COUNT);
    m_outputFile.write(reinterpret_cast<const char*>(&channels), 2);
    
    // Частота дискретизации
    quint32 sampleRate = static_cast<quint32>(SAMPLE_RATE);
    m_outputFile.write(reinterpret_cast<const char*>(&sampleRate), 4);
    
    // Скорость передачи данных (SampleRate * NumChannels * BitsPerSample/8)
    quint32 byteRate = SAMPLE_RATE * CHANNEL_COUNT * (SAMPLE_SIZE / 8);
    m_outputFile.write(reinterpret_cast<const char*>(&byteRate), 4);
    
    // Размер блока выравнивания (NumChannels * BitsPerSample/8)
    quint16 blockAlign = CHANNEL_COUNT * (SAMPLE_SIZE / 8);
    m_outputFile.write(reinterpret_cast<const char*>(&blockAlign), 2);
    
    // Бит на семпл
    quint16 bitsPerSample = static_cast<quint16>(SAMPLE_SIZE);
    m_outputFile.write(reinterpret_cast<const char*>(&bitsPerSample), 2);
    
    // data подзаголовок
    m_outputFile.write("data", 4);
    
    // Размер данных
    quint32 dataSize32 = static_cast<quint32>(dataSize);
    m_outputFile.write(reinterpret_cast<const char*>(&dataSize32), 4);

    m_outputFile.close();
    m_outputFilePath.clear();
}

void MicrophoneWrapper::writeAudioData(const QByteArray &data)
{
    if (m_outputFile.isOpen()) {
        m_outputFile.write(data);
    }
}

bool MicrophoneWrapper::startRecording(const QString &outputFilePath)
{
    if (m_isRecording) {
        return true;
    }

    if (!setupAudioInput()) {
        return false;
    }

    // Очищаем буферы
    m_audioBuffer.clear();
    m_floatBuffer.clear();

    // Открываем файл для записи если путь указан
    if (!outputFilePath.isEmpty()) {
        if (!openOutputFile(outputFilePath)) {
            return false;
        }
    }

    m_audioDevice = m_audioSource->start();
    if (!m_audioDevice) {
        emit errorOccurred("Failed to start audio device");
        closeOutputFile();
        return false;
    }

    connect(m_audioDevice, &QIODevice::readyRead, this, [this]() {
        QByteArray data = m_audioDevice->readAll();
        m_audioBuffer.append(data);
        
        // Записываем в файл напрямую
        if (m_outputFile.isOpen()) {
            writeAudioData(data);
        }
    });

    m_processingTimer->start(BUFFER_SIZE_MS);
    m_isRecording = true;

    emit recordingStarted(m_outputFilePath);
    return true;
}

void MicrophoneWrapper::stopRecording()
{
    if (!m_isRecording) {
        return;
    }

    m_processingTimer->stop();

    if (m_audioSource) {
        m_audioSource->stop();
        disconnect(m_audioDevice, nullptr, this, nullptr);
        m_audioDevice = nullptr;
    }

    // Закрываем файл с правильным заголовком
    QString filePath = m_outputFilePath;
    closeOutputFile();

    m_isRecording = false;
    emit recordingStopped(filePath);
}

bool MicrophoneWrapper::isRecording() const
{
    return m_isRecording;
}

void MicrophoneWrapper::processAudioData()
{
    if (m_audioBuffer.isEmpty()) {
        return;
    }
    
    // Конвертируем сырые байты в float массив
    const int16_t* rawData = reinterpret_cast<const int16_t*>(m_audioBuffer.constData());
    const int sampleCount = m_audioBuffer.size() / sizeof(int16_t);
    
    // Создаем вектор ТОЛЬКО для новых сэмплов
    std::vector<float> newSamples;
    newSamples.reserve(sampleCount);
    
    for (int i = 0; i < sampleCount; ++i) {
        // Конвертируем 16-bit signed int в float [-1.0, 1.0]
        float sample = static_cast<float>(rawData[i]) / 32768.0f;
        newSamples.push_back(sample);
    }
    
    // Добавляем в общий буфер истории (если он действительно нужен вам через getAudioData)
    m_floatBuffer.insert(m_floatBuffer.end(), newSamples.begin(), newSamples.end());
    
    // Ограничиваем размер истории (по умолчанию 5 минут), чтобы память не текла
    const size_t maxHistorySize = static_cast<size_t>(SAMPLE_RATE) * static_cast<size_t>(m_historySeconds);
    if (m_floatBuffer.size() > maxHistorySize) {
        size_t toRemove = m_floatBuffer.size() - maxHistorySize;
        m_floatBuffer.erase(m_floatBuffer.begin(), m_floatBuffer.begin() + static_cast<long>(toRemove));
    }
    
    // Очищаем буфер сырых данных
    m_audioBuffer.clear();
    
    // ВАЖНО: Отправляем сигнал только с НОВЫМИ данными
    emit audioBufferReady(newSamples);
}

std::vector<float> MicrophoneWrapper::getAudioData() const
{
    return m_floatBuffer;
}

void MicrophoneWrapper::clearAudioData()
{
    m_floatBuffer.clear();
    m_floatBuffer.shrink_to_fit();
}

void MicrophoneWrapper::setHistorySeconds(int seconds)
{
    if (seconds < 5) seconds = 5;       // минимум 5 сек
    if (seconds > 3600) seconds = 3600; // максимум 1 час
    m_historySeconds = seconds;
    clearAudioData();
}
