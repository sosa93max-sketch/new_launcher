#include "StoreView.h"

#include <QComboBox>
#include <QColor>
#include <QFrame>
#include <QGridLayout>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QStringList>
#include <QVBoxLayout>

#include <algorithm>

namespace
{
QString dollars(qint64 value)
{
    return QStringLiteral("$%1").arg(value);
}

QString marketDollars(qint64 cents)
{
    const auto whole = cents / 100;
    const auto minor = cents % 100;
    return QStringLiteral("$%1.%2")
        .arg(whole)
        .arg(minor, 2, 10, QLatin1Char('0'));
}

QString productType(int type)
{
    switch (type)
    {
    case 1:
        return QStringLiteral("SET");
    case 2:
        return QStringLiteral("DOTA PLUS");
    default:
        return QStringLiteral("COSMÉTICO");
    }
}

bool hasMarketPrice(const StoreCatalogItemData &item)
{
    return item.marketPriceStatus.compare(QStringLiteral("matched"), Qt::CaseInsensitive) == 0
        && (item.marketLowestPriceCents > 0 || item.marketMedianPriceCents > 0);
}

qint64 marketCents(const StoreCatalogItemData &item)
{
    return item.marketLowestPriceCents > 0
        ? item.marketLowestPriceCents
        : item.marketMedianPriceCents;
}

void addShadow(QWidget *widget)
{
    auto *effect = new QGraphicsDropShadowEffect(widget);
    effect->setBlurRadius(16);
    effect->setOffset(0, 4);
    effect->setColor(QColor(0, 0, 0, 60));
    widget->setGraphicsEffect(effect);
}
}

StoreView::StoreView(ServerClient &server, QWidget *parent)
    : QWidget(parent)
    , m_server(server)
{
    setObjectName(QStringLiteral("StorePage"));
    buildUi();

    connect(&m_server, &ServerClient::storeCatalogFinished,
            this, [this](bool ok, const QString &error, const StoreCatalogPageData &page) {
                if (m_token.isEmpty() || m_sessionInvalid)
                    return;
                if (!ok)
                {
                    handleStoreError(error);
                    setBusy(false);
                    return;
                }

                m_page = page.page;
                m_totalCount = page.totalCount;
                m_totalPages = std::max(1, (m_totalCount + m_pageSize - 1) / m_pageSize);
                populateFilters(page);
                m_catalog = page.items;
                m_catalogMeta->setText(QStringLiteral("%1 artículos · pág. %2 de %3")
                                           .arg(m_totalCount)
                                           .arg(m_page)
                                           .arg(m_totalPages));
                m_pageLabel->setText(QStringLiteral("%1 / %2").arg(m_page).arg(m_totalPages));
                m_previousPage->setEnabled(m_page > 1 && !m_busy);
                m_nextPage->setEnabled(m_page < m_totalPages && !m_busy);
                setBusy(false);
            });

    connect(&m_server, &ServerClient::storeWalletFinished,
            this, [this](bool ok, const QString &error, const StoreWalletData &wallet) {
                if (m_token.isEmpty() || m_sessionInvalid)
                    return;
                if (!ok)
                {
                    handleStoreError(error);
                    return;
                }
                m_walletValue->setText(dollars(wallet.availableDollars));
                m_walletMeta->setText(wallet.reservedDollars > 0
                                          ? QStringLiteral("%1 reservados").arg(dollars(wallet.reservedDollars))
                                          : QStringLiteral("Saldo disponible"));
            });

    connect(&m_server, &ServerClient::storeInventoryFinished,
            this, [this](bool ok, const QString &error, const QVector<StoreInventoryItemData> &items) {
                if (m_token.isEmpty() || m_sessionInvalid)
                    return;
                if (!ok)
                {
                    handleStoreError(error);
                    return;
                }
                renderInventory(items);
            });

    connect(&m_server, &ServerClient::storeTransactionsFinished,
            this, [this](bool ok, const QString &error, const QVector<StoreTransactionData> &items) {
                if (m_token.isEmpty() || m_sessionInvalid)
                    return;
                if (!ok)
                {
                    handleStoreError(error);
                    return;
                }
                renderTransactions(items);
            });

    connect(&m_server, &ServerClient::storePurchaseFinished,
            this, [this](bool ok, const QString &error, const StorePurchaseData &purchase) {
                if (m_token.isEmpty() || m_sessionInvalid)
                    return;
                m_busy = false;
                if (!ok)
                {
                    handleStoreError(error);
                    setBusy(false);
                    return;
                }
                m_stateLabel->setObjectName(QStringLiteral("StoreSuccess"));
                m_stateLabel->setText(QStringLiteral("Compra completada · artículo en tu inventario."));
                m_stateLabel->style()->unpolish(m_stateLabel);
                m_stateLabel->style()->polish(m_stateLabel);
                m_walletValue->setText(dollars(purchase.wallet.availableDollars));
                renderInventory(purchase.items);
                loadCatalog(m_page);
            });

    connect(&m_searchTimer, &QTimer::timeout, this, [this]() { loadCatalog(1); });
    m_searchTimer.setSingleShot(true);
}

