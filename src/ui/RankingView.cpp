#include "RankingView.h"

#include <QColor>
#include <QFrame>
#include <QStyle>
#include <QFont>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QStringList>
#include <QVBoxLayout>

#include <algorithm>

namespace
{
QString medalName(int tier)
{
    static const QStringList names = {
        QStringLiteral("Heraldo"), QStringLiteral("Guardián"), QStringLiteral("Cruzado"),
        QStringLiteral("Arconte"), QStringLiteral("Leyenda"), QStringLiteral("Antiguo"),
        QStringLiteral("Divino"), QStringLiteral("Inmortal")};
    return (tier >= 1 && tier <= names.size())
        ? names.at(tier - 1)
        : QStringLiteral("Sin calibrar");
}

QString stars(int count)
{
    if (count <= 0)
        return QStringLiteral("Rango máximo");
    return QString(count, QChar(0x2605));
}

void addShadow(QWidget *widget)
{
    auto *effect = new QGraphicsDropShadowEffect(widget);
    effect->setBlurRadius(16);
    effect->setOffset(0, 4);
    effect->setColor(QColor(0, 0, 0, 60));
    widget->setGraphicsEffect(effect);
}

/// The official Dota 2 medal source is the VPK path
/// panorama/images/rank_tier_icons/rank{0..8}_psd.vtex_c. Those compiled
/// assets cannot be read by Qt directly, so the launcher renders a compact
/// local vector badge with the same tier identity and does not depend on an
/// external web session or CDN at runtime.
QPixmap medalIcon(int tier, int star, int size)
{
    static const QVector<QColor> colors = {
        QColor(QStringLiteral("#64748b")), QColor(QStringLiteral("#a16207")),
        QColor(QStringLiteral("#65a30d")), QColor(QStringLiteral("#0891b2")),
        QColor(QStringLiteral("#2563eb")), QColor(QStringLiteral("#7c3aed")),
        QColor(QStringLiteral("#db2777")), QColor(QStringLiteral("#f59e0b")),
        QColor(QStringLiteral("#ef4444"))};

    const int safeTier = std::clamp(tier, 0, 8);
    const auto base = colors.at(safeTier);
    QPixmap result(size, size);
    result.fill(Qt::transparent);

    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QPainterPath shield;
    shield.moveTo(size * 0.50, size * 0.05);
    shield.lineTo(size * 0.91, size * 0.23);
    shield.lineTo(size * 0.83, size * 0.68);
    shield.quadTo(size * 0.70, size * 0.88, size * 0.50, size * 0.96);
    shield.quadTo(size * 0.30, size * 0.88, size * 0.17, size * 0.68);
    shield.lineTo(size * 0.09, size * 0.23);
    shield.closeSubpath();

    QLinearGradient gradient(0, 0, size, size);
    gradient.setColorAt(0.0, base.lighter(135));
    gradient.setColorAt(0.52, base);
    gradient.setColorAt(1.0, base.darker(145));
    painter.setBrush(gradient);
    painter.setPen(QPen(QColor(255, 255, 255, 120), std::max(1, size / 24)));
    painter.drawPath(shield);

    painter.setBrush(QColor(2, 6, 23, 90));
    painter.setPen(QPen(QColor(255, 255, 255, 125), std::max(1, size / 30)));
    painter.drawEllipse(QRectF(size * 0.27, size * 0.24, size * 0.46, size * 0.46));

    painter.setPen(QColor(248, 250, 252));
    QFont tierFont = painter.font();
    tierFont.setBold(true);
    tierFont.setPixelSize(std::max(9, size / 3));
    painter.setFont(tierFont);
    painter.drawText(QRectF(0, size * 0.27, size, size * 0.34),
                     Qt::AlignCenter,
                     safeTier == 0 ? QStringLiteral("?") : QString::number(safeTier));

    if (star > 0)
    {
        painter.setBrush(QColor(255, 255, 255, 230));
        painter.setPen(Qt::NoPen);
        const int visibleStars = std::min(star, 5);
        const qreal gap = size * 0.105;
        const qreal start = size * 0.50 - (visibleStars - 1) * gap / 2.0;
        for (int index = 0; index < visibleStars; ++index)
            painter.drawEllipse(QPointF(start + index * gap, size * 0.80),
                                std::max<qreal>(1.2, size * 0.026),
                                std::max<qreal>(1.2, size * 0.026));
    }

    return result;
}
}

