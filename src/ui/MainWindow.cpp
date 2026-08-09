#include "MainWindow.h"

#include "LoginDialog.h"

#include "../launch/DotaPathDetector.h"
#include "../util/Log.h"

#include <QCheckBox>
#include <QComboBox>
#include <QApplication>
#include <QCloseEvent>
#include <QDateTime>
#include <QDesktopServices>
#include <QFileDialog>
#include <QGraphicsDropShadowEffect>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPushButton>
#include <QStringList>
#include <QStatusBar>
#include <QStyle>
#include <QVBoxLayout>

#include <algorithm>

namespace
{
QString medalName(int tier)
{
    static const QStringList names = {
        QStringLiteral("Herald"), QStringLiteral("Guardian"), QStringLiteral("Crusader"),
        QStringLiteral("Archon"), QStringLiteral("Legend"), QStringLiteral("Ancient"),
        QStringLiteral("Divine"), QStringLiteral("Immortal")};
    return (tier >= 1 && tier <= names.size()) ? names.at(tier - 1) : QStringLiteral("Herald");
}

QPixmap circularAvatar(const QByteArray &png, int size)
{
    QPixmap source;
    if (!source.loadFromData(png))
        return {};
    source = source.scaled(size, size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);

    QPixmap result(size, size);
    result.fill(Qt::transparent);
    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addEllipse(0, 0, size, size);
    painter.setClipPath(path);
    painter.drawPixmap(-(source.width() - size) / 2, -(source.height() - size) / 2, source);
    return result;
}

void addCardShadow(QWidget *card)
{
    auto *shadow = new QGraphicsDropShadowEffect(card);
    shadow->setBlurRadius(30);
    shadow->setOffset(0, 8);
    shadow->setColor(QColor(0, 0, 0, 115));
    card->setGraphicsEffect(shadow);
}
}

MainWindow::MainWindow(ConfigStore &store, ServerClient &server, QWidget *parent)
    : QMainWindow(parent)
    , m_store(store)
    , m_server(server)
{
    setWindowTitle(QStringLiteral("D2Max Launcher"));
    resize(1040, 700);
    setMinimumSize(900, 620);
    buildUi();

    applyCurrentProfile();

    m_statusTimer.setInterval(3000);
    connect(&m_statusTimer, &QTimer::timeout, this, &MainWindow::refreshStatus);
    m_statusTimer.start();
    refreshStatus();

    m_processTimer.setInterval(1000);
    connect(&m_processTimer, &QTimer::timeout, this, &MainWindow::monitorProcess);
    m_processTimer.start();

    validateCurrentSession();
}