void StoreView::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 8, 8);
    root->setSpacing(10);

    auto *header = new QHBoxLayout;
    header->setSpacing(10);
    auto *back = new QPushButton(QStringLiteral("‹  CENTRO"), this);
    back->setObjectName(QStringLiteral("StoreBackButton"));
    back->setMinimumHeight(30);
    connect(back, &QPushButton::clicked, this, &StoreView::backRequested);
    header->addWidget(back, 0, Qt::AlignVCenter);

    auto *heading = new QVBoxLayout;
    heading->setSpacing(0);
    auto *title = new QLabel(QStringLiteral("TIENDA"), this);
    title->setObjectName(QStringLiteral("StoreTitle"));
    auto *subtitle = new QLabel(QStringLiteral("Catálogo, saldo e inventario"), this);
    subtitle->setObjectName(QStringLiteral("StoreSubtitle"));
    heading->addWidget(title);
    heading->addWidget(subtitle);
    header->addLayout(heading);
    header->addStretch();

    auto *wallet = new QFrame(this);
    wallet->setObjectName(QStringLiteral("StoreWalletCard"));
    wallet->setMinimumWidth(180);
    addShadow(wallet);
    auto *walletLayout = new QHBoxLayout(wallet);
    walletLayout->setContentsMargins(12, 7, 12, 7);
    walletLayout->setSpacing(8);
    auto *walletIcon = new QLabel(QStringLiteral("$"), wallet);
    walletIcon->setObjectName(QStringLiteral("StoreWalletIcon"));
    walletIcon->setFixedSize(28, 28);
    walletIcon->setAlignment(Qt::AlignCenter);
    auto *walletCopy = new QVBoxLayout;
    walletCopy->setSpacing(0);
    auto *walletLabel = new QLabel(QStringLiteral("SALDO"), wallet);
    walletLabel->setObjectName(QStringLiteral("StoreOverline"));
    m_walletValue = new QLabel(QStringLiteral("—"), wallet);
    m_walletValue->setObjectName(QStringLiteral("StoreWalletValue"));
    m_walletMeta = new QLabel(QStringLiteral("Cargando…"), wallet);
    m_walletMeta->setObjectName(QStringLiteral("StoreMuted"));
    walletCopy->addWidget(walletLabel);
    walletCopy->addWidget(m_walletValue);
    walletCopy->addWidget(m_walletMeta);
    walletLayout->addWidget(walletIcon);
    walletLayout->addLayout(walletCopy, 1);
    header->addWidget(wallet);
    root->addLayout(header);

    auto *scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("StoreScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *body = new QWidget(scroll);
    body->setObjectName(QStringLiteral("StoreBody"));
    auto *bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(0, 0, 4, 12);
    bodyLayout->setSpacing(10);

    auto *hero = new QFrame(body);
    hero->setObjectName(QStringLiteral("StoreHeroCard"));
    addShadow(hero);
    auto *heroLayout = new QHBoxLayout(hero);
    heroLayout->setContentsMargins(18, 14, 18, 14);
    heroLayout->setSpacing(14);
    auto *heroCopy = new QVBoxLayout;
    heroCopy->setSpacing(3);
    auto *heroOverline = new QLabel(QStringLiteral("LOADOUT · D2MAX"), hero);
    heroOverline->setObjectName(QStringLiteral("StoreOverline"));
    auto *heroTitle = new QLabel(QStringLiteral("Equipa tu próxima partida."), hero);
    heroTitle->setObjectName(QStringLiteral("StoreHeroTitle"));
    auto *heroText = new QLabel(QStringLiteral("Compra con tu saldo local y recibe el artículo en el inventario del cliente."), hero);
    heroText->setObjectName(QStringLiteral("StoreHeroText"));
    heroText->setWordWrap(true);
    heroCopy->addWidget(heroOverline);
    heroCopy->addWidget(heroTitle);
    heroCopy->addWidget(heroText);
    heroLayout->addLayout(heroCopy, 1);
    auto *heroBadge = new QFrame(hero);
    heroBadge->setObjectName(QStringLiteral("StoreHeroBadge"));
    auto *heroBadgeLayout = new QVBoxLayout(heroBadge);
    heroBadgeLayout->setContentsMargins(12, 10, 12, 10);
    auto *heroBadgeTitle = new QLabel(QStringLiteral("SIN NAVEGADOR"), heroBadge);
    heroBadgeTitle->setObjectName(QStringLiteral("StoreBadgeTitle"));
    auto *heroBadgeText = new QLabel(QStringLiteral("Sesión compartida con el launcher"), heroBadge);
    heroBadgeText->setObjectName(QStringLiteral("StoreMuted"));
    heroBadgeText->setWordWrap(true);
    heroBadgeLayout->addWidget(heroBadgeTitle);
    heroBadgeLayout->addWidget(heroBadgeText);
    heroLayout->addWidget(heroBadge);
    bodyLayout->addWidget(hero);

    auto *filters = new QFrame(body);
    filters->setObjectName(QStringLiteral("StoreFilterCard"));
    m_filterCard = filters;
    auto *filterLayout = new QHBoxLayout(filters);
    filterLayout->setContentsMargins(10, 8, 10, 8);
    filterLayout->setSpacing(6);
    m_search = new QLineEdit(filters);
    m_search->setObjectName(QStringLiteral("StoreSearch"));
    m_search->setPlaceholderText(QStringLiteral("Buscar artículos, héroes…"));
    m_category = new QComboBox(filters);
    m_category->setObjectName(QStringLiteral("StoreFilter"));
    m_category->addItem(QStringLiteral("Todas las categorías"));
    m_hero = new QComboBox(filters);
    m_hero->setObjectName(QStringLiteral("StoreFilter"));
    m_hero->addItem(QStringLiteral("Todos los héroes"));
    m_type = new QComboBox(filters);
    m_type->setObjectName(QStringLiteral("StoreFilter"));
    m_type->addItem(QStringLiteral("Todo"), -1);
    m_type->addItem(QStringLiteral("Cosméticos"), 0);
    m_type->addItem(QStringLiteral("Sets"), 1);
    m_type->addItem(QStringLiteral("Dota Plus"), 2);
    m_refreshButton = new QPushButton(QStringLiteral("ACTUALIZAR"), filters);
    m_refreshButton->setObjectName(QStringLiteral("StoreSecondaryButton"));
    filterLayout->addWidget(m_search, 2);
    filterLayout->addWidget(m_category, 1);
    filterLayout->addWidget(m_hero, 1);
    filterLayout->addWidget(m_type, 1);
    filterLayout->addWidget(m_refreshButton);
    bodyLayout->addWidget(filters);

    m_catalogHeaderHost = new QWidget(body);
    auto *catalogHeader = new QHBoxLayout(m_catalogHeaderHost);
    catalogHeader->setContentsMargins(0, 0, 0, 0);
    auto *catalogTitle = new QLabel(QStringLiteral("CATÁLOGO"), body);
    catalogTitle->setObjectName(QStringLiteral("StoreSectionTitle"));
    m_catalogMeta = new QLabel(QStringLiteral("Cargando…"), body);
    m_catalogMeta->setObjectName(QStringLiteral("StoreMuted"));
    catalogHeader->addWidget(catalogTitle);
    catalogHeader->addStretch();
    catalogHeader->addWidget(m_catalogMeta);
    bodyLayout->addWidget(m_catalogHeaderHost);

    m_stateLabel = new QLabel(body);
    m_stateLabel->setObjectName(QStringLiteral("StoreInfo"));
    m_stateLabel->setWordWrap(true);
    m_stateLabel->hide();
    bodyLayout->addWidget(m_stateLabel);

    m_catalogHost = new QWidget(body);
    m_catalogHost->setObjectName(QStringLiteral("StoreCatalogHost"));
    m_catalogGrid = new QGridLayout(m_catalogHost);
    m_catalogGrid->setContentsMargins(0, 0, 0, 0);
    m_catalogGrid->setHorizontalSpacing(8);
    m_catalogGrid->setVerticalSpacing(8);
    bodyLayout->addWidget(m_catalogHost);

    m_pagerHost = new QWidget(body);
    auto *pager = new QHBoxLayout(m_pagerHost);
    pager->setContentsMargins(0, 0, 0, 0);
    m_previousPage = new QPushButton(QStringLiteral("‹"), body);
    m_previousPage->setObjectName(QStringLiteral("StorePagerButton"));
    m_previousPage->setFixedSize(30, 28);
    m_pageLabel = new QLabel(QStringLiteral("1 / 1"), body);
    m_pageLabel->setObjectName(QStringLiteral("StorePageLabel"));
    m_nextPage = new QPushButton(QStringLiteral("›"), body);
    m_nextPage->setObjectName(QStringLiteral("StorePagerButton"));
    m_nextPage->setFixedSize(30, 28);
    pager->addStretch();
    pager->addWidget(m_previousPage);
    pager->addWidget(m_pageLabel);
    pager->addWidget(m_nextPage);
    bodyLayout->addWidget(m_pagerHost);

    m_lowerHost = new QWidget(body);
    auto *lower = new QHBoxLayout(m_lowerHost);
    lower->setSpacing(8);
    auto makePanel = [&](const QString &title, QVBoxLayout *&layout) {
        auto *panel = new QFrame(body);
        panel->setObjectName(QStringLiteral("StorePanel"));
        auto *panelLayout = new QVBoxLayout(panel);
        panelLayout->setContentsMargins(12, 10, 12, 10);
        panelLayout->setSpacing(6);
        auto *label = new QLabel(title, panel);
        label->setObjectName(QStringLiteral("StoreSectionTitle"));
        panelLayout->addWidget(label);
        layout = new QVBoxLayout;
        layout->setSpacing(3);
        panelLayout->addLayout(layout);
        return panel;
    };
    lower->addWidget(makePanel(QStringLiteral("INVENTARIO"), m_inventoryLayout), 1);
    lower->addWidget(makePanel(QStringLiteral("ACTIVIDAD"), m_transactionsLayout), 1);
    bodyLayout->addWidget(m_lowerHost);

    scroll->setWidget(body);
    root->addWidget(scroll, 1);

    connect(m_refreshButton, &QPushButton::clicked, this, [this]() { reload(); });
    connect(m_previousPage, &QPushButton::clicked, this, [this]() { loadCatalog(m_page - 1); });
    connect(m_nextPage, &QPushButton::clicked, this, [this]() { loadCatalog(m_page + 1); });
    connect(m_search, &QLineEdit::textChanged, this, [this]() { m_searchTimer.start(300); });
    connect(m_category, &QComboBox::currentTextChanged, this, [this]() { loadCatalog(1); });
    connect(m_hero, &QComboBox::currentTextChanged, this, [this]() { loadCatalog(1); });
    connect(m_type, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int) { loadCatalog(1); });

    renderSessionGate(QStringLiteral("Tienda lista"),
                      QStringLiteral("Inicia sesión en el launcher para cargar tu catálogo y saldo."));
}