RankingView::RankingView(ServerClient &server, QWidget *parent)
    : QWidget(parent)
    , m_server(server)
{
    setObjectName(QStringLiteral("RankingPage"));
    buildUi();

    connect(&m_server, &ServerClient::rankingFinished,
            this, [this](quint64 requestId, bool ok, const QString &error,
                         const RankingPageData &page) {
                if (requestId != m_activeRequestId)
                    return;
                if (m_token.isEmpty() || m_sessionInvalid)
                    return;
                if (!ok)
                {
                    handleRankingError(error);
                    setBusy(false);
                    return;
                }

                m_entries = page.items;
                m_totalCount = page.totalCount;
                m_hasLoaded = true;
                m_countLabel->setText(m_totalCount == 0
                                           ? QStringLiteral("Sin jugadores clasificados")
                                           : QStringLiteral("%1 jugadores").arg(m_totalCount));
                setBusy(false);
                renderRanking();
            });
}

void RankingView::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 8, 8);
    root->setSpacing(10);

    auto *header = new QHBoxLayout;
    header->setSpacing(10);
    auto *back = new QPushButton(QStringLiteral("‹  CENTRO"), this);
    back->setObjectName(QStringLiteral("RankingBackButton"));
    back->setMinimumHeight(30);
    connect(back, &QPushButton::clicked, this, &RankingView::backRequested);
    header->addWidget(back, 0, Qt::AlignVCenter);

    auto *heading = new QVBoxLayout;
    heading->setSpacing(0);
    auto *title = new QLabel(QStringLiteral("RANKING"), this);
    title->setObjectName(QStringLiteral("RankingTitle"));
    auto *subtitle = new QLabel(QStringLiteral("Clasificación global por MMR"), this);
    subtitle->setObjectName(QStringLiteral("RankingSubtitle"));
    heading->addWidget(title);
    heading->addWidget(subtitle);
    header->addLayout(heading);
    header->addStretch();

    m_refreshButton = new QPushButton(QStringLiteral("ACTUALIZAR"), this);
    m_refreshButton->setObjectName(QStringLiteral("RankingRefreshButton"));
    m_refreshButton->setMinimumHeight(30);
    connect(m_refreshButton, &QPushButton::clicked, this, &RankingView::reload);
    header->addWidget(m_refreshButton);
    root->addLayout(header);

    auto *scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("RankingScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *body = new QWidget(scroll);
    body->setObjectName(QStringLiteral("RankingBody"));
    auto *bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(0, 0, 4, 12);
    bodyLayout->setSpacing(10);

    auto *hero = new QFrame(body);
    hero->setObjectName(QStringLiteral("RankingHeroCard"));
    addShadow(hero);
    auto *heroLayout = new QHBoxLayout(hero);
    heroLayout->setContentsMargins(18, 14, 18, 14);
    heroLayout->setSpacing(14);
    auto *heroCopy = new QVBoxLayout;
    heroCopy->setSpacing(3);
    auto *heroOverline = new QLabel(QStringLiteral("D2MAX · COMPETITIVO"), hero);
    heroOverline->setObjectName(QStringLiteral("RankingOverline"));
    auto *heroTitle = new QLabel(QStringLiteral("Sube en la clasificación."), hero);
    heroTitle->setObjectName(QStringLiteral("RankingHeroTitle"));
    auto *heroText = new QLabel(QStringLiteral("Consulta el MMR, la medalla y el rendimiento básico de cada jugador del servidor."), hero);
    heroText->setObjectName(QStringLiteral("RankingHeroText"));
    heroText->setWordWrap(true);
    heroCopy->addWidget(heroOverline);
    heroCopy->addWidget(heroTitle);
    heroCopy->addWidget(heroText);
    heroLayout->addLayout(heroCopy, 1);

    auto *heroBadge = new QFrame(hero);
    heroBadge->setObjectName(QStringLiteral("RankingHeroBadge"));
    auto *heroBadgeLayout = new QVBoxLayout(heroBadge);
    heroBadgeLayout->setContentsMargins(12, 10, 12, 10);
    auto *badgeTitle = new QLabel(QStringLiteral("MEDALLAS DOTA 2"), heroBadge);
    badgeTitle->setObjectName(QStringLiteral("RankingBadgeTitle"));
    auto *badgeText = new QLabel(QStringLiteral("Iconos locales por medalla"), heroBadge);
    badgeText->setObjectName(QStringLiteral("RankingMuted"));
    badgeText->setWordWrap(true);
    heroBadgeLayout->addWidget(badgeTitle);
    heroBadgeLayout->addWidget(badgeText);
    heroLayout->addWidget(heroBadge);
    bodyLayout->addWidget(hero);

    auto *toolbar = new QFrame(body);
    toolbar->setObjectName(QStringLiteral("RankingToolbar"));
    auto *toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(12, 8, 12, 8);
    toolbarLayout->setSpacing(8);
    auto *toolbarTitle = new QLabel(QStringLiteral("RANKING GLOBAL"), toolbar);
    toolbarTitle->setObjectName(QStringLiteral("RankingSectionTitle"));
    m_countLabel = new QLabel(QStringLiteral("Cargando…"), toolbar);
    m_countLabel->setObjectName(QStringLiteral("RankingMuted"));
    toolbarLayout->addWidget(toolbarTitle);
    toolbarLayout->addStretch();
    toolbarLayout->addWidget(m_countLabel);
    bodyLayout->addWidget(toolbar);

    m_stateLabel = new QLabel(body);
    m_stateLabel->setObjectName(QStringLiteral("RankingInfo"));
    m_stateLabel->setWordWrap(true);
    m_stateLabel->hide();
    bodyLayout->addWidget(m_stateLabel);

    m_listHost = new QWidget(body);
    m_listHost->setObjectName(QStringLiteral("RankingListHost"));
    m_listLayout = new QVBoxLayout(m_listHost);
    m_listLayout->setContentsMargins(0, 0, 0, 0);
    m_listLayout->setSpacing(6);
    bodyLayout->addWidget(m_listHost);
    bodyLayout->addStretch();

    scroll->setWidget(body);
    root->addWidget(scroll, 1);

    renderSessionGate(QStringLiteral("Ranking listo"),
                      QStringLiteral("Inicia sesión en el launcher para consultar la clasificación."));
}

