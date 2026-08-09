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

const QString DotaExecutableRelative =
    QStringLiteral("game/bin/win64/dota2.exe");
const QString SteamDotaRelative =
    QStringLiteral("steamapps/common/dota 2 beta/game/bin/win64/dota2.exe");

QString normalized(const QString &raw)
{
    auto path = raw.trimmed();
    if (path.size() >= 2 && path.startsWith('"') && path.endsWith('"'))
        path = path.mid(1, path.size() - 2);
    path.replace(QStringLiteral("\\\\"), QStringLiteral("/"));
    path.replace(QStringLiteral("\\\""), QStringLiteral("\""));
    return path.isEmpty() ? QString() : QDir::cleanPath(QDir::fromNativeSeparators(path));
}

QString existingFile(const QString &raw)
{
    const auto path = normalized(raw);
    if (path.isEmpty())
        return {};
    const QFileInfo info(path);
    return info.isFile() && info.fileName().compare(QStringLiteral("dota2.exe"), Qt::CaseInsensitive) == 0
        ? QDir::cleanPath(info.absoluteFilePath())
        : QString();
}

QString candidateFromSteamRoot(const QString &steamRoot)
{
    const auto root = normalized(steamRoot);
    if (root.isEmpty())
        return {};

    // Accept all forms we may receive from the settings file or a VDF:
    // Steam root, library root, Dota directory, or the executable itself.
    const QStringList candidates = {
        root,
        QDir(root).filePath(DotaExecutableRelative),
        QDir(root).filePath(QStringLiteral("dota 2 beta/game/bin/win64/dota2.exe")),
        QDir(root).filePath(SteamDotaRelative),
    };
    for (const auto &candidate : candidates)
    {
        const auto found = existingFile(candidate);
        if (!found.isEmpty())
            return found;
    }
    return {};
}

QString candidateFromLibrary(const QString &libraryPath)
{
    if (libraryPath.isEmpty())
        return QString();
    // VDF paths arrive with escaped backslashes ("D:\\SteamLibrary").
    return candidateFromSteamRoot(libraryPath);
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
    roots.append(QDir::home().filePath(QStringLiteral(".steam/steam")));
    roots.append(QDir::home().filePath(QStringLiteral(".local/share/Steam")));
#endif

    const QStringList environmentRoots = {
        qEnvironmentVariable("DOTA2_PATH"),
        qEnvironmentVariable("STEAM_PATH"),
        qEnvironmentVariable("STEAM_DIR"),
        qEnvironmentVariable("ProgramFiles(x86)"),
        qEnvironmentVariable("ProgramFiles"),
        qEnvironmentVariable("LOCALAPPDATA")
    };
    for (const auto &root : environmentRoots)
    {
        if (!root.isEmpty())
        {
            roots.append(root);
            roots.append(QDir(root).filePath(QStringLiteral("Steam")));
        }
    }

    for (const auto &drive : QDir::drives())
    {
        const QDir root(drive.absoluteFilePath());
        roots.append(root.filePath(QStringLiteral("Steam")));
        roots.append(root.filePath(QStringLiteral("SteamLibrary")));
        roots.append(root.filePath(QStringLiteral("Games/Dota 2 7.22g")));
    }

    roots.removeDuplicates();
    return roots;
}
}

namespace DotaPathDetector
{
QString resolve(const QString &path)
{
    return candidateFromSteamRoot(path);
}

bool isValid(const QString &path)
{
    return !existingFile(path).isEmpty();
}

QString detect(const QString &preferredPath)
{
    // 1) The path saved by the user, then an explicit environment override.
    for (const auto &path : {preferredPath, qEnvironmentVariable("DOTA2_PATH")})
    {
        const auto found = resolve(path);
        if (!found.isEmpty())
            return found;
    }

    // 2) Known fixed locations (the private build and standard Steam drives).
    for (const auto &path : CommonPaths)
    {
        const auto found = resolve(path);
        if (!found.isEmpty())
            return found;
    }

    // 3) Steam registry/environment roots, then every library folder Steam
    // knows about. The VDF parser is intentionally used even when the root
    // itself is missing, because libraries are often on another drive.
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

    return {};
}
}
