#pragma once

#include "AppConfig.h"

#include <QString>

/// Loads and saves AppConfig as JSON (atomic write). Path on Windows:
/// %APPDATA%/D2MaxLauncher/config.json.
class ConfigStore
{
public:
    static QString rootDir();
    static QString configPath();

    void load();
    void save() const;

    AppConfig &config() { return m_config; }
    const AppConfig &config() const { return m_config; }

    /// Stable per-machine client id, generated on first run.
    static QString clientInstanceId();

private:
    AppConfig m_config;
};
