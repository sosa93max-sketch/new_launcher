#pragma once

#include <QString>
#include <QVector>

#include <cstdint>

/// One account the user can play with. Accounts are created on the D2ST server
/// itself (first login registers them), so a profile is exactly a server
/// account plus whatever the launcher needs to switch back to it.
struct Profile
{
    QString username;
    QString displayName;
    quint32 accountId = 0;
    quint64 steamId = 0;

    /// Last bearer token from POST /api/auth/login (empty until first login).
    QString token;
    qint64 tokenSavedAtMs = 0;

    qint64 lastLoginMs = 0;
};

/// Launcher-wide settings. Persisted as JSON next to the app data folder.
struct AppConfig
{
    QString serverUrl = QStringLiteral("http://127.0.0.1:5199/");
    QString dota2Path;

    QVector<Profile> profiles;
    QString currentUsername;

    bool rememberMe = true;
    bool enableConsole = true;
    bool skipIntro = true;
    bool insecureMode = false;

    QString clientInstanceId;
};
