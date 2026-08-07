#include "DotaPathDetector.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSettings>
#include <QStringList>

namespace
{
const QStringList CommonPaths = {
    // The private 7.22g build the server targets.
    QStringLiteral("D:\\Games\\Dota 2 7.22g\\game\\bin\\win64\\dota2.exe"),
    // Standard Steam installs.
    QStringLiteral("C:\\Program Files (x86)\\Steam\\steamapps\\common\\dota 2 beta\\game\\bin\\win64\\dota2.exe"),
    QStringLiteral("C:\\Program Files\\Steam\\steamapps\\common\\dota 2 beta\\game\\bin\\win64\\dota2.exe"),
    QStringLiteral("D:\\Steam\\steamapps\\common\\dota 2 beta\\game\\bin\\win64\\dota2.exe"),
    QStringLiteral("E:\\Steam\\steamapps\\common\\dota 2 beta\\game\\bin\\win64\\dota2.exe"),
};

const QString DotaRelative =
    QStringLiteral("steamapps/common/dota 2 beta/game/bin/win64/dota2.exe");

QString candidateFromSteamRoot(const QString &steamRoot)
{
    if (steamRoot.isEmpty())
        return QString();
    const QString candidate = QDir(steamRoot).filePath(DotaRelative);
    return QFileInfo::exists(candidate) ? QDir::cleanPath(candidate) : QString();
}

QString candidateFromLibrary(const QString &libraryPath)
{
    if (libraryPath.isEmpty())
        return QString();
    // VDF paths arrive with escaped backslashes ("D:\\SteamLibrary").
    const QString cleaned = QString(libraryPath).replace(QStringLiteral("\\\\"),
                                                         QStringLiteral("/"));
    return candidateFromSteamRoot(cleaned);
}

/// Parses a steamapps/libraryfolders.vdf (or config/libraryfolders.vdf) for the
/// "path" entries, so Dota is found on libraries other than the Steam root.
QStringList libraryFoldersFrom(const QString &steamRoot)
{
    QStringList result;
    const QStringList candidates = {
        QDir(steamRoot).filePath(QStringLiteral("steamapps/libraryfolders.vdf")),
        QDir(steamRoot).filePath(QStringLiteral("config/libraryfolders.vdf")),
    };

    static const QRegularExpression pathPattern(
        QStringLiteral("\"path\"\\s+\"([^\"]+)\""));
    for (const auto &vdfPath : candidates)
    {
        QFile file(vdfPath);
        if (!file.open(QIODevice::ReadOnly))
            continue;
        const QString content = QString::fromUtf8(file.readAll());
        auto it = pathPattern.globalMatch(content);
        while (it.hasNext())
        {
            const QString path = it.next().captured(1);
            if (!path.isEmpty())
                result.append(path);
        }
    }
    return result;
}

QStringList steamRootsFromRegistry()
{
    QStringList roots;
#ifdef Q_OS_WIN
    const QStringList machineKeys = {
        QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Valve\\Steam"),
        QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Valve\\Steam"),
    };
    for (const auto &key : machineKeys)
    {
        QSettings settings(key, QSettings::NativeFormat);
        const QString install = settings.value(QStringLiteral("InstallPath")).toString();
        if (!install.isEmpty())
            roots.append(install);
    }

    QSettings user(QStringLiteral("HKEY_CURRENT_USER\\SOFTWARE\\Valve\\Steam"),
                   QSettings::NativeFormat);
    const QString steamPath = user.value(QStringLiteral("SteamPath")).toString();
    if (!steamPath.isEmpty())
        roots.append(steamPath);
#else
    Q_UNUSED(roots)
#endif
    return roots;
}
}

namespace DotaPathDetector
{
QString detect()
{
    // 1) Known fixed locations (the private build and common Steam drives).
    for (const auto &path : CommonPaths)
    {
        if (QFileInfo::exists(path))
            return path;
    }

    // 2) Steam registry roots, then every library folder Steam knows about.
    const auto roots = steamRootsFromRegistry();
    for (const auto &root : roots)
    {
        const QString direct = candidateFromSteamRoot(root);
        if (!direct.isEmpty())
            return direct;

        const auto libraries = libraryFoldersFrom(root);
        for (const auto &library : libraries)
        {
            const QString found = candidateFromLibrary(library);
            if (!found.isEmpty())
                return found;
        }
    }

    return QString();
}
}
