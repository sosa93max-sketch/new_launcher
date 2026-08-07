#pragma once

#include "../config/AppConfig.h"
#include "../net/ServerClient.h"

#include <QDialog>
#include <QTimer>

#include <cstdint>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;

/// Server URL + username + password login against the D2ST server. Unknown
/// usernames are registered by the server on first login, so this is also how
/// new accounts are created.
class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    LoginDialog(AppConfig &config, ServerClient &server, QWidget *parent = nullptr);

    QString username() const { return m_username; }
    QString displayName() const { return m_displayName; }
    QString token() const { return m_token; }
    quint64 steamId() const { return m_steamId; }
    quint32 accountId() const { return m_accountId; }
    bool rememberMe() const { return m_rememberMe; }

private:
    void startStatusProbe();
    void attemptLogin();

    AppConfig &m_config;
    ServerClient &m_server;

    QLineEdit *m_serverUrl = nullptr;
    QLineEdit *m_username = nullptr;
    QLineEdit *m_password = nullptr;
    QCheckBox *m_remember = nullptr;
    QLabel *m_status = nullptr;
    QLabel *m_error = nullptr;
    QPushButton *m_loginButton = nullptr;
    QTimer m_statusTimer;

    QString m_username;
    QString m_displayName;
    QString m_token;
    quint64 m_steamId = 0;
    quint32 m_accountId = 0;
    bool m_rememberMe = true;
};