void RankingView::setSessionToken(const QString &token)
{
    const auto normalized = token.trimmed();
    if (m_token == normalized)
        return;

    m_token = normalized;
    m_sessionInvalid = false;
    m_busy = false;
    m_entries.clear();
    m_totalCount = 0;
    m_hasLoaded = false;
    m_activeRequestId = 0;
    setBusy(false);
    if (m_token.isEmpty())
    {
        m_countLabel->setText(QStringLiteral("Sesión no iniciada"));
        renderSessionGate(QStringLiteral("Sesión requerida"),
                          QStringLiteral("Inicia sesión en el launcher para acceder al ranking."));
        return;
    }

    m_countLabel->setText(QStringLiteral("Actualizando…"));
    m_stateLabel->setObjectName(QStringLiteral("RankingInfo"));
    m_stateLabel->setText(QStringLiteral("Cargando clasificación…"));
    m_stateLabel->show();
    renderLoadingState();
}

void RankingView::reload()
{
    if (m_token.isEmpty())
    {
        setBusy(false);
        m_countLabel->setText(QStringLiteral("Sesión no iniciada"));
        renderSessionGate(QStringLiteral("Sesión requerida"),
                          QStringLiteral("Inicia sesión en el launcher para acceder al ranking."));
        return;
    }

    m_sessionInvalid = false;
    setBusy(true);
    m_stateLabel->setObjectName(QStringLiteral("RankingInfo"));
    m_stateLabel->setText(QStringLiteral("Actualizando clasificación…"));
    m_stateLabel->show();
    m_activeRequestId = m_server.fetchRanking(m_token);
}

void RankingView::renderRanking()
{
    clearLayout(m_listLayout);
    if (!m_hasLoaded)
    {
        renderLoadingState();
        return;
    }
    if (m_entries.isEmpty())
    {
        renderEmptyState();
        return;
    }

    for (const auto &entry : m_entries)
        m_listLayout->addWidget(createRankingRow(entry));
    m_listLayout->addStretch(1);
    m_stateLabel->hide();
}