void StoreView::setSessionToken(const QString &token)
{
    const auto normalized = token.trimmed();
    if (m_token == normalized)
        return;

    m_token = normalized;
    m_sessionInvalid = false;
    m_busy = false;
    m_catalog.clear();
    m_page = 1;
    m_totalCount = 0;
    m_totalPages = 1;
    if (m_token.isEmpty())
    {
        m_walletValue->setText(QStringLiteral("—"));
        m_walletMeta->setText(QStringLiteral("Inicia sesión para ver tu saldo"));
        renderSessionGate(QStringLiteral("Sesión requerida"),
                          QStringLiteral("Inicia sesión en el launcher para acceder a la tienda."));
        return;
    }

    m_walletValue->setText(QStringLiteral("—"));
    m_walletMeta->setText(QStringLiteral("Cargando saldo…"));
    m_stateLabel->hide();
    renderCatalog();
}

void StoreView::reload()
{
    if (m_token.isEmpty())
    {
        m_walletValue->setText(QStringLiteral("—"));
        m_walletMeta->setText(QStringLiteral("Inicia sesión para ver tu saldo"));
        renderSessionGate(QStringLiteral("Sesión requerida"),
                          QStringLiteral("Inicia sesión en el launcher para acceder a la tienda."));
        return;
    }

    m_sessionInvalid = false;
    setBusy(true);
    m_stateLabel->setObjectName(QStringLiteral("StoreInfo"));
    m_stateLabel->setText(QStringLiteral("Actualizando catálogo, saldo e inventario…"));
    m_stateLabel->show();
    m_server.fetchStoreCatalog(m_token, m_page, m_pageSize,
                               m_search->text(),
                               m_category->currentData().toString(),
                               m_hero->currentData().toString(),
                               m_type->currentData().isValid() ? m_type->currentData().toInt() : -1);
    m_server.fetchStoreWallet(m_token);
    m_server.fetchStoreInventory(m_token);
    m_server.fetchStoreTransactions(m_token);
}

