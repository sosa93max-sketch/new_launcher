#include "LoginDialog.h"

#include "../util/Log.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QProgressBar>
#include <QPushButton>
#include <QScreen>
#include <QVBoxLayout>
#include <QWidget>

LoginDialog::LoginDialog(AppConfig &config, ServerClient &server, QWidget *parent)
    : QDialog(parent)
    , m_config(config)
    , m_server(server)
{
    // Qt::Window (not Dialog) so the frameless window gets a taskbar entry and
    // showMinimized() really minimizes to the taskbar.
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
    setAttribute(Qt::WA_TranslucentBackground);
    setModal(true);

    auto *root = new QWidget(this);
    root->setObjectName(QStringLiteral("LoginRoot"));

    auto *layout = new QVBoxLayout(root);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    buildUi();
    layout->addWidget(m_header);

    auto *body = new QWidget(root);
    body->setObjectName(QStringLiteral("LoginBody"));
    auto *bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(38, 30, 38, 30);
    bodyLayout->setSpacing(12);

    auto *title = new QLabel(QStringLiteral("BIENVENIDO"), body);
    title->setObjectName(QStringLiteral("LoginTitle"));
    auto *subtitle = new QLabel(
        QStringLiteral("Inicia sesión con tu cuenta del servidor D2. Si el usuario no existe, se crea automáticamente."),
        body);
    subtitle->setObjectName(QStringLiteral("SubtitleLabel"));
    subtitle->setWordWrap(true);

    m_serverUrl = new QLineEdit(m_config.serverUrl, body);
    m_serverUrl->setPlaceholderText(QStringLiteral("http://127.0.0.1:5199/"));
    m_serverUrl->addAction(QIcon(QStringLiteral(":/icons/server.svg")), QLineEdit::LeadingPosition);
    m_serverUrl->setMinimumHeight(38);

    m_usernameEdit = new QLineEdit(body);
    m_usernameEdit->setPlaceholderText(QStringLiteral("Usuario"));
    m_usernameEdit->addAction(QIcon(QStringLiteral(":/icons/user.svg")), QLineEdit::LeadingPosition);
    m_usernameEdit->setMinimumHeight(38);

    m_password = new QLineEdit(body);
    m_password->setPlaceholderText(QStringLiteral("Contraseña"));
    m_password->setEchoMode(QLineEdit::Password);
    m_password->addAction(QIcon(QStringLiteral(":/icons/lock.svg")), QLineEdit::LeadingPosition);
    m_password->setMinimumHeight(38);
    auto *eyeAction = m_password->addAction(QIcon(QStringLiteral(":/icons/eye.svg")),
                                            QLineEdit::TrailingPosition);
    eyeAction->setCheckable(true);
    connect(eyeAction, &QAction::triggered, this, [this, eyeAction](bool checked) {
        m_password->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
        eyeAction->setIcon(QIcon(checked ? QStringLiteral(":/icons/eye-off.svg")
                                         : QStringLiteral(":/icons/eye.svg")));
    });

    m_remember = new QCheckBox(QStringLiteral("Recordar cuenta"), body);
    m_remember->setChecked(m_config.rememberMe);

    m_status = new QLabel(QStringLiteral("Comprobando servidor..."), body);
    m_status->setObjectName(QStringLiteral("SubtitleLabel"));
    m_error = new QLabel(body);
    m_error->setObjectName(QStringLiteral("ErrorLabel"));
    m_error->setWordWrap(true);
    m_error->hide();

    m_progress = new QProgressBar(body);
    m_progress->setObjectName(QStringLiteral("BusyBar"));
    m_progress->setRange(0, 0);
    m_progress->setTextVisible(false);
    m_progress->hide();

    m_loginButton = new QPushButton(QStringLiteral("INICIAR SESIÓN"), body);
    m_loginButton->setObjectName(QStringLiteral("AccentButton"));
    m_loginButton->setMinimumHeight(44);
    m_loginButton->setCursor(Qt::PointingHandCursor);
    m_loginButton->setDefault(true);

    m_cancelButton = new QPushButton(QStringLiteral("CANCELAR"), body);
    m_cancelButton->setObjectName(QStringLiteral("GhostButton"));
    m_cancelButton->setMinimumHeight(36);

    bodyLayout->addWidget(title);
    bodyLayout->addWidget(subtitle);
    bodyLayout->addSpacing(10);
    bodyLayout->addWidget(m_serverUrl);
    bodyLayout->addWidget(m_usernameEdit);
    bodyLayout->addWidget(m_password);
    bodyLayout->addWidget(m_remember);
    bodyLayout->addSpacing(4);
    bodyLayout->addWidget(m_status);
    bodyLayout->addWidget(m_error);
    bodyLayout->addWidget(m_progress);
    bodyLayout->addSpacing(10);
    bodyLayout->addWidget(m_loginButton);
    bodyLayout->addWidget(m_cancelButton);

    layout->addWidget(body);

    // The card fills the translucent window, sized to its content.
    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->addWidget(root);

    setMinimumWidth(520);
    adjustSize();
    setFixedSize(size());
    if (const auto screen = QGuiApplication::primaryScreen())
        move(screen->availableGeometry().center() - rect().center());

    connectSignals();
    m_server.setBaseUrl(m_config.serverUrl);
    startStatusProbe();
}

