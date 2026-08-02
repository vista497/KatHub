#pragma once

#include <QObject>
#include <QString>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QFile>
#include <QMap>
#include <QMutex>
#include <QScopedPointer>

class AudioPlayer : public QObject
{
    Q_OBJECT

public:
    // Синглтон методы
    static AudioPlayer* instance();
    static void destroy();
    
    ~AudioPlayer();

    // Основные методы управления аудио
    int playFile(const QString &filePath); // Возвращает ID воспроизведения
    int deleteAfterPlay(const QString &filePath, bool deleteImmediately = false);
    
    // Управление конкретным аудио
    void stop(int playId);
    void pause(int playId);
    void resume(int playId);
    
    // Глобальное управление
    void stopAll();
    void pauseAll();
    void resumeAll();
    
    // Информация о состоянии
    bool isPlaying(int playId) const;
    bool isPlayingAny() const;
    QList<int> getActivePlayIds() const;

public slots:
    void onPlaybackFinished();

signals:
    void playbackStarted(int playId, const QString &filePath);
    void playbackFinished(int playId, const QString &filePath);
    void playbackStopped(int playId, const QString &filePath);
    void errorOccurred(int playId, const QString &errorMessage);
    void positionChanged(int playId, qint64 position, qint64 duration);

private:
    // Приватный конструктор для синглтона
    explicit AudioPlayer(QObject *parent = nullptr);
    
    // Запрет копирования
    AudioPlayer(const AudioPlayer&) = delete;
    AudioPlayer& operator=(const AudioPlayer&) = delete;

    struct AudioPlayback {
        QMediaPlayer* player;
        QString filePath;
        bool deleteAfterPlay;
        
        AudioPlayback() : player(nullptr), deleteAfterPlay(false) {}
        AudioPlayback(QMediaPlayer* p, const QString& path, bool del) 
            : player(p), filePath(path), deleteAfterPlay(del) {}
    };
    
    // Статический экземпляр
    static QScopedPointer<AudioPlayer> s_instance;
    static QMutex s_mutex;
    
    QMap<int, AudioPlayback> m_activePlaybacks;
    mutable QMutex m_mutex;
    int m_nextPlayId;
};