void StoreView::loadCatalog(int page)
{
    if (m_token.isEmpty() || m_busy)
        return;
    m_page = std::max(1, page);
    setBusy(true);
    m_server.fetchStoreCatalog(m_token, m_page, m_pageSize,
                               m_search->text(),
                               m_category->currentData().toString(),
                               m_hero->currentData().toString(),
                               m_type->currentData().isValid() ? m_type->currentData().toInt() : -1);
    m_stateLabel->setObjectName(QStringLiteral("StoreInfo"));
    m_stateLabel->setText(QStringLiteral("Actualizando catálogo…"));
    m_stateLabel->show();
}

void StoreView::populateFilters(const StoreCatalogPageData &page)
{
    const auto category = m_category->currentText();
    const auto hero = m_hero->currentText();
    {
        const QSignalBlocker blocker(m_category);
        m_category->clear();
        m_category->addItem(QStringLiteral("Todas las categorías"));
        for (const auto &value : page.categories)
            m_category->addItem(value, value);
        const auto index = m_category->findText(category);
        if (index >= 0)
            m_category->setCurrentIndex(index);
    }
    {
        const QSignalBlocker blocker(m_hero);
        m_hero->clear();
        m_hero->addItem(QStringLiteral("Todos los héroes"));
        for (const auto &value : page.heroes)
            m_hero->addItem(value, value);
        const auto index = m_hero->findText(hero);
        if (index >= 0)
            m_hero->setCurrentIndex(index);
    }
}

