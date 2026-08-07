#pragma once

#include "../config/AppConfig.h"
#include "../net/ServerClient.h"

#include <QDialog>
#include <QPoint>
#include <QTimer>

#include <cstdint>

class QCheckBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QWidget;

/// Frameless, custom-headed login against the D2ST server. Unknown usernames
/// are registered by the server on first login, so this is also how new
/// accounts are created. Cancel means "don't enter the launcher": the caller
/// decides whether to quit or stay.
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

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void buildUi();
    void connectSignals();
    void startStatusProbe();
    void attemptLogin();
    void setBusy(bool busy);

    AppConfig &m_config;
    ServerClient &m_server;

    QWidget *m_header = nullptr;
    QLabel *m_status = nullptr;
    QLabel *m_error = nullptr;
    QLineEdit *m_serverUrl = nullptr;
    QLineEdit *m_usernameEdit = nullptr;
    QLineEdit *m_password = nullptr;
    QCheckBox *m_remember = nullptr;
    QPushButton *m_loginButton = nullptr;
    QPushButton *m_cancelButton = nullptr;
    QProgressBar *m_progress = nullptr;
    QTimer m_statusTimer;

    QPoint m_dragOffset;
    bool m_dragging = false;

    QString m_username;
    QString m_displayName;
    QString m_token;
    quint64 m_steamId = 0;
    quint32 m_accountId = 0;
    bool m_rememberMe = true;
};
