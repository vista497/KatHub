#include "HermesCliHelper.h"

#include <QProcess>
#include <QStringList>
#include <QString>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>

std::string HermesCliHelper::findHermesExe()
{
    // 1. Explicit override.
    QByteArray override = qgetenv("HERMES_EXE");
    if (!override.isEmpty() && QFileInfo::exists(QString::fromLocal8Bit(override)))
        return override.toStdString();

    // 2. PATH lookup.
    QString pathHit = QStandardPaths::findExecutable(QStringLiteral("hermes"));
    if (!pathHit.isEmpty())
        return pathHit.toStdString();

    // 3. `where hermes` (same resolution KatHub already uses for python).
    {
        QProcess where;
        where.start(QStringLiteral("where"), {QStringLiteral("hermes")});
        where.waitForFinished(3000);
        QString out = QString::fromLocal8Bit(where.readAllStandardOutput());
        QString first = out.split('\n').first().trimmed();
        if (!first.isEmpty() && QFileInfo::exists(first))
            return first.toStdString();
    }

    // 4. Installed layout: hermes venv lives under %LOCALAPPDATA%/hermes.
    {
        QString localAppData = QDir::homePath() + QStringLiteral("/AppData/Local");
        QString venvExe = localAppData + QStringLiteral("/hermes/hermes-agent/venv/Scripts/hermes.exe");
        if (QFileInfo::exists(venvExe))
            return venvExe.toStdString();
    }

    // Last resort — let QProcess fail with a meaningful error.
    return "hermes";
}

std::string HermesCliHelper::run(const std::string& args)
{
    QProcess proc;
    // Split args by space
    QStringList argList;
    QString argStr = QString::fromStdString(args);
    for (const QString& part : argStr.split(' ', Qt::SkipEmptyParts)) {
        argList << part;
    }

    proc.start(QString::fromStdString(findHermesExe()), argList);
    proc.waitForFinished(5000);

    if (proc.exitCode() == 0) {
        return proc.readAllStandardOutput().toStdString();
    }
    return "";
}

// Locate the venv Python that owns the hermes CLI:
// `hermes` resolves to <venv>/Scripts/hermes.exe — python.exe sits next to it.
static QString findPythonExe()
{
    QProcess where;
    where.start("where", {"hermes"});
    where.waitForFinished(3000);
    QString out = QString::fromLocal8Bit(where.readAllStandardOutput());
    QString first = out.split('\n').first().trimmed();
    if (!first.isEmpty()) {
        QFileInfo fi(first);
        QDir scriptsDir = fi.absoluteDir();
        QString py = scriptsDir.filePath("python.exe");
        if (QFileInfo::exists(py)) return py;
    }
    // Fallback: rely on PATH (only useful if the venv Scripts dir is there)
    return "python";
}

std::string HermesCliHelper::runPython(const std::string& scriptPath, const std::vector<std::string>& args)
{
    QProcess proc;
    QStringList argList;
    argList << QString::fromStdString(scriptPath);
    for (const std::string& a : args) {
        argList << QString::fromStdString(a);
    }

    proc.start(findPythonExe(), argList);
    proc.waitForFinished(15000);

    if (proc.exitCode() == 0) {
        return proc.readAllStandardOutput().toStdString();
    }
    return "";
}

HermesCliHelper::CliResult HermesCliHelper::runArgv(const std::vector<std::string>& args,
                                                    int timeoutMs)
{
    CliResult result;
    QProcess proc;
    QStringList argList;
    for (const std::string& a : args) {
        argList << QString::fromStdString(a);
    }

    proc.start(QString::fromStdString(findHermesExe()), argList);
    if (timeoutMs < 0) {
        // Infinite wait — used for chat runs (agent may work for minutes).
        if (!proc.waitForFinished(-1)) {
            proc.kill();
            proc.waitForFinished(3000);
            result.exitCode = -2;
            result.timedOut = true;
            result.stderrText = "hermes CLI failed to start or was killed";
            return result;
        }
    } else if (!proc.waitForFinished(timeoutMs)) {
        // Timed out — kill and report.
        proc.kill();
        proc.waitForFinished(3000);
        result.exitCode = -2;  // timeout marker
        result.timedOut = true;
        result.stderrText = "hermes CLI timed out after " +
                            std::to_string(timeoutMs) + " ms";
        return result;
    }

    result.exitCode = proc.exitCode();
    result.stdoutText = proc.readAllStandardOutput().toStdString();
    result.stderrText = proc.readAllStandardError().toStdString();
    return result;
}
