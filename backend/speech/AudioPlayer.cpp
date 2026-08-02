#include "AudioPlayer.h"
#include <QDebug>
#include <QTimer>
#include <QFileInfo>

// Инициализация статического экземпляра (без мьютекса — предполагаем однопоточный доступ)
QScopedPointer<AudioPlayer> AudioPlayer::s_instance;

AudioPlayer* AudioPlayer::instance()
{
    if (!s_instance) {
        s_instance.reset(new AudioPlayer());
    }
    return s_instance.data();
}

void AudioPlayer::destroy()
{
    s_instance.reset();
}

AudioPlayer::AudioPlayer(QObject *parent)
    : QObject(parent)
    , m_nextPlayId(1)
{
}

AudioPlayer::~AudioPlayer()
{
    stopAll();
}

int AudioPlayer::playFile(const QString &filePath)
{
    // Проверяем существование файла
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        qWarning() << "Audio file does not exist:" << filePath;
        return -1;
    }

    int playId = m_nextPlayId++;

    QMediaPlayer* player = new QMediaPlayer(this);
    QAudioOutput* audioOutput = new QAudioOutput(player);
    audioOutput->setVolume(1.0); // Maximum volume
    player->setAudioOutput(audioOutput);
    AudioPlayback playback(player, filePath, false);

    // Сигнал позиции воспроизведения
    connect(player, &QMediaPlayer::positionChanged,
            this, [this, playId, player](qint64 position) {
                qint64 duration = player->duration();
                emit positionChanged(playId, position, duration);
            });

    // Сигнал длительности (для инициализации)
    connect(player, &QMediaPlayer::durationChanged,
            this, [this, playId](qint64 duration) {
                emit positionChanged(playId, 0, duration);
            });

    // Лямбда будет выполнена в главном потоке благодаря singleShot
    connect(player, &QMediaPlayer::playbackStateChanged,
            this, [this, playId, player](QMediaPlayer::PlaybackState state)
            {
                if (state == QMediaPlayer::StoppedState)
                {
                    QTimer::singleShot(0, this, [this, playId]() {
                        onPlaybackFinished();
                    });
                }
            });

    connect(player, &QMediaPlayer::errorOccurred,
            this, [this, playId](QMediaPlayer::Error /*error*/, const QString &errorString)
            {
                qWarning() << "AudioPlayer error for" << playId << ":" << errorString;
                emit errorOccurred(playId, errorString);
            });

    player->setSource(QUrl::fromLocalFile(filePath));
    player->play();

    m_activePlaybacks.insert(playId, playback);

    qDebug() << "Started audio playback" << playId << "for file:" << filePath;
    emit playbackStarted(playId, filePath);

    return playId;
}

int AudioPlayer::deleteAfterPlay(const QString &filePath, bool deleteImmediately)
{
    if (deleteImmediately)
    {
        QFile file(filePath);
        if (file.exists())
        {
            if (!file.remove())
            {
                qWarning() << "Failed to delete audio file immediately:" << filePath;
            }
        }
        return -1;
    }

    // Аналогично playFile, только с флагом deleteAfterPlay = true
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        qWarning() << "Audio file does not exist:" << filePath;
        return -1;
    }

    int playId = m_nextPlayId++;

    QMediaPlayer* player = new QMediaPlayer(this);
    QAudioOutput* audioOutput = new QAudioOutput(player);
    audioOutput->setVolume(1.0); // Maximum volume
    player->setAudioOutput(audioOutput);

    AudioPlayback playback(player, filePath, true);

    connect(player, &QMediaPlayer::playbackStateChanged,
            this, [this, playId, player](QMediaPlayer::PlaybackState state)
            {
                if (state == QMediaPlayer::StoppedState)
                {
                    QTimer::singleShot(0, this, [this, playId]() {
                        onPlaybackFinished();
                    });
                }
            });

    connect(player, &QMediaPlayer::errorOccurred,
            this, [this, playId](QMediaPlayer::Error /*error*/, const QString &errorString)
            {
                qWarning() << "AudioPlayer error for" << playId << ":" << errorString;
                emit errorOccurred(playId, errorString);
            });

    player->setSource(QUrl::fromLocalFile(filePath));
    player->play();

    m_activePlaybacks.insert(playId, playback);

    qDebug() << "Started audio playback with auto-delete" << playId << "for file:" << filePath;
    emit playbackStarted(playId, filePath);

    return playId;
}