void MainWindow::buildUi()
{
    auto *central = new QWidget(this);
    central->setObjectName(QStringLiteral("LauncherRoot"));
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(30, 24, 30, 14);
    layout->setSpacing(16);

    // ---- header ----
    auto *header = new QHBoxLayout;
    auto *title = new QLabel(QStringLiteral("D2MAX LAUNCHER"), central);
    title->setObjectName(QStringLiteral("TitleLabel"));
    m_statusLabel = new QLabel(QStringLiteral("● COMPROBANDO SERVIDOR"), central);
    m_statusLabel->setObjectName(QStringLiteral("StatusChecking"));
    auto *logoutButton = new QPushButton(QStringLiteral("SALIR"), central);
    logoutButton->setObjectName(QStringLiteral("GhostButton"));
    m_storeButton = new QPushButton(QStringLiteral("TIENDA"), central);
    m_storeButton->setObjectName(QStringLiteral("AccentButton"));
    header->addWidget(title);
    header->addStretch();
    header->addWidget(m_statusLabel);
    header->addWidget(m_storeButton);
    header->addWidget(logoutButton);
    layout->addLayout(header);

    // ---- account card ----
    auto *accountBox = new QGroupBox(QStringLiteral("CUENTA"), central);
    accountBox->setObjectName(QStringLiteral("PanelCard"));
    addCardShadow(accountBox);
    auto *accountLayout = new QVBoxLayout(accountBox);
    auto *accountRow = new QHBoxLayout;

    m_avatar = new QLabel(accountBox);
    m_avatar->setFixedSize(64, 64);
    m_avatar->setAlignment(Qt::AlignCenter);
    m_avatar->setStyleSheet(QStringLiteral(
        "border-radius: 32px; background: #1f6feb; color: white;"
        "font-size: 26px; font-weight: 700;"));
    m_avatar->setText(QStringLiteral("?"));

    auto *infoCol = new QVBoxLayout;
    m_accountName = new QLabel(QStringLiteral("Sin cuenta"), accountBox);
    m_accountName->setStyleSheet(QStringLiteral("font-size: 15px; font-weight: 700;"));
    m_accountMeta = new QLabel(QStringLiteral("--"), accountBox);
    m_accountMeta->setObjectName(QStringLiteral("SubtitleLabel"));
    infoCol->addWidget(m_accountName);
    infoCol->addWidget(m_accountMeta);

    m_rankLabel = new QLabel(QStringLiteral("MMR --"), accountBox);
    m_rankLabel->setObjectName(QStringLiteral("SubtitleLabel"));
    m_level = new QLabel(QStringLiteral("Nivel --"), accountBox);
    m_level->setObjectName(QStringLiteral("SubtitleLabel"));

    accountRow->addWidget(m_avatar);
    accountRow->addSpacing(16);
    accountRow->addLayout(infoCol);
    accountRow->addStretch();
    accountRow->addWidget(m_rankLabel);
    accountRow->addSpacing(16);
    accountRow->addWidget(m_level);
    accountLayout->addLayout(accountRow);
    layout->addWidget(accountBox);

    // ---- dota path ----
    auto *gameBox = new QGroupBox(QStringLiteral("DOTA 2"), central);
    gameBox->setObjectName(QStringLiteral("PanelCard"));
    addCardShadow(gameBox);
    auto *gameLayout = new QVBoxLayout(gameBox);
    auto *pathRow = new QHBoxLayout;
    m_dotaPath = new QLineEdit(m_store.config().dota2Path, gameBox);
    m_dotaPath->setPlaceholderText(QStringLiteral("ruta a ...\\game\\bin\\win64\\dota2.exe"));
    auto *browseButton = new QPushButton(QStringLiteral("EXAMINAR"), gameBox);
    browseButton->setObjectName(QStringLiteral("GhostButton"));
    auto *detectButton = new QPushButton(QStringLiteral("AUTO-DETECTAR"), gameBox);
    detectButton->setObjectName(QStringLiteral("GhostButton"));
    pathRow->addWidget(m_dotaPath, 1);
    pathRow->addWidget(browseButton);
    pathRow->addWidget(detectButton);
    gameLayout->addLayout(pathRow);

    auto *argsRow = new QHBoxLayout;
    m_console = new QCheckBox(QStringLiteral("-console"), gameBox);
    m_console->setChecked(m_store.config().enableConsole);
    m_novid = new QCheckBox(QStringLiteral("-novid"), gameBox);
    m_novid->setChecked(m_store.config().skipIntro);
    m_insecure = new QCheckBox(QStringLiteral("-insecure"), gameBox);
    m_insecure->setChecked(m_store.config().insecureMode);
    argsRow->addWidget(m_console);
    argsRow->addWidget(m_novid);
    argsRow->addWidget(m_insecure);
    argsRow->addStretch();
    gameLayout->addLayout(argsRow);
    layout->addWidget(gameBox);

    // ---- server ----
    auto *serverBox = new QGroupBox(QStringLiteral("SERVIDOR D2"), central);
    serverBox->setObjectName(QStringLiteral("PanelCard"));
    addCardShadow(serverBox);
    auto *serverLayout = new QHBoxLayout(serverBox);
    m_serverUrl = new QLineEdit(m_store.config().serverUrl, serverBox);
    auto *applyServerButton = new QPushButton(QStringLiteral("APLICAR"), serverBox);
    applyServerButton->setObjectName(QStringLiteral("GhostButton"));
    serverLayout->addWidget(m_serverUrl, 1);
    serverLayout->addWidget(applyServerButton);
    layout->addWidget(serverBox);

    // ---- play ----
    m_playButton = new QPushButton(QStringLiteral("JUGAR DOTA 2"), central);
    m_playButton->setObjectName(QStringLiteral("AccentButton"));
    m_playButton->setMinimumHeight(44);
    layout->addWidget(m_playButton);
    layout->addStretch();

    setCentralWidget(central);
    m_statusBar = statusBar();
    m_statusBar->showMessage(QStringLiteral("Listo"));

    // ---- connections ----
    connect(m_storeButton, &QPushButton::clicked, this, &MainWindow::openStore);
    connect(logoutButton, &QPushButton::clicked, this, &MainWindow::logout);
    connect(browseButton, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(
            this, QStringLiteral("Selecciona dota2.exe"),
            m_dotaPath->text(),
            QStringLiteral("Dota 2 executable (dota2.exe);;All files (*.*)"));
        if (DotaPathDetector::isValid(path))
            m_dotaPath->setText(path);
        else if (!path.isEmpty())
            m_statusBar->showMessage(QStringLiteral("El archivo seleccionado no es un dota2.exe válido"));
    });
    connect(detectButton, &QPushButton::clicked, this, [this]() {
        const QString path = DotaPathDetector::detect(m_dotaPath->text());
        if (!path.isEmpty())
        {
            m_dotaPath->setText(path);
            m_store.config().dota2Path = path;
            m_store.save();
            m_statusBar->showMessage(QStringLiteral("Dota 2 detectado automáticamente"));
        }
        else
            m_statusBar->showMessage(QStringLiteral("No se encontró dota2.exe; selecciona la carpeta o el ejecutable"));
    });
    connect(applyServerButton, &QPushButton::clicked, this, [this]() {
        m_store.config().serverUrl = m_serverUrl->text().trimmed();
        m_server.setBaseUrl(m_store.config().serverUrl);
        m_store.save();
        refreshStatus();
    });
    connect(m_playButton, &QPushButton::clicked, this, &MainWindow::play);

    connect(&m_server, &ServerClient::pingFinished, this,
            [this](bool reachable, const QString &version) {
                m_statusLabel->setObjectName(reachable
                    ? QStringLiteral("StatusOnline")
                    : QStringLiteral("StatusOffline"));
                m_statusLabel->setText(reachable
                    ? (version.isEmpty()
                        ? QStringLiteral("● SERVIDOR EN LÍNEA")
                        : QStringLiteral("● EN LÍNEA · %1").arg(version))
                    : QStringLiteral("● SERVIDOR SIN RESPUESTA"));
                if (m_statusLabel->style())
                {
                    m_statusLabel->style()->unpolish(m_statusLabel);
                    m_statusLabel->style()->polish(m_statusLabel);
                }
            });

    connect(&m_server, &ServerClient::meFinished, this,
            [this](bool ok, const QString &, const QString &personaName,
                   quint32 accountId, quint64 steamId, int playerLevel) {
                if (!ok)
                {
                    m_accountName->setText(QStringLiteral("Sesión expirada"));
                    m_accountMeta->setText(QStringLiteral("Vuelve a iniciar sesión"));
                    m_level->setText(QStringLiteral("Nivel --"));
                    return;
                }
                for (auto &profile : m_store.config().profiles)
                {
                    if (profile.username == m_store.config().currentUsername)
                    {
                        if (!personaName.isEmpty())
                            profile.displayName = personaName;
                        profile.accountId = accountId;
                        profile.steamId = steamId;
                    }
                }
                m_store.save();
                m_accountName->setText(personaName.isEmpty()
                                           ? QStringLiteral("Jugador")
                                           : personaName);
                m_accountMeta->setText(QStringLiteral("ID %1  •  Steam %2")
                                           .arg(accountId)
                                           .arg(steamId));
                m_level->setText(QStringLiteral("Nivel %1").arg(playerLevel));

                QString token;
                for (const auto &profile : m_store.config().profiles)
                {
                    if (profile.username == m_store.config().currentUsername)
                    {
                        token = profile.token;
                        break;
                    }
                }
                m_server.fetchAvatar(steamId, token);
                m_server.fetchRank(token);
            });

    connect(&m_server, &ServerClient::rankFinished, this,
            [this](bool ok, int mmr, int rankTier, int rankStar) {
                m_rankLabel->setText(ok
                    ? QStringLiteral("MMR %1  •  %2 %3")
                          .arg(mmr)
                          .arg(medalName(rankTier))
                          .arg(rankStar)
                    : QStringLiteral("MMR --"));
            });

    connect(&m_server, &ServerClient::avatarFinished, this,
            [this](bool ok, const QByteArray &png) {
                if (!ok)
                    return;
                const auto pixmap = circularAvatar(png, 64);
                if (pixmap.isNull())
                    return;
                m_avatar->setPixmap(pixmap);
                m_avatar->setText(QString());
            });

    connect(&m_server, &ServerClient::storeHandoffFinished, this,
            [this](bool ok, const QString &error, const QString &path) {
                if (!ok)
                {
                    m_statusBar->showMessage(error);
                    QMessageBox::warning(this, QStringLiteral("Tienda"), error);
                    if (m_storeButton)
                        m_storeButton->setEnabled(true);
                    return;
                }

                const auto url = m_server.urlForPath(path);
                if (!QDesktopServices::openUrl(url))
                {
                    QMessageBox::information(
                        this,
                        QStringLiteral("Tienda"),
                        QStringLiteral("Copia este enlace en tu navegador:\n%1").arg(url.toString()));
                }
                m_statusBar->showMessage(QStringLiteral("Tienda abierta en el navegador"));
                if (m_storeButton)
                    m_storeButton->setEnabled(true);
            });

    const auto detectedPath = DotaPathDetector::detect(m_dotaPath->text());
    if (!detectedPath.isEmpty() && detectedPath != m_dotaPath->text())
    {
        m_dotaPath->setText(detectedPath);
        m_store.config().dota2Path = detectedPath;
        m_store.save();
        m_statusBar->showMessage(QStringLiteral("Dota 2 detectado automáticamente"));
    }
}

