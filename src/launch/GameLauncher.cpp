#include "GameLauncher.h"

#include "DllInjector.h"
#include "DotaPathDetector.h"
#include "IniGenerator.h"
#include "PeUtils.h"

#include "../util/Log.h"

#include <QCryptographicHash>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace
{
QString payloadFile(const QString &directory)
{
    return QDir(directory).filePath(QStringLiteral("steam_api64.dll"));
}
}

GameLauncher::GameLauncher(QObject *parent)
    : QObject(parent)
{
}

QString GameLauncher::payloadDllPath()
{
    const auto appDir = QCoreApplication::applicationDirPath();
    const QString direct = payloadFile(appDir);
    if (QFileInfo::exists(direct))
        return direct;

    const QString bundled = QDir(appDir).filePath(
        QStringLiteral("payload/x64/steam_api64.dll"));
    return QFileInfo::exists(bundled) ? bundled : QString();
}

QString GameLauncher::prepareInjectablePayload(const QString &payload) const
{
    QFile source(payload);
    if (!source.open(QIODevice::ReadOnly))
        return QString();
    const QByteArray bytes = source.readAll();

    const QByteArray hash = QCryptographicHash::hash(bytes, QCryptographicHash::Sha256)
                                .left(8)
                                .toHex();
    const QString shadowRoot = QDir(QStandardPaths::writableLocation(
                                        QStandardPaths::TempLocation))
                                   .filePath(QStringLiteral("D2MaxLauncher/payload-shadow"));
    const QString shadowDir = QDir(shadowRoot).filePath(QString::fromLatin1(hash));
    const QString shadowPath = QDir(shadowDir).filePath(QStringLiteral("steam_api64.dll"));

    if (!QDir().mkpath(shadowDir))
        return QString();
    if (!QFileInfo::exists(shadowPath) || QFileInfo(shadowPath).size() != bytes.size())
    {
        QFile target(shadowPath);
        if (!target.open(QIODevice::WriteOnly) || target.write(bytes) != bytes.size())
            return QString();
    }
    return shadowPath;
}

LaunchOutcome GameLauncher::launch(const QString &dota2ExePath,
                                   const Profile &profile,
                                   const AppConfig &config,
                                   const QString &serverUrl)
{
    if (dota2ExePath.isEmpty() || !QFileInfo::exists(dota2ExePath))
        return {false, 0, nullptr, QStringLiteral("dota2.exe no encontrado")};

    const auto payload = payloadDllPath();
    if (payload.isEmpty())
        return {false, 0, nullptr,
                QStringLiteral("steam_api64.dll no está junto al launcher (payload faltante)")};

    QString error;
    if (!IniGenerator::write(dota2ExePath, serverUrl, config.clientInstanceId,
                             profile, config.enableConsole, &error))
        return {false, 0, nullptr, error};

    const QString shadowPayload = prepareInjectablePayload(payload);
    if (shadowPayload.isEmpty())
        return {false, 0, nullptr, QStringLiteral("No se pudo preparar el payload")};

    // The exe may import the emulator as steam_api64.dll or steam_api.dll;
    // whichever it is, the rebinder has to find that descriptor and point it at
    // the injected payload.
    QString importName;
    for (const auto &candidate :
         {QStringLiteral("steam_api64.dll"), QStringLiteral("steam_api.dll")})
    {
        if (PeUtils::importsModule(dota2ExePath, candidate))
        {
            importName = candidate;
            break;
        }
    }
    const bool hasStaticImport = !importName.isEmpty();

    QString arguments;
    if (config.enableConsole)
        arguments += QStringLiteral("-console ");
    if (config.skipIntro)
        arguments += QStringLiteral("-novid ");
    if (config.insecureMode)
        arguments += QStringLiteral("-insecure ");

    const auto result = DllInjector::launchAndInject(
        dota2ExePath, shadowPayload, arguments.trimmed(),
        QFileInfo(dota2ExePath).absolutePath(),
        hasStaticImport,
        hasStaticImport ? importName : QStringLiteral("steam_api64.dll"));

    if (result.success)
        Log::line(QStringLiteral("PLAY pid=%1 account=%2 (%3)")
                      .arg(result.pid)
                      .arg(profile.accountId)
                      .arg(profile.displayName));
    else
        Log::line(QStringLiteral("PLAY falló: %1").arg(result.error));

    return {result.success, result.pid, result.processHandle, result.error};
}

void GameLauncher::stop(quint32 pid, void *processHandle)
{
#ifdef Q_OS_WIN
    if (processHandle != nullptr)
    {
        TerminateProcess(static_cast<HANDLE>(processHandle), 1);
        CloseHandle(static_cast<HANDLE>(processHandle));
    }
    Q_UNUSED(pid)
#else
    Q_UNUSED(pid)
    Q_UNUSED(processHandle)
#endif
    Log::line(QStringLiteral("STOP pid=%1").arg(pid));
}

bool GameLauncher::isRunning(quint32 pid, void *processHandle) const
{
#ifdef Q_OS_WIN
    Q_UNUSED(pid)
    if (processHandle == nullptr)
        return false;
    DWORD exitCode = 0;
    if (!GetExitCodeProcess(static_cast<HANDLE>(processHandle), &exitCode))
        return false;
    return exitCode == STILL_ACTIVE;
#else
    Q_UNUSED(pid)
    Q_UNUSED(processHandle)
    return false;
#endif
}
