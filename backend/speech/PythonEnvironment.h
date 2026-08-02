#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

/**
 * @brief PythonEnvironment — управление Python venv для скриптов STT/TTS
 *
 * Использует AppPaths для всех путей. Никаких хардкодов.
 */
class PythonEnvironment : public QObject
{
    Q_OBJECT

public:
    explicit PythonEnvironment(QObject *parent = nullptr);

    /// Существует ли venv и установлены ли зависимости
    bool isReady() const;

    /// Создать venv (если нет) + установить зависимости из requirements.txt
    bool setup();

    /// Запустить Python-скрипт в venv. Возвращает QProcess (родитель = this).
    QProcess *runScript(const QString &scriptPath,
                        const QStringList &args = {},
                        const QString &workingDir = {});

    /// Установить пакет через pip в venv
    bool pipInstall(const QString &package);

signals:
    void setupProgress(int percent, const QString &step);
    void setupFinished(bool success);
    void errorOccurred(const QString &error);

private:
    bool createVenv();
    bool installRequirements();
    bool checkDependenciesInstalled() const;
};
