#include "Log.h"

#include "../config/ConfigStore.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QTextStream>

namespace
{
QMutex g_mutex;
}

QString Log::filePath()
{
    return QDir(ConfigStore::rootDir()).filePath(QStringLiteral("launcher.log"));
}

void Log::init()
{
    QDir().mkpath(ConfigStore::rootDir());
    line(QStringLiteral("D2Max Launcher started"));
}

void Log::line(const QString &message)
{
    QMutexLocker locker(&g_mutex);
    QFile file(filePath());
    if (!file.open(QIODevice::Append | QIODevice::WriteOnly | QIODevice::Text))
        return;

    QTextStream stream(&file);
    stream << QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))
           << QLatin1Char(' ') << message << QLatin1Char('\n');
}