void StoreView::renderCatalog()
{
    if (m_filterCard)
        m_filterCard->show();
    if (m_catalogHeaderHost)
        m_catalogHeaderHost->show();
    if (m_pagerHost)
        m_pagerHost->show();
    if (m_lowerHost)
        m_lowerHost->show();
    m_catalogHost->show();
    clearLayout(m_catalogGrid);
    if (m_catalog.isEmpty())
    {
        auto *empty = new QFrame(m_catalogHost);
        empty->setObjectName(QStringLiteral("StoreEmptyState"));
        auto *layout = new QVBoxLayout(empty);
        layout->setContentsMargins(18, 20, 18, 20);
        auto *title = new QLabel(QStringLiteral("No hay artículos"), empty);
        title->setObjectName(QStringLiteral("StoreEmptyTitle"));
        auto *text = new QLabel(QStringLiteral("Prueba otra búsqueda o vuelve a actualizar."), empty);
        text->setObjectName(QStringLiteral("StoreMuted"));
        text->setAlignment(Qt::AlignCenter);
        layout->addWidget(title);
        layout->addWidget(text);
        m_catalogGrid->addWidget(empty, 0, 0, 1, 3);
        m_stateLabel->hide();
        return;
    }

    const int columns = m_catalogHost->width() >= 800 ? 3 : (m_catalogHost->width() >= 520 ? 2 : 1);
    for (int index = 0; index < m_catalog.size(); ++index)
        m_catalogGrid->addWidget(createProductCard(m_catalog.at(index)), index / columns, index % columns);
    for (int column = 0; column < columns; ++column)
        m_catalogGrid->setColumnStretch(column, 1);
    m_stateLabel->hide();
}

