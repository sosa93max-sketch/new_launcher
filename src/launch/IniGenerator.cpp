#include "IniGenerator.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QTextStream>

namespace
{
QString boolText(bool value)
{
    return value ? QStringLiteral("true") : QStringLiteral("false");
}
}

namespace IniGenerator
{
QString skynetDir(const QString &dota2ExePath)
{
    // .../game/bin/win64/dota2.exe -> .../game/SKYNET
    const QFileInfo exeInfo(dota2ExePath);
    if (exeInfo.fileName().compare(QStringLiteral("dota2.exe"), Qt::CaseInsensitive) == 0)
    {
        const QFileInfo gameInfo(
            QDir(exeInfo.absolutePath()).filePath(QStringLiteral("../..")));
        if (gameInfo.fileName().compare(QStringLiteral("game"), Qt::CaseInsensitive) == 0)
            return QDir(gameInfo.absoluteFilePath()).filePath(QStringLiteral("SKYNET"));
    }

    return QDir(exeInfo.absolutePath()).filePath(QStringLiteral("SKYNET"));
}

QString iniPath(const QString &dota2ExePath)
{
    return QDir(skynetDir(dota2ExePath)).filePath(QStringLiteral("steam_api.ini"));
}

bool write(const QString &dota2ExePath,
           const QString &serverUrl,
           const QString &clientInstanceId,
           const Profile &profile,
           bool enableConsole,
           QString *error)
{
    QString content;
    QTextStream out(&content);

    out << "[User Settings]\n";
    out << "ClientInstanceId = " << clientInstanceId << "\n";
    out << "FallbackPersonaName = " << profile.displayName << "\n";
    out << "FallbackAccountId = " << profile.accountId << "\n";
    out << "\n";

    out << "[Game Settings]\n";
    out << "AppId = 570\n";
    out << "Languaje = english\n";
    out << "UnlockAllDLC = true\n";
    out << "\n";

    out << "[Network Settings]\n";
    out << "UseServerApi = true\n";
    out << "ServerUrl = " << serverUrl << "\n";
    out << "DiscoveryPort = 27081\n";
    out << "UseActiveWebUser = true\n";
    out << "SecureNetworking = false\n";
    out << "VacSecureGameServer = false\n";
    out << "PollIntervalMs = 1000\n";
    out << "HttpTimeoutMs = 30000\n";
    out << "\n";

    out << "[Log Settings]\n";
    out << "File = true\n";
    out << "Console = " << boolText(enableConsole) << "\n";
    out << "\n";

    out << "[Audio Settings]\n";
    out << "EnableVoiceCapture = true\n";
    out << "\n";

    out << "[Inventory]\n";
    out << "Enabled = true\n";
    out << "AutoGrantPurchases = true\n";
    out << "AutoGrantPromos = true\n";
    out << "\n";

    out.flush();

    const QString dir = skynetDir(dota2ExePath);
    if (!QDir().mkpath(dir))
    {
        if (error)
            *error = QStringLiteral("No se pudo crear %1").arg(dir);
        return false;
    }

    QSaveFile file(iniPath(dota2ExePath));
    if (!file.open(QIODevice::WriteOnly))
    {
        if (error)
            *error = QStringLiteral("No se pudo escribir %1").arg(iniPath(dota2ExePath));
        return false;
    }
    file.write(content.toUtf8());
    return file.commit();
}
}