void MainWindow::applyCurrentProfile()
{
    const auto &profiles = m_store.config().profiles;
    const auto it = std::find_if(profiles.cbegin(), profiles.cend(),
                                 [this](const Profile &profile) {
                                     return profile.username == m_store.config().currentUsername;
                                 });
    if (it == profiles.cend())
    {
        m_accountName->setText(QStringLiteral("Sin cuenta"));
        m_accountMeta->setText(QStringLiteral("Inicia sesión para continuar"));
        m_avatar->setPixmap(QPixmap());
        m_avatar->setText(QStringLiteral("?"));
        m_rankLabel->setText(QStringLiteral("MMR --"));
        return;
    }
    const QString name = it->displayName.isEmpty() ? it->username : it->displayName;
    m_accountName->setText(name);
    m_accountMeta->setText(QStringLiteral("ID %1  •  Steam %2")
                               .arg(it->accountId)
                               .arg(it->steamId));
    m_avatar->setPixmap(QPixmap());
    m_avatar->setText(name.left(1).toUpper());
    m_rankLabel->setText(QStringLiteral("MMR --"));
}

void MainWindow::validateCurrentSession()
{
    const auto &profiles = m_store.config().profiles;
    const auto it = std::find_if(profiles.cbegin(), profiles.cend(),
                                 [this](const Profile &profile) {
                                     return profile.username == m_store.config().currentUsername;
                                 });
    if (it == profiles.cend() || it->token.isEmpty())
        return;
    m_server.me(it->token);
}