QWidget *StoreView::createProductCard(const StoreCatalogItemData &item)
{
    auto *card = new QFrame(m_catalogHost);
    card->setObjectName(QStringLiteral("StoreProductCard"));
    card->setMinimumHeight(180);
    addShadow(card);
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(5);

    auto *top = new QHBoxLayout;
    auto *type = new QLabel(productType(item.productType), card);
    type->setObjectName(QStringLiteral("StoreProductBadge"));
    auto *owned = new QLabel(item.ownedQuantity > 0
                                 ? QStringLiteral("ACTIVADO ×%1").arg(item.ownedQuantity)
                                 : QStringLiteral("NUEVO"), card);
    owned->setObjectName(QStringLiteral("StoreOwnedBadge"));
    top->addWidget(type);
    top->addStretch();
    top->addWidget(owned);
    layout->addLayout(top);

    auto *name = new QLabel(item.name.isEmpty() ? QStringLiteral("Artículo Dota 2") : item.name, card);
    name->setObjectName(QStringLiteral("StoreProductName"));
    name->setWordWrap(true);
    name->setMinimumHeight(32);
    layout->addWidget(name);
    auto *description = new QLabel(item.description.isEmpty()
                                       ? QStringLiteral("Artículo cosmético para tu inventario.")
                                       : item.description,
                                   card);
    description->setObjectName(QStringLiteral("StoreProductDescription"));
    description->setWordWrap(true);
    description->setMaximumHeight(32);
    layout->addWidget(description);

    if (!item.heroes.isEmpty())
    {
        QStringList heroNames;
        for (qsizetype index = 0; index < std::min<qsizetype>(2, item.heroes.size()); ++index)
            heroNames.append(item.heroes.at(index));
        auto *hero = new QLabel(heroNames.join(QStringLiteral("  ·  ")), card);
        hero->setObjectName(QStringLiteral("StoreProductHero"));
        layout->addWidget(hero);
    }
    else
    {
        layout->addSpacing(12);
    }

    auto *bottom = new QHBoxLayout;
    bottom->setSpacing(6);
    auto *priceBox = new QVBoxLayout;
    priceBox->setSpacing(0);
    const auto market = hasMarketPrice(item);
    const auto cents = marketCents(item);
    auto *price = new QLabel(market ? marketDollars(cents) : dollars(item.priceDollars), card);
    price->setObjectName(QStringLiteral("StoreProductPrice"));
    auto *priceMeta = new QLabel(market
                                     ? QStringLiteral("Steam mínimo · saldo %1").arg(dollars(item.priceDollars))
                                     : item.priceDollars > 0
                                         ? QStringLiteral("Precio local")
                                         : QStringLiteral("Precio pendiente"),
                                 card);
    priceMeta->setObjectName(QStringLiteral("StoreProductPriceMeta"));
    priceBox->addWidget(price);
    priceBox->addWidget(priceMeta);
    bottom->addLayout(priceBox, 1);

    auto *buyButton = new QPushButton(item.ownedQuantity > 0
                                          ? QStringLiteral("COMPRAR OTRO")
                                          : QStringLiteral("COMPRAR"),
                                      card);
    buyButton->setObjectName(QStringLiteral("StoreBuyButton"));
    buyButton->setEnabled(!m_busy && item.priceDollars > 0);
    connect(buyButton, &QPushButton::clicked, this, [this, productId = item.productId]() { buy(productId); });
    bottom->addWidget(buyButton);
    layout->addStretch();
    layout->addLayout(bottom);
    return card;
}

