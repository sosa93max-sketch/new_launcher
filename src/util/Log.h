#pragma once

#include <QString>

/// Small file logger to %APPDATA%/D2MaxLauncher/launcher.log.
class Log
{
public:
    static void init();
    static void line(const QString &message);
    static QString filePath();
};