void AudioPlayer::stop(int playId)
{
    if (!m_activePlaybacks.contains(playId))
        return;

    AudioPlayback &playback = m_activePlaybacks[playId];
    QMediaPlayer* player = playback.player;

    if (player && (player->playbackState() == QMediaPlayer::PlayingState ||
                   player->playbackState() == QMediaPlayer::PausedState))
    {
        player->stop();

        QString filePath = playback.filePath;

        if (playback.deleteAfterPlay)
        {
            player->setSource(QUrl());  // освободить файл перед удалением (Windows)
            QFile file(filePath);
            if (file.exists() && !file.remove())
            {
                qWarning() << "Failed to delete audio file after stop:" << filePath;
            }
        }

        player->deleteLater();
        m_activePlaybacks.remove(playId);

        qDebug() << "Stopped audio playback" << playId << "for file:" << filePath;
        emit playbackStopped(playId, filePath);
    }
}

void AudioPlayer::pause(int playId)
{
    if (m_activePlaybacks.contains(playId))
    {
        QMediaPlayer* player = m_activePlaybacks[playId].player;
        if (player && player->playbackState() == QMediaPlayer::PlayingState)
        {
            player->pause();
        }
    }
}

void AudioPlayer::resume(int playId)
{
    if (m_activePlaybacks.contains(playId))
    {
        QMediaPlayer* player = m_activePlaybacks[playId].player;
        if (player && player->playbackState() == QMediaPlayer::PausedState)
        {
            player->play();
        }
    }
}

void AudioPlayer::stopAll()
{
    // Собираем ключи заранее, чтобы безопасно удалять во время итерации
    QList<int> playIds = m_activePlaybacks.keys();

    for (int playId : playIds)
    {
        if (m_activePlaybacks.contains(playId))
        {
            AudioPlayback &playback = m_activePlaybacks[playId];
            QMediaPlayer* player = playback.player;

            if (player && (player->playbackState() == QMediaPlayer::PlayingState ||
                           player->playbackState() == QMediaPlayer::PausedState))
            {
                player->stop();

                QString filePath = playback.filePath;

                if (playback.deleteAfterPlay)
                {
                    player->setSource(QUrl());  // освободить файл перед удалением (Windows)
                    QFile file(filePath);
                    if (file.exists() && !file.remove())
                    {
                        qWarning() << "Failed to delete audio file after stopAll:" << filePath;
                    }
                }

                player->deleteLater();
                qDebug() << "Stopped audio playback" << playId << "for file:" << filePath;
                emit playbackStopped(playId, filePath);
            }
        }
    }

    m_activePlaybacks.clear();
}

void AudioPlayer::pauseAll()
{
    for (auto &playback : m_activePlaybacks)
    {
        QMediaPlayer* player = playback.player;
        if (player && player->playbackState() == QMediaPlayer::PlayingState)
        {
            player->pause();
        }
    }
}

void AudioPlayer::resumeAll()
{
    for (auto &playback : m_activePlaybacks)
    {
        QMediaPlayer* player = playback.player;
        if (player && player->playbackState() == QMediaPlayer::PausedState)
        {
            player->play();
        }
    }
}

bool AudioPlayer::isPlaying(int playId) const
{
    if (m_activePlaybacks.contains(playId))
    {
        QMediaPlayer* player = m_activePlaybacks[playId].player;
        return player && player->playbackState() == QMediaPlayer::PlayingState;
    }
    return false;
}

bool AudioPlayer::isPlayingAny() const
{
    for (const auto &playback : m_activePlaybacks)
    {
        QMediaPlayer* player = playback.player;
        if (player && player->playbackState() == QMediaPlayer::PlayingState)
            return true;
    }
    return false;
}

QList<int> AudioPlayer::getActivePlayIds() const
{
    return m_activePlaybacks.keys();
}

void AudioPlayer::onPlaybackFinished()
{
    QList<int> finishedIds;

    for (auto it = m_activePlaybacks.constBegin(); it != m_activePlaybacks.constEnd(); ++it)
    {
        QMediaPlayer* player = it.value().player;
        if (player && player->playbackState() == QMediaPlayer::StoppedState)
        {
            finishedIds.append(it.key());
        }
    }

    for (int playId : finishedIds)
    {
        if (m_activePlaybacks.contains(playId))
        {
            AudioPlayback &playback = m_activePlaybacks[playId];
            QString filePath = playback.filePath;

            if (playback.deleteAfterPlay)
            {
                playback.player->setSource(QUrl());  // освободить файл перед удалением (Windows)
                QFile file(filePath);
                if (file.exists() && !file.remove())
                {
                    qWarning() << "Failed to delete audio file:" << filePath;
                }
            }

            playback.player->deleteLater();
            m_activePlaybacks.remove(playId);

            qDebug() << "Audio playback finished" << playId << "for file:" << filePath;
            emit playbackFinished(playId, filePath);
        }
    }
}