void RankingView::renderLoadingState()
{
    auto *loading = new QFrame(m_listHost);
    loading->setObjectName(QStringLiteral("RankingLoadingState"));
    auto *layout = new QVBoxLayout(loading);
    layout->setContentsMargins(20, 24, 20, 24);
    layout->setSpacing(6);
    auto *title = new QLabel(QStringLiteral("Cargando ranking…"), loading);
    title->setObjectName(QStringLiteral("RankingEmptyTitle"));
    title->setAlignment(Qt::AlignCenter);
    auto *copy = new QLabel(QStringLiteral("Estamos consultando el MMR y las medallas del servidor."), loading);
    copy->setObjectName(QStringLiteral("RankingMuted"));
    copy->setAlignment(Qt::AlignCenter);
    copy->setWordWrap(true);
    layout->addWidget(title);
    layout->addWidget(copy);
    m_listLayout->addWidget(loading);
    m_listLayout->addStretch(1);
}

void RankingView::renderEmptyState()
{
    auto *empty = new QFrame(m_listHost);
    empty->setObjectName(QStringLiteral("RankingEmptyState"));
    auto *layout = new QVBoxLayout(empty);
    layout->setContentsMargins(20, 24, 20, 24);
    layout->setSpacing(6);
    auto *icon = new QLabel(QStringLiteral("✦"), empty);
    icon->setObjectName(QStringLiteral("RankingEmptyIcon"));
    icon->setAlignment(Qt::AlignCenter);
    auto *title = new QLabel(QStringLiteral("Aún no hay jugadores clasificados"), empty);
    title->setObjectName(QStringLiteral("RankingEmptyTitle"));
    title->setAlignment(Qt::AlignCenter);
    auto *copy = new QLabel(QStringLiteral("El ranking aparecerá cuando una cuenta complete una partida calibrada o reciba MMR."), empty);
    copy->setObjectName(QStringLiteral("RankingMuted"));
    copy->setWordWrap(true);
    copy->setAlignment(Qt::AlignCenter);
    layout->addWidget(icon);
    layout->addWidget(title);
    layout->addWidget(copy);
    m_listLayout->addWidget(empty);
    m_listLayout->addStretch(1);
    m_stateLabel->hide();
}

QWidget *RankingView::createRankingRow(const RankingEntryData &entry)
{
    auto *row = new QFrame(m_listHost);
    row->setObjectName(QStringLiteral("RankingRow"));
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(10);

    auto *position = new QLabel(QStringLiteral("#%1").arg(entry.position), row);
    position->setObjectName(QStringLiteral("RankingPosition"));
    position->setFixedWidth(38);
    position->setAlignment(Qt::AlignCenter);
    layout->addWidget(position);

    auto *icon = new QLabel(row);
    icon->setObjectName(QStringLiteral("RankingMedalIcon"));
    icon->setFixedSize(48, 48);
    icon->setAlignment(Qt::AlignCenter);
    icon->setPixmap(medalIcon(entry.rankTier, entry.rankStar, 44));
    layout->addWidget(icon);

    auto *identity = new QVBoxLayout;
    identity->setSpacing(2);
    auto *name = new QLabel(entry.personaName.isEmpty() ? entry.username : entry.personaName, row);
    name->setObjectName(QStringLiteral("RankingPlayerName"));
    name->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto *handle = new QLabel(entry.username == entry.personaName || entry.personaName.isEmpty()
                                   ? QStringLiteral("Cuenta D2MAX")
                                   : QStringLiteral("@%1").arg(entry.username), row);
    handle->setObjectName(QStringLiteral("RankingPlayerHandle"));
    auto *presence = new QLabel(entry.online ? QStringLiteral("● EN LÍNEA") : QStringLiteral("○ DESCONECTADO"), row);
    presence->setObjectName(entry.online
                                ? QStringLiteral("RankingOnline")
                                : QStringLiteral("RankingOffline"));
    identity->addWidget(name);
    identity->addWidget(handle);
    identity->addWidget(presence);
    layout->addLayout(identity, 1);

    auto *medal = new QVBoxLayout;
    medal->setSpacing(1);
    auto *medalLabel = new QLabel(medalName(entry.rankTier), row);
    medalLabel->setObjectName(QStringLiteral("RankingMedalName"));
    auto *starLabel = new QLabel(stars(entry.rankStar), row);
    starLabel->setObjectName(QStringLiteral("RankingStars"));
    medal->addWidget(medalLabel, 0, Qt::AlignRight);
    medal->addWidget(starLabel, 0, Qt::AlignRight);
    layout->addLayout(medal);

    auto *mmr = new QVBoxLayout;
    mmr->setSpacing(1);
    auto *mmrValue = new QLabel(QStringLiteral("%1").arg(entry.mmr), row);
    mmrValue->setObjectName(QStringLiteral("RankingMmr"));
    auto *mmrLabel = new QLabel(QStringLiteral("MMR"), row);
    mmrLabel->setObjectName(QStringLiteral("RankingMetricLabel"));
    mmr->addWidget(mmrValue, 0, Qt::AlignRight);
    mmr->addWidget(mmrLabel, 0, Qt::AlignRight);
    layout->addLayout(mmr);

    auto *stats = new QLabel(QStringLiteral("%1 partidas · %2 V · %3 D\n%4% WR")
                                 .arg(entry.games)
                                 .arg(entry.wins)
                                 .arg(entry.losses)
                                 .arg(entry.winRatePercent), row);
    stats->setObjectName(QStringLiteral("RankingStats"));
    stats->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    stats->setMinimumWidth(145);
    layout->addWidget(stats);

    return row;
}