bool MainWindow::addAccount()
{
    // No owner window: an owned (parented) dialog gets no taskbar button on
    // Windows. A parentless application-modal dialog is still modal and appears
    // in the taskbar, so it can be minimized and restored normally.
    LoginDialog dialog(m_store.config(), m_server);
    if (dialog.exec() != QDialog::Accepted)
        return false;
    return applyAccount(dialog);
}

bool MainWindow::applyAccount(LoginDialog &dialog)
{
    auto &profiles = m_store.config().profiles;
    auto it = std::find_if(profiles.begin(), profiles.end(),
                           [&dialog](const Profile &profile) {
                               return profile.username.compare(dialog.username(),
                                                               Qt::CaseInsensitive) == 0;
                           });
    if (it == profiles.end())
    {
        profiles.append(Profile{});
        it = profiles.end() - 1;
    }

    it->username = dialog.username();
    it->displayName = dialog.displayName();
    it->token = dialog.token();
    it->tokenSavedAtMs = QDateTime::currentMSecsSinceEpoch();
    it->lastLoginMs = it->tokenSavedAtMs;
    it->accountId = dialog.accountId();
    it->steamId = dialog.steamId();
    m_store.config().currentUsername = it->username;
    m_store.config().rememberMe = dialog.rememberMe();
    m_store.save();

    applyCurrentProfile();
    validateCurrentSession();
    m_statusBar->showMessage(QStringLiteral("Sesión iniciada como %1").arg(it->username));
    return true;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    const auto &profiles = m_store.config().profiles;
    const auto it = std::find_if(profiles.cbegin(), profiles.cend(),
                                 [this](const Profile &profile) {
                                     return profile.username == m_store.config().currentUsername;
                                 });
    if (it != profiles.cend() && !it->token.isEmpty())
        m_server.logout(it->token, /*waitForDelivery=*/true);
    event->accept();
}

