#include "MainWindow.h"

#include "LoginDialog.h"
#include "StoreView.h"

#include "../launch/DotaPathDetector.h"
#include "../util/Log.h"

#include <QCheckBox>
#include <QComboBox>
#include <QApplication>
#include <QCloseEvent>
#include <QDateTime>
#include <QFileDialog>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QStringList>
#include <QStackedWidget>
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
    resize(1240, 820);
    setMinimumSize(1050, 700);
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
    buildDashboardUi();
}

void MainWindow::buildDashboardUi()
{
    setWindowTitle(QStringLiteral("D2Max · Centro de juego"));
    resize(1240, 820);
    setMinimumSize(1050, 700);

    auto *central = new QWidget(this);
    central->setObjectName(QStringLiteral("LauncherRoot"));
    auto *shell = new QHBoxLayout(central);
    shell->setContentsMargins(22, 22, 22, 16);
    shell->setSpacing(20);

    auto *sidebar = new QFrame(central);
    sidebar->setObjectName(QStringLiteral("Sidebar"));
    sidebar->setFixedWidth(222);
    addCardShadow(sidebar);
    auto *sideLayout = new QVBoxLayout(sidebar);
    sideLayout->setContentsMargins(20, 22, 20, 20);
    sideLayout->setSpacing(12);

    auto *brandRow = new QHBoxLayout;
    auto *brandMark = new QLabel(QStringLiteral("D2"), sidebar);
    brandMark->setObjectName(QStringLiteral("BrandMark"));
    brandMark->setFixedSize(44, 44);
    brandMark->setAlignment(Qt::AlignCenter);
    auto *brandText = new QVBoxLayout;
    auto *brandName = new QLabel(QStringLiteral("D2MAX"), sidebar);
    brandName->setObjectName(QStringLiteral("BrandName"));
    auto *brandCaption = new QLabel(QStringLiteral("GAME HUB"), sidebar);
    brandCaption->setObjectName(QStringLiteral("BrandCaption"));
    brandText->addWidget(brandName);
    brandText->addWidget(brandCaption);
    brandRow->addWidget(brandMark);
    brandRow->addSpacing(10);
    brandRow->addLayout(brandText);
    brandRow->addStretch();
    sideLayout->addLayout(brandRow);
    sideLayout->addSpacing(22);

    auto *navigationLabel = new QLabel(QStringLiteral("NAVEGACIÓN"), sidebar);
    navigationLabel->setObjectName(QStringLiteral("SideCaption"));
    sideLayout->addWidget(navigationLabel);

    m_homeButton = new QPushButton(QStringLiteral("  INICIO"), sidebar);
    m_homeButton->setObjectName(QStringLiteral("NavButtonActive"));
    m_homeButton->setMinimumHeight(44);
    sideLayout->addWidget(m_homeButton);

    m_storeButton = new QPushButton(QStringLiteral("  TIENDA"), sidebar);
    m_storeButton->setObjectName(QStringLiteral("NavButton"));
    m_storeButton->setMinimumHeight(44);
    sideLayout->addWidget(m_storeButton);

    auto *sideInfo = new QFrame(sidebar);
    sideInfo->setObjectName(QStringLiteral("SidebarStatus"));
    auto *sideInfoLayout = new QVBoxLayout(sideInfo);
    sideInfoLayout->setContentsMargins(13, 12, 13, 12);
    auto *sideInfoTitle = new QLabel(QStringLiteral("TU SESIÓN"), sideInfo);
    sideInfoTitle->setObjectName(QStringLiteral("SideCaption"));
    auto *sideInfoText = new QLabel(QStringLiteral("El launcher mantiene tu acceso y prepara Dota 2 con la configuración guardada."), sideInfo);
    sideInfoText->setObjectName(QStringLiteral("SideInfo"));
    sideInfoText->setWordWrap(true);
    sideInfoLayout->addWidget(sideInfoTitle);
    sideInfoLayout->addWidget(sideInfoText);
    sideLayout->addWidget(sideInfo);
    sideLayout->addStretch();

    auto *sideVersion = new QLabel(QStringLiteral("D2MAX LAUNCHER  ·  1.0"), sidebar);
    sideVersion->setObjectName(QStringLiteral("SideVersion"));
    sideLayout->addWidget(sideVersion);
    auto *logoutButton = new QPushButton(QStringLiteral("CERRAR SESIÓN"), sidebar);
    logoutButton->setObjectName(QStringLiteral("GhostButton"));
    logoutButton->setMinimumHeight(40);
    sideLayout->addWidget(logoutButton);
    shell->addWidget(sidebar);

    m_pageStack = new QStackedWidget(central);
    m_pageStack->setObjectName(QStringLiteral("PageStack"));

    auto *dashboardScroll = new QScrollArea(m_pageStack);
    dashboardScroll->setObjectName(QStringLiteral("DashboardScroll"));
    dashboardScroll->setWidgetResizable(true);
    dashboardScroll->setFrameShape(QFrame::NoFrame);
    dashboardScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *content = new QWidget;
    content->setObjectName(QStringLiteral("DashboardPage"));
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(4, 4, 12, 18);
    contentLayout->setSpacing(18);

    auto *topbar = new QHBoxLayout;
    auto *topCopy = new QVBoxLayout;
    auto *topTitle = new QLabel(QStringLiteral("CENTRO DE JUEGO"), content);
    topTitle->setObjectName(QStringLiteral("TitleLabel"));
    auto *topSubtitle = new QLabel(QStringLiteral("Todo listo para entrar a la partida"), content);
    topSubtitle->setObjectName(QStringLiteral("SubtitleLabel"));
    topCopy->addWidget(topTitle);
    topCopy->addWidget(topSubtitle);
    m_statusLabel = new QLabel(QStringLiteral("● COMPROBANDO SERVIDOR"), content);
    m_statusLabel->setObjectName(QStringLiteral("StatusChecking"));
    topbar->addLayout(topCopy);
    topbar->addStretch();
    topbar->addWidget(m_statusLabel);
    contentLayout->addLayout(topbar);

    auto *hero = new QFrame(content);
    hero->setObjectName(QStringLiteral("HeroCard"));
    addCardShadow(hero);
    auto *heroLayout = new QHBoxLayout(hero);
    heroLayout->setContentsMargins(30, 28, 24, 24);
    heroLayout->setSpacing(24);

    auto *heroCopy = new QVBoxLayout;
    auto *heroEyebrow = new QLabel(QStringLiteral("D2MAX  /  PARTIDA LOCAL"), hero);
    heroEyebrow->setObjectName(QStringLiteral("HeroEyebrow"));
    auto *heroTitle = new QLabel(QStringLiteral("Juega a tu manera."), hero);
    heroTitle->setObjectName(QStringLiteral("HeroTitle"));
    heroTitle->setWordWrap(true);
    auto *heroText = new QLabel(QStringLiteral("Tu cuenta, tu inventario y tu servidor en un solo lugar. Entra a Dota 2 cuando estés listo."), hero);
    heroText->setObjectName(QStringLiteral("HeroText"));
    heroText->setWordWrap(true);
    heroText->setMaximumWidth(510);
    m_playButton = new QPushButton(QStringLiteral("JUGAR DOTA 2"), hero);
    m_playButton->setObjectName(QStringLiteral("LaunchButton"));
    m_playButton->setMinimumHeight(52);
    m_playButton->setMinimumWidth(205);
    heroCopy->addWidget(heroEyebrow);
    heroCopy->addWidget(heroTitle);
    heroCopy->addWidget(heroText);
    heroCopy->addSpacing(13);
    heroCopy->addWidget(m_playButton, 0, Qt::AlignLeft);
    heroCopy->addStretch();
    heroLayout->addLayout(heroCopy, 1);

    auto *accountCard = new QFrame(hero);
    accountCard->setObjectName(QStringLiteral("AccountCard"));
    accountCard->setMinimumWidth(270);
    auto *accountLayout = new QVBoxLayout(accountCard);
    accountLayout->setContentsMargins(18, 18, 18, 18);
    accountLayout->setSpacing(10);
    auto *accountCaption = new QLabel(QStringLiteral("CUENTA ACTIVA"), accountCard);
    accountCaption->setObjectName(QStringLiteral("SideCaption"));
    auto *accountRow = new QHBoxLayout;
    m_avatar = new QLabel(accountCard);
    m_avatar->setObjectName(QStringLiteral("AvatarLabel"));
    m_avatar->setFixedSize(72, 72);
    m_avatar->setAlignment(Qt::AlignCenter);
    m_avatar->setText(QStringLiteral("?"));
    auto *infoCol = new QVBoxLayout;
    m_accountName = new QLabel(QStringLiteral("Sin cuenta"), accountCard);
    m_accountName->setObjectName(QStringLiteral("AccountName"));
    m_accountName->setWordWrap(true);
    m_accountMeta = new QLabel(QStringLiteral("Inicia sesión para continuar"), accountCard);
    m_accountMeta->setObjectName(QStringLiteral("SubtitleLabel"));
    m_accountMeta->setWordWrap(true);
    infoCol->addWidget(m_accountName);
    infoCol->addWidget(m_accountMeta);
    accountRow->addWidget(m_avatar);
    accountRow->addSpacing(12);
    accountRow->addLayout(infoCol, 1);
    accountLayout->addWidget(accountCaption);
    accountLayout->addLayout(accountRow);
    heroLayout->addWidget(accountCard);
    contentLayout->addWidget(hero);

    auto *metrics = new QHBoxLayout;
    metrics->setSpacing(12);
    auto makeMetric = [&](const QString &label, QLabel *&value) {
        auto *card = new QFrame(content);
        card->setObjectName(QStringLiteral("MetricCard"));
        auto *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(17, 14, 17, 14);
        auto *caption = new QLabel(label, card);
        caption->setObjectName(QStringLiteral("MetricLabel"));
        value = new QLabel(QStringLiteral("--"), card);
        value->setObjectName(QStringLiteral("MetricValue"));
        cardLayout->addWidget(caption);
        cardLayout->addWidget(value);
        metrics->addWidget(card, 1);
    };
    makeMetric(QStringLiteral("RANGO COMPETITIVO"), m_rankLabel);
    makeMetric(QStringLiteral("NIVEL DE CUENTA"), m_level);
    auto *regionCard = new QFrame(content);
    regionCard->setObjectName(QStringLiteral("MetricCard"));
    auto *regionLayout = new QVBoxLayout(regionCard);
    regionLayout->setContentsMargins(17, 14, 17, 14);
    auto *regionCaption = new QLabel(QStringLiteral("ENTORNO"), regionCard);
    regionCaption->setObjectName(QStringLiteral("MetricLabel"));
    auto *regionValue = new QLabel(QStringLiteral("SERVIDOR D2MAX"), regionCard);
    regionValue->setObjectName(QStringLiteral("MetricValue"));
    regionLayout->addWidget(regionCaption);
    regionLayout->addWidget(regionValue);
    metrics->addWidget(regionCard, 1);
    contentLayout->addLayout(metrics);

    auto *settings = new QFrame(content);
    settings->setObjectName(QStringLiteral("SurfaceCard"));
    addCardShadow(settings);
    auto *settingsLayout = new QVBoxLayout(settings);
    settingsLayout->setContentsMargins(20, 17, 20, 17);
    settingsLayout->setSpacing(12);
    auto *settingsHead = new QHBoxLayout;
    auto *settingsTitle = new QLabel(QStringLiteral("CONFIGURACIÓN DE SESIÓN"), settings);
    settingsTitle->setObjectName(QStringLiteral("SectionLabel"));
    auto *settingsHint = new QLabel(QStringLiteral("Se conserva en este equipo"), settings);
    settingsHint->setObjectName(QStringLiteral("SubtitleLabel"));
    settingsHead->addWidget(settingsTitle);
    settingsHead->addStretch();
    settingsHead->addWidget(settingsHint);
    settingsLayout->addLayout(settingsHead);

    auto *pathTitle = new QLabel(QStringLiteral("UBICACIÓN DE DOTA 2"), settings);
    pathTitle->setObjectName(QStringLiteral("FieldLabel"));
    settingsLayout->addWidget(pathTitle);
    auto *pathRow = new QHBoxLayout;
    pathRow->setSpacing(8);
    m_dotaPath = new QLineEdit(m_store.config().dota2Path, settings);
    m_dotaPath->setObjectName(QStringLiteral("PathField"));
    m_dotaPath->setPlaceholderText(QStringLiteral("Ruta a dota2.exe o a la instalación de Dota 2"));
    auto *browseButton = new QPushButton(QStringLiteral("EXAMINAR"), settings);
    browseButton->setObjectName(QStringLiteral("GhostButton"));
    auto *detectButton = new QPushButton(QStringLiteral("AUTO-DETECTAR"), settings);
    detectButton->setObjectName(QStringLiteral("GhostButton"));
    pathRow->addWidget(m_dotaPath, 1);
    pathRow->addWidget(browseButton);
    pathRow->addWidget(detectButton);
    settingsLayout->addLayout(pathRow);

    auto *serverTitle = new QLabel(QStringLiteral("SERVIDOR"), settings);
    serverTitle->setObjectName(QStringLiteral("FieldLabel"));
    settingsLayout->addWidget(serverTitle);
    auto *serverRow = new QHBoxLayout;
    serverRow->setSpacing(8);
    m_serverUrl = new QLineEdit(m_store.config().serverUrl, settings);
    m_serverUrl->setObjectName(QStringLiteral("PathField"));
    m_serverUrl->setPlaceholderText(QStringLiteral("http://127.0.0.1:27015"));
    auto *applyServerButton = new QPushButton(QStringLiteral("APLICAR"), settings);
    applyServerButton->setObjectName(QStringLiteral("GhostButton"));
    serverRow->addWidget(m_serverUrl, 1);
    serverRow->addWidget(applyServerButton);
    settingsLayout->addLayout(serverRow);

    auto *argsRow = new QHBoxLayout;
    argsRow->setSpacing(18);
    m_console = new QCheckBox(QStringLiteral("Consola -console"), settings);
    m_console->setChecked(m_store.config().enableConsole);
    m_novid = new QCheckBox(QStringLiteral("Saltar intro -novid"), settings);
    m_novid->setChecked(m_store.config().skipIntro);
    m_insecure = new QCheckBox(QStringLiteral("Modo local -insecure"), settings);
    m_insecure->setChecked(m_store.config().insecureMode);
    argsRow->addWidget(m_console);
    argsRow->addWidget(m_novid);
    argsRow->addWidget(m_insecure);
    argsRow->addStretch();
    settingsLayout->addLayout(argsRow);
    contentLayout->addWidget(settings);

    auto *footerCard = new QFrame(content);
    footerCard->setObjectName(QStringLiteral("FooterCard"));
    auto *footerLayout = new QHBoxLayout(footerCard);
    footerLayout->setContentsMargins(16, 11, 16, 11);
    auto *footerDot = new QLabel(QStringLiteral("●"), footerCard);
    footerDot->setObjectName(QStringLiteral("FooterDot"));
    auto *footerText = new QLabel(QStringLiteral("La tienda está integrada en el launcher y comparte tu sesión con Dota 2."), footerCard);
    footerText->setObjectName(QStringLiteral("SubtitleLabel"));
    footerText->setWordWrap(true);
    footerLayout->addWidget(footerDot);
    footerLayout->addSpacing(8);
    footerLayout->addWidget(footerText, 1);
    contentLayout->addWidget(footerCard);
    contentLayout->addStretch();
    dashboardScroll->setWidget(content);
    m_pageStack->addWidget(dashboardScroll);
    m_storeView = new StoreView(m_server, m_pageStack);
    m_pageStack->addWidget(m_storeView);
    shell->addWidget(m_pageStack, 1);

    setCentralWidget(central);
    m_statusBar = statusBar();
    m_statusBar->showMessage(QStringLiteral("Listo para jugar"));

    connect(m_homeButton, &QPushButton::clicked, this, &MainWindow::showDashboard);
    connect(m_storeButton, &QPushButton::clicked, this, &MainWindow::openStore);
    connect(m_storeView, &StoreView::backRequested, this, &MainWindow::showDashboard);
    connect(m_storeView, &StoreView::loginRequested, this, [this]() {
        if (addAccount())
        {
            m_storeView->setSessionToken(currentToken());
            m_storeView->reload();
        }
    });
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
            m_statusBar->showMessage(QStringLiteral("No se encontró dota2.exe; selecciona la instalación"));
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
                    ? QStringLiteral("MMR %1  ·  %2 %3")
                          .arg(mmr)
                          .arg(medalName(rankTier))
                          .arg(rankStar)
                    : QStringLiteral("MMR --"));
            });

    connect(&m_server, &ServerClient::avatarFinished, this,
            [this](bool ok, const QByteArray &png) {
                if (!ok)
                    return;
                const auto pixmap = circularAvatar(png, 72);
                if (pixmap.isNull())
                    return;
                m_avatar->setPixmap(pixmap);
                m_avatar->setText(QString());
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
        m_level->setText(QStringLiteral("Nivel --"));
        if (m_storeView)
            m_storeView->setSessionToken(QString());
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
    m_level->setText(QStringLiteral("Nivel --"));
    if (m_storeView)
        m_storeView->setSessionToken(it->token);
}

QString MainWindow::currentToken() const
{
    const auto &profiles = m_store.config().profiles;
    const auto it = std::find_if(profiles.cbegin(), profiles.cend(),
                                 [this](const Profile &profile) {
                                     return profile.username == m_store.config().currentUsername;
                                 });
    return it == profiles.cend() ? QString() : it->token;
}

void MainWindow::showDashboard()
{
    if (!m_pageStack)
        return;
    m_pageStack->setCurrentIndex(0);
    m_homeButton->setObjectName(QStringLiteral("NavButtonActive"));
    m_storeButton->setObjectName(QStringLiteral("NavButton"));
    for (auto *button : {m_homeButton, m_storeButton})
    {
        if (button && button->style())
        {
            button->style()->unpolish(button);
            button->style()->polish(button);
        }
    }
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
    if (m_storeView)
        m_storeView->setSessionToken(QString());
    showDashboard();

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
    const auto token = currentToken();
    m_storeView->setSessionToken(token);
    m_pageStack->setCurrentIndex(1);
    m_homeButton->setObjectName(QStringLiteral("NavButton"));
    m_storeButton->setObjectName(QStringLiteral("NavButtonActive"));
    for (auto *button : {m_homeButton, m_storeButton})
    {
        if (button && button->style())
        {
            button->style()->unpolish(button);
            button->style()->polish(button);
        }
    }
    m_statusBar->showMessage(QStringLiteral("Tienda integrada abierta"));
    m_storeView->reload();
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