void RankingView::renderSessionGate(const QString &title, const QString &message)
{
    clearLayout(m_listLayout);
    auto *gate = new QFrame(m_listHost);
    gate->setObjectName(QStringLiteral("RankingSessionGate"));
    auto *layout = new QVBoxLayout(gate);
    layout->setContentsMargins(20, 24, 20, 24);
    layout->setSpacing(6);
    auto *heading = new QLabel(title, gate);
    heading->setObjectName(QStringLiteral("RankingEmptyTitle"));
    heading->setAlignment(Qt::AlignCenter);
    auto *copy = new QLabel(message, gate);
    copy->setObjectName(QStringLiteral("RankingMuted"));
    copy->setWordWrap(true);
    copy->setAlignment(Qt::AlignCenter);
    auto *login = new QPushButton(QStringLiteral("INICIAR SESIÓN"), gate);
    login->setObjectName(QStringLiteral("RankingLoginButton"));
    login->setMinimumWidth(150);
    connect(login, &QPushButton::clicked, this, &RankingView::loginRequested);
    layout->addWidget(heading);
    layout->addWidget(copy);
    layout->addSpacing(4);
    layout->addWidget(login, 0, Qt::AlignCenter);
    m_listLayout->addWidget(gate);
    m_listLayout->addStretch(1);
    m_stateLabel->hide();
}

void RankingView::setBusy(bool busy)
{
    m_busy = busy;
    m_refreshButton->setEnabled(!busy && !m_token.isEmpty() && !m_sessionInvalid);
}

void RankingView::handleRankingError(const QString &error)
{
    if (error == QStringLiteral("SESSION_EXPIRED"))
    {
        m_sessionInvalid = true;
        m_busy = false;
        m_countLabel->setText(QStringLiteral("Sesión expirada"));
        renderSessionGate(QStringLiteral("Sesión expirada"),
                          QStringLiteral("Vuelve a iniciar sesión para consultar el ranking."));
        m_stateLabel->hide();
        if (m_token.isEmpty())
            emit loginRequested();
        return;
    }

    m_stateLabel->setObjectName(QStringLiteral("RankingError"));
    m_stateLabel->setText(error.isEmpty()
                              ? QStringLiteral("No se pudo actualizar el ranking.")
                              : error);
    m_stateLabel->show();
    if (m_stateLabel->style())
    {
        m_stateLabel->style()->unpolish(m_stateLabel);
        m_stateLabel->style()->polish(m_stateLabel);
    }
}

void RankingView::clearLayout(QLayout *layout)
{
    while (auto *item = layout->takeAt(0))
    {
        if (auto *widget = item->widget())
            widget->deleteLater();
        if (auto *childLayout = item->layout())
        {
            clearLayout(childLayout);
            delete childLayout;
        }
        delete item;
    }
}