void StoreView::renderInventory(const QVector<StoreInventoryItemData> &items)
{
    clearLayout(m_inventoryLayout);
    if (items.isEmpty())
    {
        auto *empty = new QLabel(QStringLiteral("Sin artículos activados."), this);
        empty->setObjectName(QStringLiteral("StoreMuted"));
        m_inventoryLayout->addWidget(empty);
        return;
    }

    const auto count = std::min<qsizetype>(6, items.size());
    for (qsizetype index = 0; index < count; ++index)
    {
        const auto &item = items.at(index);
        auto *row = new QFrame(this);
        row->setObjectName(QStringLiteral("StoreListRow"));
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(8, 5, 8, 5);
        auto *name = new QLabel(QStringLiteral("Definición %1").arg(item.defIndex), row);
        name->setObjectName(QStringLiteral("StoreListPrimary"));
        auto *quantity = new QLabel(QStringLiteral("×%1").arg(item.quantity), row);
        quantity->setObjectName(QStringLiteral("StoreListAccent"));
        rowLayout->addWidget(name);
        rowLayout->addStretch();
        rowLayout->addWidget(quantity);
        m_inventoryLayout->addWidget(row);
    }
}

void StoreView::renderTransactions(const QVector<StoreTransactionData> &items)
{
    clearLayout(m_transactionsLayout);
    if (items.isEmpty())
    {
        auto *empty = new QLabel(QStringLiteral("Sin movimientos."), this);
        empty->setObjectName(QStringLiteral("StoreMuted"));
        m_transactionsLayout->addWidget(empty);
        return;
    }

    const auto count = std::min<qsizetype>(6, items.size());
    for (qsizetype index = 0; index < count; ++index)
    {
        const auto &item = items.at(index);
        auto *row = new QFrame(this);
        row->setObjectName(QStringLiteral("StoreListRow"));
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(8, 5, 8, 5);
        auto *name = new QLabel(item.reference.isEmpty() ? QStringLiteral("Movimiento") : item.reference, row);
        name->setObjectName(QStringLiteral("StoreListPrimary"));
        name->setWordWrap(true);
        auto *amount = new QLabel(QStringLiteral("%1").arg(dollars(item.amountDollars)), row);
        amount->setObjectName(QStringLiteral("StoreListAccent"));
        rowLayout->addWidget(name, 1);
        rowLayout->addWidget(amount);
        m_transactionsLayout->addWidget(row);
    }
}