void MainWindow::logout()
{
    QString previousToken;
    auto &profiles = m_store.config().profiles;
    const auto it = std::find_if(profiles.begin(), profiles.end(),
                                 [this](const Profile &profile) {
                                     return profile.username == m_store.config().currentUsername;
                                 });
    if (it != profiles.end())
    {
        previousToken = it->token;
        it->token.clear();
        it->tokenSavedAtMs = 0;
    }
    if (!previousToken.isEmpty())
        m_server.logout(previousToken);
    m_store.save();

    // The dashboard is only usable while logged in: closing the session
    // requires logging back in, and cancelling closes the launcher. Hide it
    // while the login dialog is up so it does not stay visible behind it.
    hide();
    LoginDialog dialog(m_store.config(), m_server);
    if (dialog.exec() == QDialog::Accepted)
    {
        applyAccount(dialog);
        show();
        return;
    }
    qApp->quit();
}

void MainWindow::refreshStatus()
{
    if (m_serverUrl->text().trimmed() != m_store.config().serverUrl)
        m_store.config().serverUrl = m_serverUrl->text().trimmed();
    m_server.setBaseUrl(m_store.config().serverUrl);
    m_server.ping();
}

void MainWindow::play()
{
    if (m_runningPid != 0)
    {
        stop();
        return;
    }

    const auto &profiles = m_store.config().profiles;
    const auto it = std::find_if(profiles.cbegin(), profiles.cend(),
                                 [this](const Profile &profile) {
                                     return profile.username == m_store.config().currentUsername;
                                 });
    if (it == profiles.cend())
    {
        QMessageBox::information(this, QStringLiteral("Cuenta"),
                                 QStringLiteral("Agrega una cuenta antes de jugar."));
        return;
    }

    const QString dotaPath = DotaPathDetector::resolve(m_dotaPath->text());
    if (dotaPath.isEmpty())
    {
        QMessageBox::information(this, QStringLiteral("Dota 2"),
                                 QStringLiteral("No se encontró dota2.exe. Usa Auto-detectar o selecciona la instalación de Dota 2."));
        return;
    }

    m_dotaPath->setText(dotaPath);
    m_store.config().dota2Path = dotaPath;
    m_store.config().enableConsole = m_console->isChecked();
    m_store.config().skipIntro = m_novid->isChecked();
    m_store.config().insecureMode = m_insecure->isChecked();
    m_store.save();

    m_playButton->setEnabled(false);
    m_playButton->setText(QStringLiteral("LANZANDO..."));
    m_statusBar->showMessage(QStringLiteral("Lanzando Dota 2..."));

    const auto outcome = m_launcher.launch(dotaPath, *it, m_store.config(),
                                           m_store.config().serverUrl);
    m_playButton->setEnabled(true);
    if (!outcome.success)
    {
        m_playButton->setText(QStringLiteral("JUGAR DOTA 2"));
        m_statusBar->showMessage(QStringLiteral("Error al lanzar: %1").arg(outcome.error));
        QMessageBox::warning(this, QStringLiteral("Error de lanzamiento"), outcome.error);
        return;
    }

    m_runningPid = outcome.pid;
    m_runningHandle = outcome.processHandle;
    m_playButton->setText(QStringLiteral("DETENER DOTA 2"));
    m_statusBar->showMessage(QStringLiteral("Dota 2 ejecutándose (PID %1)").arg(outcome.pid));
}

void MainWindow::openStore()
{
    const auto &profiles = m_store.config().profiles;
    const auto it = std::find_if(profiles.cbegin(), profiles.cend(),
                                 [this](const Profile &profile) {
                                     return profile.username == m_store.config().currentUsername;
                                 });
    if (it == profiles.cend() || it->token.isEmpty())
    {
        QMessageBox::information(
            this,
            QStringLiteral("Tienda"),
            QStringLiteral("Inicia sesión antes de abrir la tienda."));
        return;
    }

    m_storeButton->setEnabled(false);
    m_statusBar->showMessage(QStringLiteral("Preparando acceso seguro a la tienda…"));
    m_server.createStoreHandoff(it->token);
}

void MainWindow::stop()
{
    m_launcher.stop(m_runningPid, m_runningHandle);
    m_runningPid = 0;
    m_runningHandle = nullptr;
    m_playButton->setText(QStringLiteral("JUGAR DOTA 2"));
    m_statusBar->showMessage(QStringLiteral("Dota 2 detenido"));
}

void MainWindow::monitorProcess()
{
    if (m_runningPid == 0)
        return;
    if (m_launcher.isRunning(m_runningPid, m_runningHandle))
        return;

    m_runningPid = 0;
    m_runningHandle = nullptr;
    m_playButton->setText(QStringLiteral("JUGAR DOTA 2"));
    m_statusBar->showMessage(QStringLiteral("Dota 2 finalizó"));
}
