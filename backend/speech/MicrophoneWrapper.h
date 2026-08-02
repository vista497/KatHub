#pragma once
#include <QAudioFormat>
#include <QObject>
#include <QTimer>
#include <QThread>
#include <QFile>
#include <QAudioOutput>
#include <memory>
#include <vector>

class QAudioSource;
class QIODevice;

class MicrophoneWrapper : public QObject
{
    Q_OBJECT

public:
    explicit MicrophoneWrapper(QObject *parent = nullptr);
    ~MicrophoneWrapper();

    bool startRecording(const QString &outputFilePath = QString());
    void stopRecording();
    bool isRecording() const;

    // Захватить аудиоданные за последние секунды
    std::vector<float> getAudioData() const;

    // Частота дискретизации микрофона (16 кГц)
    int sampleRate() const { return SAMPLE_RATE; }

    // Очистить буфер аудиоданных
    void clearAudioData();

    /// Установить глубину кольцевого буфера истории (секунды, по умолч. 300)
    void setHistorySeconds(int seconds);

    /// Текущая глубина истории, сек
    int historySeconds() const { return m_historySeconds; }

    // Получить путь к записанному файлу
    QString recordedFilePath() const { return m_outputFilePath; }

signals:
    void audioBufferReady(const std::vector<float>& audioData);
    void recordingStarted(const QString &filePath);
    void recordingStopped(const QString &filePath);
    void errorOccurred(const QString& errorMessage);

private slots:
    void processAudioData();

private:
    void initializeAudioFormat();
    bool setupAudioInput();
    bool openOutputFile(const QString &filePath);
    void closeOutputFile();
    void writeAudioData(const QByteArray &data);

    QAudioFormat m_audioFormat;
    QAudioSource* m_audioSource;
    QIODevice* m_audioDevice;
    QByteArray m_audioBuffer;
    std::vector<float> m_floatBuffer;
    int m_historySeconds = 300;  // глубина кольцевого буфера, сек
    QTimer* m_processingTimer;
    bool m_isRecording;

    // Для записи в файл
    QFile m_outputFile;
    QString m_outputFilePath;
    QAudioOutput* m_audioOutput;

    const int SAMPLE_RATE = 16000;  // Whisper требует 16kHz
    const int SAMPLE_SIZE = 16;     // 16-bit samples
    const int CHANNEL_COUNT = 1;    // Mono
    const int BUFFER_SIZE_MS = 100; // Process audio every 100ms
};
