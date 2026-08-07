#include "ConfigStore.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>

static const QLatin1String KeyServerUrl("ServerUrl");
static const QLatin1String KeyDota2Path("Dota2Path");
static const QLatin1String KeyProfiles("Profiles");
static const QLatin1String KeyCurrentUsername("CurrentUsername");
static const QLatin1String KeyRememberMe("RememberMe");
static const QLatin1String KeyEnableConsole("EnableConsole");
static const QLatin1String KeySkipIntro("SkipIntro");
static const QLatin1String KeyInsecureMode("InsecureMode");
static const QLatin1String KeyClientInstanceId("ClientInstanceId");

static const QLatin1String KeyUsername("Username");
static const QLatin1String KeyDisplayName("DisplayName");
static const QLatin1String KeyAccountId("AccountId");
static const QLatin1String KeySteamId("SteamId");
static const QLatin1String KeyToken("Token");
static const QLatin1String KeyTokenSavedAtMs("TokenSavedAtMs");
static const QLatin1String KeyLastLoginMs("LastLoginMs");

QString ConfigStore::rootDir()
{
    const auto base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!base.isEmpty())
        return base;
    return QDir::home().filePath(QStringLiteral(".D2MaxLauncher"));
}

QString ConfigStore::configPath()
{
    return QDir(rootDir()).filePath(QStringLiteral("config.json"));
}

QString ConfigStore::clientInstanceId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

void ConfigStore::load()
{
    m_config = AppConfig{};

    QFile file(configPath());
    if (!file.open(QIODevice::ReadOnly))
        return;

    const auto document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject())
        return;

    const auto root = document.object();
    m_config.serverUrl = root.value(KeyServerUrl).toString(m_config.serverUrl);
    m_config.dota2Path = root.value(KeyDota2Path).toString();
    m_config.currentUsername = root.value(KeyCurrentUsername).toString();
    m_config.rememberMe = root.value(KeyRememberMe).toBool(m_config.rememberMe);
    m_config.enableConsole = root.value(KeyEnableConsole).toBool(m_config.enableConsole);
    m_config.skipIntro = root.value(KeySkipIntro).toBool(m_config.skipIntro);
    m_config.insecureMode = root.value(KeyInsecureMode).toBool(m_config.insecureMode);
    m_config.clientInstanceId = root.value(KeyClientInstanceId).toString();

    const auto profiles = root.value(KeyProfiles).toArray();
    for (const auto &value : profiles)
    {
        const auto object = value.toObject();
        Profile profile;
        profile.username = object.value(KeyUsername).toString();
        profile.displayName = object.value(KeyDisplayName).toString();
        profile.accountId = static_cast<quint32>(object.value(KeyAccountId).toDouble(0));
        profile.steamId = static_cast<quint64>(object.value(KeySteamId).toDouble(0));
        profile.token = object.value(KeyToken).toString();
        profile.tokenSavedAtMs = static_cast<qint64>(object.value(KeyTokenSavedAtMs).toDouble(0));
        profile.lastLoginMs = static_cast<qint64>(object.value(KeyLastLoginMs).toDouble(0));
        if (!profile.username.isEmpty())
            m_config.profiles.append(profile);
    }

    if (m_config.clientInstanceId.isEmpty())
        m_config.clientInstanceId = clientInstanceId();
}

void ConfigStore::save() const
{
    QDir().mkpath(rootDir());

    QJsonArray profiles;
    for (const auto &profile : m_config.profiles)
    {
        QJsonObject object;
        object.insert(KeyUsername, profile.username);
        object.insert(KeyDisplayName, profile.displayName);
        object.insert(KeyAccountId, static_cast<double>(profile.accountId));
        object.insert(KeySteamId, static_cast<double>(profile.steamId));
        object.insert(KeyToken, profile.token);
        object.insert(KeyTokenSavedAtMs, static_cast<double>(profile.tokenSavedAtMs));
        object.insert(KeyLastLoginMs, static_cast<double>(profile.lastLoginMs));
        profiles.append(object);
    }

    QJsonObject root;
    root.insert(KeyServerUrl, m_config.serverUrl);
    root.insert(KeyDota2Path, m_config.dota2Path);
    root.insert(KeyProfiles, profiles);
    root.insert(KeyCurrentUsername, m_config.currentUsername);
    root.insert(KeyRememberMe, m_config.rememberMe);
    root.insert(KeyEnableConsole, m_config.enableConsole);
    root.insert(KeySkipIntro, m_config.skipIntro);
    root.insert(KeyInsecureMode, m_config.insecureMode);
    root.insert(KeyClientInstanceId, m_config.clientInstanceId);

    QSaveFile file(configPath());
    if (file.open(QIODevice::WriteOnly))
    {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        file.commit();
    }
}
