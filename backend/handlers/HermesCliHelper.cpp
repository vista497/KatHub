#include "HermesCliHelper.h"

#include <QProcess>
#include <QStringList>

std::string HermesCliHelper::run(const std::string& args)
{
    QProcess proc;
    // Split args by space
    QStringList argList;
    QString argStr = QString::fromStdString(args);
    for (const QString& part : argStr.split(' ', Qt::SkipEmptyParts)) {
        argList << part;
    }

    proc.start("hermes", argList);
    proc.waitForFinished(5000);

    if (proc.exitCode() == 0) {
        return proc.readAllStandardOutput().toStdString();
    }
    return "";
}