void StoreView::renderSessionGate(const QString &title, const QString &message)
{
    if (m_filterCard)
        m_filterCard->hide();
    if (m_catalogHeaderHost)
        m_catalogHeaderHost->hide();
    if (m_pagerHost)
        m_pagerHost->hide();
    if (m_lowerHost)
        m_lowerHost->hide();
    m_catalogHost->show();
    clearLayout(m_catalogGrid);
    auto *gate = new QFrame(m_catalogHost);
    gate->setObjectName(QStringLiteral("StoreSessionGate"));
    auto *layout = new QVBoxLayout(gate);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(6);
    auto *heading = new QLabel(title, gate);
    heading->setObjectName(QStringLiteral("StoreEmptyTitle"));
    heading->setAlignment(Qt::AlignCenter);
    auto *copy = new QLabel(message, gate);
    copy->setObjectName(QStringLiteral("StoreMuted"));
    copy->setAlignment(Qt::AlignCenter);
    copy->setWordWrap(true);
    auto *login = new QPushButton(QStringLiteral("INICIAR SESIÓN"), gate);
    login->setObjectName(QStringLiteral("StoreBuyButton"));
    login->setMinimumWidth(150);
    connect(login, &QPushButton::clicked, this, &StoreView::loginRequested);
    layout->addWidget(heading);
    layout->addWidget(copy);
    layout->addSpacing(4);
    layout->addWidget(login, 0, Qt::AlignCenter);
    m_catalogGrid->addWidget(gate, 0, 0, 1, 3);
    m_catalogMeta->setText(QStringLiteral("Tienda offline"));
}

void StoreView::setBusy(bool busy)
{
    m_busy = busy;
    m_refreshButton->setEnabled(!busy);
    m_search->setEnabled(!busy);
    m_category->setEnabled(!busy);
    m_hero->setEnabled(!busy);
    m_type->setEnabled(!busy);
    m_previousPage->setEnabled(!busy && m_page > 1);
    m_nextPage->setEnabled(!busy && m_page < m_totalPages);
    if (!m_sessionInvalid && !m_token.isEmpty())
        renderCatalog();
}

void StoreView::clearLayout(QLayout *layout)
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

void StoreView::buy(quint32 productId)
{
    if (m_busy || m_token.isEmpty() || productId == 0)
        return;

    m_busy = true;
    setBusy(true);
    m_stateLabel->setObjectName(QStringLiteral("StoreInfo"));
    m_stateLabel->setText(QStringLiteral("Procesando compra…"));
    m_stateLabel->show();
    m_server.purchaseStoreItem(m_token, productId, 1);
}

void StoreView::handleStoreError(const QString &error)
{
    if (error == QStringLiteral("SESSION_EXPIRED"))
    {
        m_sessionInvalid = true;
        m_busy = false;
        m_walletValue->setText(QStringLiteral("—"));
        m_walletMeta->setText(QStringLiteral("Vuelve a iniciar sesión"));
        renderSessionGate(QStringLiteral("Sesión expirada"),
                          QStringLiteral("Vuelve a iniciar sesión para continuar usando la tienda."));
        m_stateLabel->hide();
        if (m_token.isEmpty())
            emit loginRequested();
        return;
    }

    m_stateLabel->setObjectName(QStringLiteral("StoreError"));
    m_stateLabel->setText(error.isEmpty() ? QStringLiteral("No se pudo actualizar la tienda.") : error);
    m_stateLabel->show();
    m_stateLabel->style()->unpolish(m_stateLabel);
    m_stateLabel->style()->polish(m_stateLabel);
}

void StoreView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (!m_reflowPending)
    {
        m_reflowPending = true;
        QTimer::singleShot(0, this, [this]() {
            m_reflowPending = false;
            if (!m_token.isEmpty() && !m_sessionInvalid)
                renderCatalog();
        });
    }
}
