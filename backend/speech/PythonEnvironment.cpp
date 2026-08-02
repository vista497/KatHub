#include "PythonEnvironment.h"
#include "AppPaths.h"
#include <QFileInfo>
#include <QDebug>

using namespace KatHub;  // KatHub::AppPaths

PythonEnvironment::PythonEnvironment(QObject *parent)
    : QObject(parent)
{
}

bool PythonEnvironment::isReady() const
{
    return QFileInfo::exists(AppPaths::pythonExePath())
           && checkDependenciesInstalled();
}

bool PythonEnvironment::setup()
{
    if (isReady()) {
        emit setupProgress(100, QStringLiteral("Python-окружение уже готово"));
        emit setupFinished(true);
        return true;
    }

    // Шаг 1: venv
    emit setupProgress(10, QStringLiteral("Создание виртуального окружения..."));
    if (!QFileInfo::exists(AppPaths::pythonExePath())) {
        if (!createVenv()) {
            emit setupFinished(false);
            return false;
        }
    }

    // Шаг 2: pip install
    emit setupProgress(30, QStringLiteral("Установка зависимостей Python..."));
    if (!installRequirements()) {
        emit setupFinished(false);
        return false;
    }

    emit setupProgress(100, QStringLiteral("Python-окружение готово"));
    emit setupFinished(true);
    return true;
}

QProcess *PythonEnvironment::runScript(const QString &scriptPath,
                                       const QStringList &args,
                                       const QString &workingDir)
{
    auto *proc = new QProcess(this);

    QString python = QFileInfo::exists(AppPaths::pythonExePath())
                     ? AppPaths::pythonExePath()
                     : QStringLiteral("python");

    QString wd = workingDir;
    if (wd.isEmpty())
        wd = QFileInfo(scriptPath).absolutePath();

    proc->setWorkingDirectory(wd);
    proc->setProcessChannelMode(QProcess::SeparateChannels);

    QStringList fullArgs;
    fullArgs << QStringLiteral("-u") << scriptPath;
    fullArgs << args;

    qDebug() << "[PythonEnvironment] Starting:" << python << fullArgs;
    proc->start(python, fullArgs);

    return proc;
}

bool PythonEnvironment::pipInstall(const QString &package)
{
    QProcess proc;
    proc.start(AppPaths::pipExePath(), {QStringLiteral("install"), package});
    proc.waitForFinished(120000);
    return proc.exitCode() == 0;
}

// ─── private ─────────────────────────────────────────────────────

bool PythonEnvironment::createVenv()
{
    const QString venvDir = AppPaths::pythonVenvDir();

    QProcess proc;
    proc.start(QStringLiteral("python"),
               {QStringLiteral("-m"), QStringLiteral("venv"), venvDir});
    proc.waitForFinished(60000);

    if (proc.exitCode() != 0) {
        QString err = QStringLiteral("Не удалось создать venv: ")
                      + QString::fromUtf8(proc.readAllStandardError());
        emit errorOccurred(err);
        qWarning() << "[PythonEnvironment]" << err;
        return false;
    }

    qDebug() << "[PythonEnvironment] venv создан:" << venvDir;
    return true;
}

bool PythonEnvironment::installRequirements()
{
    const QString reqPath = AppPaths::sttRequirementsPath();

    if (!QFileInfo::exists(reqPath)) {
        qDebug() << "[PythonEnvironment] requirements.txt не найден, пропускаем";
        return true;
    }

    QProcess proc;
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start(AppPaths::pipExePath(),
               {QStringLiteral("install"), QStringLiteral("-r"), reqPath});

    int lastPercent = 30;
    while (proc.state() != QProcess::NotRunning) {
        proc.waitForReadyRead(3000);
        QString output = QString::fromUtf8(proc.readAll());

        if (output.contains(QStringLiteral("Successfully installed")))
            lastPercent = 80;
        else if (output.contains(QStringLiteral("Downloading")))
            lastPercent = qMin(lastPercent + 2, 70);

        emit setupProgress(lastPercent,
                           QStringLiteral("pip install... %1%").arg(lastPercent));
    }

    proc.waitForFinished(300000);

    if (proc.exitCode() != 0) {
        QString err = QStringLiteral("pip install завершился с ошибкой: ")
                      + QString::fromUtf8(proc.readAll());
        emit errorOccurred(err);
        qWarning() << "[PythonEnvironment]" << err;
        return false;
    }

    qDebug() << "[PythonEnvironment] Зависимости установлены";
    return true;
}

bool PythonEnvironment::checkDependenciesInstalled() const
{
    QProcess proc;
    proc.start(AppPaths::pythonExePath(), {
        QStringLiteral("-c"),
        QStringLiteral("import faster_whisper, whisperx, numpy; print('OK')")
    });
    proc.waitForFinished(10000);
    return proc.exitCode() == 0;
}