void LoginDialog::buildUi()
{
    m_header = new QWidget(this);
    m_header->setObjectName(QStringLiteral("LoginHeader"));
    m_header->setFixedHeight(56);
    m_header->installEventFilter(this);

    auto *headerLayout = new QHBoxLayout(m_header);
    headerLayout->setContentsMargins(20, 0, 12, 0);
    headerLayout->setSpacing(8);

    auto *logo = new QLabel(m_header);
    logo->setPixmap(QIcon(QStringLiteral(":/icons/logo.svg")).pixmap(30, 30));
    auto *brand = new QLabel(QStringLiteral("D2MAX"), m_header);
    brand->setObjectName(QStringLiteral("LoginBrand"));

    auto *minimizeButton = new QPushButton(m_header);
    minimizeButton->setObjectName(QStringLiteral("IconButton"));
    minimizeButton->setIcon(QIcon(QStringLiteral(":/icons/minus.svg")));
    minimizeButton->setFixedSize(26, 26);
    minimizeButton->setIconSize(QSize(13, 13));
    minimizeButton->setToolTip(QStringLiteral("Minimizar"));
    minimizeButton->setCursor(Qt::PointingHandCursor);

    auto *closeButton = new QPushButton(m_header);
    closeButton->setObjectName(QStringLiteral("CloseButton"));
    closeButton->setIcon(QIcon(QStringLiteral(":/icons/close.svg")));
    closeButton->setFixedSize(26, 26);
    closeButton->setIconSize(QSize(13, 13));
    closeButton->setToolTip(QStringLiteral("Cerrar"));
    closeButton->setCursor(Qt::PointingHandCursor);

    headerLayout->addWidget(logo);
    headerLayout->addWidget(brand);
    headerLayout->addStretch();
    headerLayout->addWidget(minimizeButton);
    headerLayout->addWidget(closeButton);

    connect(minimizeButton, &QPushButton::clicked, this, &QWidget::showMinimized);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::reject);
}

void LoginDialog::connectSignals()
{
    connect(m_loginButton, &QPushButton::clicked, this, &LoginDialog::attemptLogin);
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_password, &QLineEdit::returnPressed, this, &LoginDialog::attemptLogin);
    connect(m_usernameEdit, &QLineEdit::returnPressed, this, &LoginDialog::attemptLogin);

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
                setBusy(false);
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
}

void LoginDialog::startStatusProbe()
{
    m_server.ping();
    m_statusTimer.setInterval(2000);
    connect(&m_statusTimer, &QTimer::timeout, this, [this]() {
        const QString url = m_serverUrl->text().trimmed();
        if (url != m_config.serverUrl)
        {
            m_config.serverUrl = url;
            m_server.setBaseUrl(url);
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
    setBusy(true);
    m_server.setBaseUrl(url);
    m_server.login(user, pass);
}

void LoginDialog::setBusy(bool busy)
{
    m_loginButton->setEnabled(!busy);
    m_cancelButton->setEnabled(!busy);
    m_loginButton->setText(busy ? QStringLiteral("CONECTANDO...") : QStringLiteral("INICIAR SESIÓN"));
    m_progress->setVisible(busy);
}

bool LoginDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_header)
    {
        if (event->type() == QEvent::MouseButtonPress)
        {
            auto *mouse = static_cast<QMouseEvent *>(event);
            if (mouse->button() == Qt::LeftButton)
            {
                m_dragOffset = mouse->globalPosition().toPoint() - frameGeometry().topLeft();
                m_dragging = true;
                return true;
            }
        }
        else if (event->type() == QEvent::MouseMove && m_dragging)
        {
            auto *mouse = static_cast<QMouseEvent *>(event);
            move(mouse->globalPosition().toPoint() - m_dragOffset);
            return true;
        }
        else if (event->type() == QEvent::MouseButtonRelease)
        {
            m_dragging = false;
        }
    }
    return QDialog::eventFilter(watched, event);
}
