#include "MainWindow.h"

#include "LoginDialog.h"

#include "../launch/DotaPathDetector.h"
#include "../util/Log.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStatusBar>
#include <QVBoxLayout>

#include <algorithm>

MainWindow::MainWindow(ConfigStore &store, ServerClient &server, QWidget *parent)
    : QMainWindow(parent)
    , m_store(store)
    , m_server(server)
{
    setWindowTitle(QStringLiteral("D2Max Launcher"));
    resize(860, 620);
    buildUi();

    rebuildProfiles();
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
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(24, 20, 24, 14);
    layout->setSpacing(12);

    // ---- header ----
    auto *header = new QHBoxLayout;
    auto *title = new QLabel(QStringLiteral("D2MAX LAUNCHER"), central);
    title->setObjectName(QStringLiteral("TitleLabel"));
    m_statusLabel = new QLabel(QStringLiteral("--"), central);
    m_statusLabel->setObjectName(QStringLiteral("SubtitleLabel"));
    auto *addButton = new QPushButton(QStringLiteral("+ AGREGAR CUENTA"), central);
    addButton->setObjectName(QStringLiteral("GhostButton"));
    auto *logoutButton = new QPushButton(QStringLiteral("CERRAR SESIÓN"), central);
    logoutButton->setObjectName(QStringLiteral("GhostButton"));
    header->addWidget(title);
    header->addStretch();
    header->addWidget(m_statusLabel);
    header->addWidget(addButton);
    header->addWidget(logoutButton);
    layout->addLayout(header);

    // ---- account card ----
    auto *accountBox = new QGroupBox(QStringLiteral("CUENTA"), central);
    auto *accountLayout = new QVBoxLayout(accountBox);
    auto *accountRow = new QHBoxLayout;
    m_accountCombo = new QComboBox(accountBox);
    m_accountCombo->setMinimumWidth(260);
    m_accountName = new QLabel(QStringLiteral("Sin cuenta"), accountBox);
    m_accountName->setStyleSheet(QStringLiteral("font-size: 15px; font-weight: 700;"));
    m_accountMeta = new QLabel(QStringLiteral("--"), accountBox);
    m_accountMeta->setObjectName(QStringLiteral("SubtitleLabel"));
    m_level = new QLabel(QStringLiteral("Nivel --"), accountBox);
    m_level->setObjectName(QStringLiteral("SubtitleLabel"));
    accountRow->addWidget(m_accountCombo);
    accountRow->addSpacing(16);
    accountRow->addWidget(m_accountName);
    accountRow->addSpacing(16);
    accountRow->addWidget(m_accountMeta);
    accountRow->addStretch();
    accountRow->addWidget(m_level);
    accountLayout->addLayout(accountRow);
    layout->addWidget(accountBox);

    // ---- dota path ----
    auto *gameBox = new QGroupBox(QStringLiteral("DOTA 2"), central);
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
    connect(m_accountCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index < 0 || index >= m_store.config().profiles.size())
            return;
        m_store.config().currentUsername = m_store.config().profiles.at(index).username;
        m_store.save();
        applyCurrentProfile();
        validateCurrentSession();
    });
    connect(addButton, &QPushButton::clicked, this, &MainWindow::addAccount);
    connect(logoutButton, &QPushButton::clicked, this, &MainWindow::logout);
    connect(browseButton, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(
            this, QStringLiteral("Selecciona dota2.exe"),
            m_dotaPath->text(),
            QStringLiteral("Dota 2 executable (dota2.exe);;All files (*.*)"));
        if (!path.isEmpty())
            m_dotaPath->setText(path);
    });
    connect(detectButton, &QPushButton::clicked, this, [this]() {
        const QString path = DotaPathDetector::detect();
        if (!path.isEmpty())
            m_dotaPath->setText(path);
        else
            m_statusBar->showMessage(QStringLiteral("No se encontró Dota 2 automáticamente"));
    });
    connect(applyServerButton, &QPushButton::clicked, this, [this]() {
        m_store.config().serverUrl = m_serverUrl->text().trimmed();
        m_server.setBaseUrl(m_store.config().serverUrl);
        m_store.save();
        refreshStatus();
    });
    connect(m_playButton, &QPushButton::clicked, this, &MainWindow::play);

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
            });
}

void MainWindow::rebuildProfiles()
{
    const QString current = m_store.config().currentUsername;
    m_accountCombo->clear();
    for (const auto &profile : m_store.config().profiles)
        m_accountCombo->addItem(profile.username);

    const int index = std::max(0, [&]() {
        for (int i = 0; i < m_store.config().profiles.size(); ++i)
        {
            if (m_store.config().profiles.at(i).username == current)
                return i;
        }
        return 0;
    }());
    m_accountCombo->setCurrentIndex(index);
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
        m_accountMeta->setText(QStringLiteral("Agrega una cuenta para jugar"));
        return;
    }
    m_accountName->setText(it->displayName.isEmpty() ? it->username : it->displayName);
    m_accountMeta->setText(QStringLiteral("ID %1  •  Steam %2")
                               .arg(it->accountId)
                               .arg(it->steamId));
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

    rebuildProfiles();
    applyCurrentProfile();
    validateCurrentSession();
    m_statusBar->showMessage(QStringLiteral("Sesión iniciada como %1").arg(it->username));
    return true;
}

void MainWindow::logout()
{
    auto &profiles = m_store.config().profiles;
    const auto it = std::find_if(profiles.begin(), profiles.end(),
                                 [this](const Profile &profile) {
                                     return profile.username == m_store.config().currentUsername;
                                 });
    if (it != profiles.end())
    {
        it->token.clear();
        it->tokenSavedAtMs = 0;
        m_store.save();
    }
    m_accountName->setText(QStringLiteral("Sesión cerrada"));
    m_accountMeta->setText(QStringLiteral("Vuelve a iniciar sesión para jugar"));
    m_statusBar->showMessage(QStringLiteral("Sesión cerrada"));
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

    const QString dotaPath = m_dotaPath->text().trimmed();
    if (dotaPath.isEmpty())
    {
        QMessageBox::information(this, QStringLiteral("Dota 2"),
                                 QStringLiteral("Selecciona dota2.exe antes de jugar."));
        return;
    }

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
