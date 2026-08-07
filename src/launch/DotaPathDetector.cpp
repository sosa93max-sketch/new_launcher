#include "DotaPathDetector.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStringList>

namespace
{
const QStringList CommonPaths = {
    QStringLiteral("D:\\Games\\Dota 2 7.22g\\game\\bin\\win64\\dota2.exe"),
    QStringLiteral("C:\\Program Files (x86)\\Steam\\steamapps\\common\\dota 2 beta\\game\\bin\\win64\\dota2.exe"),
    QStringLiteral("C:\\Program Files\\Steam\\steamapps\\common\\dota 2 beta\\game\\bin\\win64\\dota2.exe"),
    QStringLiteral("D:\\Steam\\steamapps\\common\\dota 2 beta\\game\\bin\\win64\\dota2.exe"),
    QStringLiteral("E:\\Steam\\steamapps\\common\\dota 2 beta\\game\\bin\\win64\\dota2.exe"),
};

QString candidateFromSteamPath(const QString &steamPath)
{
    if (steamPath.isEmpty())
        return QString();
    const QString candidate = QDir(steamPath)
                                  .filePath(QStringLiteral(
                                      "steamapps\\common\\dota 2 beta\\game\\bin\\win64\\dota2.exe"));
    return QFileInfo::exists(candidate) ? candidate : QString();
}
}

namespace DotaPathDetector
{
QString detect()
{
    for (const auto &path : CommonPaths)
    {
        if (QFileInfo::exists(path))
            return path;
    }

#ifdef Q_OS_WIN
    // Steam registry keys; QSettings reads HKLM/HKCU directly on Windows.
    QSettings machine(QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Valve\\Steam"),
                      QSettings::NativeFormat);
    const QString steamPath = machine.value(QStringLiteral("InstallPath")).toString();
    const QString found = candidateFromSteamPath(steamPath);
    if (!found.isEmpty())
        return found;

    QSettings user(QStringLiteral("HKEY_CURRENT_USER\\SOFTWARE\\Valve\\Steam"),
                   QSettings::NativeFormat);
    const QString userSteamPath = user.value(QStringLiteral("SteamPath")).toString();
    return candidateFromSteamPath(userSteamPath);
#else
    return QString();
#endif
}
}
