#pragma once

#include "../config/AppConfig.h"

#include <QObject>
#include <QString>

struct LaunchOutcome
{
    bool success = false;
    quint32 pid = 0;
    void *processHandle = nullptr;
    QString error;
};

/// Launches dota2.exe with the D2 steam_api payload injected, without touching
/// the game folder (the DLL is shadow-copied to %TEMP%). The payload file must
/// sit next to the launcher executable (steam_api64.dll) or under payload/x64/.
class GameLauncher : public QObject
{
    Q_OBJECT

public:
    explicit GameLauncher(QObject *parent = nullptr);

    LaunchOutcome launch(const QString &dota2ExePath,
                         const Profile &profile,
                         const AppConfig &config,
                         const QString &serverUrl);

    /// Stops a launched game process.
    void stop(quint32 pid, void *processHandle);

    /// Whether the launched process is still alive.
    bool isRunning(quint32 pid, void *processHandle) const;

    static QString payloadDllPath();

private:
    QString prepareInjectablePayload(const QString &payload) const;
};
