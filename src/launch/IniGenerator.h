#pragma once

#include "../config/AppConfig.h"

#include <QString>

/// Writes the steam_api.ini the D2 shim reads. The config folder lives with the
/// game: <dota game dir>/SKYNET, resolved by walking up from bin/win64.
namespace IniGenerator
{
QString skynetDir(const QString &dota2ExePath);
QString iniPath(const QString &dota2ExePath);

bool write(const QString &dota2ExePath,
           const QString &serverUrl,
           const QString &clientInstanceId,
           const Profile &profile,
           bool enableConsole,
           QString *error);
}
