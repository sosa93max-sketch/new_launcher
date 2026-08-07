#pragma once

#include "../config/AppConfig.h"

#include <QString>

/// Writes the steam_api.ini the D2 shim reads. The config folder lives beside
/// the loaded executable: <dota exe dir>/D2MAX. Keeping this path identical to
/// Common.DataPath() in the shim is important when switching accounts.
namespace IniGenerator
{
QString d2maxDir(const QString &dota2ExePath);
QString iniPath(const QString &dota2ExePath);

bool write(const QString &dota2ExePath,
           const QString &serverUrl,
           const QString &clientInstanceId,
           const Profile &profile,
           bool enableConsole,
           QString *error);
}
