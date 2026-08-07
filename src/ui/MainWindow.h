#pragma once

#include "../config/ConfigStore.h"
#include "../launch/GameLauncher.h"
#include "../net/ServerClient.h"

#include <QMainWindow>
#include <QTimer>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QStatusBar;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(ConfigStore &store, ServerClient &server, QWidget *parent = nullptr);

private:
    void buildUi();
    void rebuildProfiles();
    void applyCurrentProfile();
    void validateCurrentSession();
    /// Shows the login dialog. Returns true only when an account was added or
    /// updated; false means the user cancelled.
    bool addAccount();
    void logout();
    void play();
    void stop();
    void refreshStatus();
    void refreshAccountInfo();
    void monitorProcess();

    ConfigStore &m_store;
    ServerClient &m_server;
    GameLauncher m_launcher;

    QComboBox *m_accountCombo = nullptr;
    QLabel *m_accountName = nullptr;
    QLabel *m_accountMeta = nullptr;
    QLabel *m_level = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLineEdit *m_serverUrl = nullptr;
    QLineEdit *m_dotaPath = nullptr;
    QCheckBox *m_console = nullptr;
    QCheckBox *m_novid = nullptr;
    QCheckBox *m_insecure = nullptr;
    QPushButton *m_playButton = nullptr;
    QStatusBar *m_statusBar = nullptr;

    QTimer m_statusTimer;
    QTimer m_processTimer;
    quint32 m_runningPid = 0;
    void *m_runningHandle = nullptr;
};
