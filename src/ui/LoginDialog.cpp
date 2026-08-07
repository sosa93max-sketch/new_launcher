#include "LoginDialog.h"

#include "../util/Log.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

LoginDialog::LoginDialog(AppConfig &config, ServerClient &server, QWidget *parent)
    : QDialog(parent)
    , m_config(config)
    , m_server(server)
{
    setWindowTitle(QStringLiteral("Iniciar sesión"));
    setMinimumWidth(420);

    auto *title = new QLabel(QStringLiteral("D2MAX LAUNCHER"), this);
    title->setObjectName(QStringLiteral("TitleLabel"));
    auto *subtitle = new QLabel(
        QStringLiteral("Inicia sesión con tu cuenta del servidor D2. Si no existe, se creará automáticamente."),
        this);
    subtitle->setObjectName(QStringLiteral("SubtitleLabel"));
    subtitle->setWordWrap(true);

    m_serverUrl = new QLineEdit(m_config.serverUrl, this);
    m_serverUrl->setPlaceholderText(QStringLiteral("http://127.0.0.1:5199/"));
    m_usernameEdit = new QLineEdit(this);
    m_usernameEdit->setPlaceholderText(QStringLiteral("Usuario"));
    m_password = new QLineEdit(this);
    m_password->setPlaceholderText(QStringLiteral("Contraseña"));
    m_password->setEchoMode(QLineEdit::Password);
    m_remember = new QCheckBox(QStringLiteral("Recordar cuenta"), this);
    m_remember->setChecked(m_config.rememberMe);

    m_status = new QLabel(QStringLiteral("Comprobando servidor..."), this);
    m_status->setObjectName(QStringLiteral("SubtitleLabel"));
    m_error = new QLabel(this);
    m_error->setObjectName(QStringLiteral("ErrorLabel"));
    m_error->setWordWrap(true);
    m_error->hide();

    m_loginButton = new QPushButton(QStringLiteral("INICIAR SESIÓN"), this);
    m_loginButton->setObjectName(QStringLiteral("AccentButton"));
    m_loginButton->setDefault(true);
    auto *cancelButton = new QPushButton(QStringLiteral("Cancelar"), this);
    cancelButton->setObjectName(QStringLiteral("GhostButton"));

    auto *form = new QFormLayout;
    form->setSpacing(10);
    form->addRow(QStringLiteral("Servidor"), m_serverUrl);
    form->addRow(QStringLiteral("Usuario"), m_usernameEdit);
    form->addRow(QStringLiteral("Contraseña"), m_password);
    form->addRow(QString(), m_remember);

    auto *buttons = new QHBoxLayout;
    buttons->addWidget(cancelButton);
    buttons->addStretch();
    buttons->addWidget(m_loginButton);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 26, 28, 24);
    layout->setSpacing(12);
    layout->addWidget(title);
    layout->addWidget(subtitle);
    layout->addSpacing(8);
    layout->addLayout(form);
    layout->addWidget(m_status);
    layout->addWidget(m_error);
    layout->addSpacing(8);
    layout->addLayout(buttons);

    connect(m_loginButton, &QPushButton::clicked, this, &LoginDialog::attemptLogin);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_password, &QLineEdit::returnPressed, this, &LoginDialog::attemptLogin);
    connect(&m_server, &ServerClient::pingFinished,
            this, [this](bool reachable, const QString &version) {
                m_status->setStyleSheet(reachable ? QStringLiteral("color: #3fb950; font-weight: 700;")
                                                  : QStringLiteral("color: #f85149; font-weight: 700;"));
                m_status->setText(reachable
                                      ? (version.isEmpty()
                                             ? QStringLiteral("Servidor en línea")
                                             : QStringLiteral("Servidor en línea (%1)").arg(version))
                                      : QStringLiteral("Servidor no detectado"));
            });
    connect(&m_server, &ServerClient::loginFinished,
            this, [this](bool ok, const QString &error, const QString &token,
                         quint64 steamId, quint32 accountId) {
                m_loginButton->setEnabled(true);
                m_loginButton->setText(QStringLiteral("INICIAR SESIÓN"));
                if (!ok)
                {
                    m_error->setText(error);
                    m_error->show();
                    Log::line(QStringLiteral("LOGIN dialog error: %1").arg(error));
                    return;
                }
                m_username = m_usernameEdit->text().trimmed();
                m_displayName = m_username;
                m_token = token;
                m_steamId = steamId;
                m_accountId = accountId;
                m_rememberMe = m_remember->isChecked();
                accept();
            });

    m_server.setBaseUrl(m_config.serverUrl);
    startStatusProbe();
}

void LoginDialog::startStatusProbe()
{
    m_server.ping();
    m_statusTimer.setInterval(2000);
    connect(&m_statusTimer, &QTimer::timeout, this, [this]() {
        if (m_serverUrl->text().trimmed() != m_config.serverUrl)
        {
            m_config.serverUrl = m_serverUrl->text().trimmed();
            m_server.setBaseUrl(m_config.serverUrl);
        }
        m_server.ping();
    });
    m_statusTimer.start();
}

void LoginDialog::attemptLogin()
{
    const QString url = m_serverUrl->text().trimmed();
    const QString user = m_usernameEdit->text().trimmed();
    const QString pass = m_password->text();

    if (url.isEmpty())
    {
        m_error->setText(QStringLiteral("Indica la URL del servidor"));
        m_error->show();
        return;
    }
    if (user.isEmpty() || pass.isEmpty())
    {
        m_error->setText(QStringLiteral("Usuario y contraseña son obligatorios"));
        m_error->show();
        return;
    }

    m_error->hide();
    m_loginButton->setEnabled(false);
    m_loginButton->setText(QStringLiteral("CONECTANDO..."));
    m_server.setBaseUrl(url);
    m_server.login(user, pass);
}
