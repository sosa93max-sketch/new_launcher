#pragma once

#include "../config/ConfigStore.h"
#include "../launch/GameLauncher.h"
#include "../net/ServerClient.h"

#include <QMainWindow>
#include <QTimer>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QStatusBar;
class StoreView;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(ConfigStore &store, ServerClient &server, QWidget *parent = nullptr);

    /// Shows the login dialog. Returns true only when an account was added or
    /// updated; false means the user cancelled.
    bool addAccount();

protected:
    /// Tells the server the account went offline before the app exits.
    void closeEvent(QCloseEvent *event) override;

private:
    /// Applies an accepted login dialog to the profile store.
    bool applyAccount(class LoginDialog &dialog);
    void buildUi();
    void buildDashboardUi();
    void applyCurrentProfile();
    void validateCurrentSession();
    QString currentToken() const;
    void showDashboard();
    void logout();
    void play();
    void openStore();
    void stop();
    void refreshStatus();
    void monitorProcess();

    ConfigStore &m_store;
    ServerClient &m_server;
    GameLauncher m_launcher;

    QLabel *m_avatar = nullptr;
    QLabel *m_accountName = nullptr;
    QLabel *m_accountMeta = nullptr;
    QLabel *m_rankLabel = nullptr;
    QLabel *m_level = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLineEdit *m_serverUrl = nullptr;
    QLineEdit *m_dotaPath = nullptr;
    QCheckBox *m_console = nullptr;
    QCheckBox *m_novid = nullptr;
    QCheckBox *m_insecure = nullptr;
    QPushButton *m_playButton = nullptr;
    QPushButton *m_homeButton = nullptr;
    QPushButton *m_storeButton = nullptr;
    QStackedWidget *m_pageStack = nullptr;
    StoreView *m_storeView = nullptr;
    QStatusBar *m_statusBar = nullptr;

    QTimer m_statusTimer;
    QTimer m_processTimer;
    quint32 m_runningPid = 0;
    void *m_runningHandle = nullptr;
};
