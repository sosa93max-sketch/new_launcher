#include "ServerClient.h"

#include "../util/Log.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QEventLoop>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

static const QLatin1String KeyUsername("Username");
static const QLatin1String KeyPassword("Password");
static const QLatin1String KeyToken("Token");
static const QLatin1String KeySteamId("SteamId");
static const QLatin1String KeyAccountId("AccountId");
static const QLatin1String KeyPersonaName("PersonaName");
static const QLatin1String KeyPlayerLevel("PlayerLevel");
static const QLatin1String KeyVersion("Version");
static const QLatin1String KeyMmr("Mmr");
static const QLatin1String KeyRankTier("RankTier");
static const QLatin1String KeyRankStar("RankStar");

namespace
{
/// JSON numbers above 2^53 lose precision when Qt parses them as double, and a
/// Steam id (~7.6e16) is far above that. Read the exact digits from the raw
/// payload instead, so every account keeps its own id.
quint64 readBigInt(const QByteArray &raw, const QByteArray &key)
{
    const QRegularExpression pattern(
        QStringLiteral("\"%1\"\\s*:\\s*(\\d{1,20})").arg(QString::fromLatin1(key)));
    const auto match = pattern.match(QString::fromUtf8(raw));
    return match.hasMatch() ? match.captured(1).toULongLong() : 0;
}

qint64 integerValue(const QJsonObject &object, const char *key)
{
    return static_cast<qint64>(object.value(QString::fromLatin1(key)).toDouble(0));
}

QVector<QString> stringList(const QJsonValue &value)
{
    QVector<QString> values;
    for (const auto &entry : value.toArray())
    {
        const auto text = entry.toString().trimmed();
        if (!text.isEmpty())
            values.append(text);
    }
    return values;
}

StoreCatalogItemData catalogItem(const QJsonObject &object)
{
    StoreCatalogItemData item;
    item.productId = static_cast<quint32>(integerValue(object, "ProductId"));
    item.defIndex = static_cast<quint32>(integerValue(object, "DefIndex"));
    item.name = object.value(QStringLiteral("Name")).toString();
    item.productType = object.value(QStringLiteral("ProductType")).toInt(0);
    item.priceDollars = integerValue(object, "PriceDollars");
    item.category = object.value(QStringLiteral("Category")).toString();
    item.description = object.value(QStringLiteral("Description")).toString();
    item.active = object.value(QStringLiteral("Active")).toBool(false);
    item.ownedQuantity = static_cast<quint32>(integerValue(object, "OwnedQuantity"));
    item.marketLowestPriceCents = integerValue(object, "MarketLowestPriceCents");
    item.marketMedianPriceCents = integerValue(object, "MarketMedianPriceCents");
    item.marketPriceStatus = object.value(QStringLiteral("MarketPriceStatus")).toString();
    item.heroes = stringList(object.value(QStringLiteral("Heroes")));
    return item;
}

StoreWalletData walletData(const QJsonObject &object)
{
    StoreWalletData wallet;
    wallet.balanceDollars = integerValue(object, "BalanceDollars");
    wallet.reservedDollars = integerValue(object, "ReservedDollars");
    wallet.availableDollars = integerValue(object, "AvailableDollars");
    return wallet;
}

StoreInventoryItemData inventoryItem(const QJsonObject &object)
{
    StoreInventoryItemData item;
    item.itemId = static_cast<quint64>(integerValue(object, "ItemId"));
    item.defIndex = static_cast<quint32>(integerValue(object, "DefIndex"));
    item.quantity = static_cast<quint32>(integerValue(object, "Quantity"));
    return item;
}

StoreCatalogPageData catalogPage(const QJsonObject &object)
{
    StoreCatalogPageData page;
    for (const auto &value : object.value(QStringLiteral("Items")).toArray())
        page.items.append(catalogItem(value.toObject()));
    page.page = object.value(QStringLiteral("Page")).toInt(1);
    page.pageSize = object.value(QStringLiteral("PageSize")).toInt(24);
    page.totalCount = object.value(QStringLiteral("TotalCount")).toInt(0);
    page.activeCount = object.value(QStringLiteral("ActiveCount")).toInt(0);
    page.categories = stringList(object.value(QStringLiteral("Categories")));
    page.heroes = stringList(object.value(QStringLiteral("Heroes")));
    return page;
}

QVector<StoreInventoryItemData> inventoryList(const QJsonObject &object)
{
    QVector<StoreInventoryItemData> items;
    for (const auto &value : object.value(QStringLiteral("Items")).toArray())
        items.append(inventoryItem(value.toObject()));
    return items;
}

RankingEntryData rankingEntry(const QJsonObject &object)
{
    RankingEntryData entry;
    entry.position = object.value(QStringLiteral("Position")).toInt(0);
    entry.accountId = static_cast<quint32>(integerValue(object, "AccountId"));
    entry.steamId = object.value(QStringLiteral("SteamId")).toString();
    entry.username = object.value(QStringLiteral("Username")).toString();
    entry.personaName = object.value(QStringLiteral("PersonaName")).toString();
    entry.online = object.value(QStringLiteral("Online")).toBool(false);
    entry.mmr = object.value(QStringLiteral("Mmr")).toInt(0);
    entry.rankTier = object.value(QStringLiteral("RankTier")).toInt(0);
    entry.rankStar = object.value(QStringLiteral("RankStar")).toInt(0);
    entry.rankValue = object.value(QStringLiteral("RankValue")).toInt(0);
    entry.rankProgress = object.value(QStringLiteral("RankProgress")).toInt(0);
    entry.calibrated = object.value(QStringLiteral("IsCalibrated")).toBool(false);
    entry.games = object.value(QStringLiteral("Games")).toInt(0);
    entry.wins = object.value(QStringLiteral("Wins")).toInt(0);
    entry.losses = object.value(QStringLiteral("Losses")).toInt(0);
    entry.winRatePercent = object.value(QStringLiteral("WinRatePercent")).toInt(0);
    return entry;
}

RankingPageData rankingPage(const QJsonObject &object)
{
    RankingPageData page;
    for (const auto &value : object.value(QStringLiteral("Items")).toArray())
        page.items.append(rankingEntry(value.toObject()));
    page.page = object.value(QStringLiteral("Page")).toInt(1);
    page.pageSize = object.value(QStringLiteral("PageSize")).toInt(50);
    page.totalCount = object.value(QStringLiteral("TotalCount")).toInt(0);
    return page;
}
}

ServerClient::ServerClient(QObject *parent)
    : QObject(parent)
    , m_baseUrl(QStringLiteral("http://127.0.0.1:5199/"))
{
}

void ServerClient::setBaseUrl(const QString &url)
{
    auto normalized = url.trimmed();
    if (!normalized.endsWith(QLatin1Char('/')))
        normalized += QLatin1Char('/');
    m_baseUrl = normalized;
}

QNetworkReply *ServerClient::postJson(const QString &path,
                                      const QJsonObject &body,
                                      const QString &token)
{
    QNetworkRequest request(QUrl(m_baseUrl + path));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    if (!token.isEmpty())
        request.setRawHeader("Authorization", "Bearer " + token.toUtf8());
    return m_nam.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
}

QNetworkReply *ServerClient::getJson(const QString &path, const QString &token)
{
    QNetworkRequest request(QUrl(m_baseUrl + path));
    if (!token.isEmpty())
        request.setRawHeader("Authorization", "Bearer " + token.toUtf8());
    return m_nam.get(request);
}

bool ServerClient::isSuccessful(QNetworkReply *reply)
{
    const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    return reply->error() == QNetworkReply::NoError && (status == 0 || (status >= 200 && status < 300));
}

QString ServerClient::errorText(QNetworkReply *reply,
                                const QString &fallback,
                                const QByteArray &body)
{
    if (reply->error() == QNetworkReply::OperationCanceledError)
        return QStringLiteral("Sin respuesta del servidor");
    if (reply->error() == QNetworkReply::ConnectionRefusedError)
        return QStringLiteral("Servidor no accesible");
    const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status == 401 || reply->error() == QNetworkReply::AuthenticationRequiredError)
        return fallback;

    const auto payload = body.isEmpty() ? reply->readAll() : body;
    const auto document = QJsonDocument::fromJson(payload);
    if (document.isObject())
    {
        const auto object = document.object();
        const auto message = object.value(QStringLiteral("Message")).toString(
            object.value(QStringLiteral("message")).toString());
        if (!message.trimmed().isEmpty())
            return message.trimmed();
        const auto code = object.value(QStringLiteral("Code")).toString(
            object.value(QStringLiteral("code")).toString());
        if (!code.trimmed().isEmpty())
            return QStringLiteral("%1 (%2)").arg(fallback, code.trimmed());
    }

    const auto text = QString::fromUtf8(payload).trimmed();
    return text.isEmpty() ? fallback : QStringLiteral("%1: %2").arg(fallback, text);
}

void ServerClient::ping()
{
    auto *reply = getJson(QStringLiteral("api/version"), QString());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        QString version;
        if (isSuccessful(reply))
        {
            const auto object = QJsonDocument::fromJson(reply->readAll()).object();
            version = object.value(KeyVersion).toString();
        }
        const bool reachable = isSuccessful(reply);
        reply->deleteLater();
        emit pingFinished(reachable, version);
    });
}

void ServerClient::login(const QString &username, const QString &password)
{
    Log::line(QStringLiteral("LOGIN POST api/auth/login user=%1").arg(username));
    QJsonObject body;
    body.insert(KeyUsername, username);
    body.insert(KeyPassword, password);

    auto *reply = postJson(QStringLiteral("api/auth/login"), body);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (!isSuccessful(reply))
        {
            const auto raw = reply->readAll();
            const auto error = errorText(reply, QStringLiteral("Usuario o contraseña incorrectos"), raw);
            Log::line(QStringLiteral("LOGIN failed: %1").arg(error));
            emit loginFinished(false, error,
                               QString(), 0, 0);
            reply->deleteLater();
            return;
        }

        const QByteArray raw = reply->readAll();
        const auto object = QJsonDocument::fromJson(raw).object();
        const auto token = object.value(KeyToken).toString();
        const auto steamId = readBigInt(raw, QByteArrayLiteral("SteamId"));
        const auto accountId = static_cast<quint32>(object.value(KeyAccountId).toDouble(0));
        reply->deleteLater();

        if (token.isEmpty())
        {
            emit loginFinished(false, QStringLiteral("Respuesta de login inválida"), QString(), 0, 0);
            return;
        }

        Log::line(QStringLiteral("LOGIN OK accountId=%1").arg(accountId));
        emit loginFinished(true, QString(), token, steamId, accountId);
    });
}

void ServerClient::me(const QString &token)
{
    auto *reply = getJson(QStringLiteral("api/users/me"), token);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (!isSuccessful(reply))
        {
            const auto raw = reply->readAll();
            emit meFinished(false, errorText(reply, QStringLiteral("Sesión no válida"), raw),
                            QString(), 0, 0, 0);
            reply->deleteLater();
            return;
        }

        const QByteArray raw = reply->readAll();
        const auto object = QJsonDocument::fromJson(raw).object();
        const auto personaName = object.value(KeyPersonaName).toString();
        const auto accountId = static_cast<quint32>(object.value(KeyAccountId).toDouble(0));
        const auto steamId = readBigInt(raw, QByteArrayLiteral("SteamId"));
        const auto playerLevel = object.value(KeyPlayerLevel).toInt(0);
        reply->deleteLater();
        emit meFinished(true, QString(), personaName, accountId, steamId, playerLevel);
    });
}

void ServerClient::fetchRank(const QString &token)
{
    auto *reply = getJson(QStringLiteral("api/users/me/rank"), token);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (!isSuccessful(reply))
        {
            emit rankFinished(false, 0, 1, 1);
            reply->deleteLater();
            return;
        }

        const auto object = QJsonDocument::fromJson(reply->readAll()).object();
        const int mmr = object.value(KeyMmr).toInt(0);
        const int tier = object.value(KeyRankTier).toInt(1);
        const int star = object.value(KeyRankStar).toInt(1);
        reply->deleteLater();
        emit rankFinished(true, mmr, tier, star);
    });
}

quint64 ServerClient::fetchRanking(const QString &token, int page, int pageSize)
{
    const auto requestId = ++m_nextRankingRequestId;
    QUrl url(m_baseUrl + QStringLiteral("api/ranking"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("page"), QString::number(qMax(1, page)));
    query.addQueryItem(QStringLiteral("pageSize"), QString::number(qBound(10, pageSize, 100)));
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setRawHeader("Authorization", "Bearer " + token.toUtf8());
    auto *reply = m_nam.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, requestId]() {
        const auto raw = reply->readAll();
        if (!isSuccessful(reply))
        {
            emit rankingFinished(requestId, false,
                                 reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 401
                                     ? QStringLiteral("SESSION_EXPIRED")
                                     : errorText(reply, QStringLiteral("No se pudo cargar el ranking"), raw),
                                 RankingPageData{});
            reply->deleteLater();
            return;
        }

        const auto document = QJsonDocument::fromJson(raw);
        if (!document.isObject())
        {
            emit rankingFinished(requestId, false,
                                 QStringLiteral("El ranking devolvió una respuesta inválida"),
                                 RankingPageData{});
            reply->deleteLater();
            return;
        }

        emit rankingFinished(requestId, true, QString(), rankingPage(document.object()));
        reply->deleteLater();
    });
    return requestId;
}

void ServerClient::fetchAvatar(quint64 steamId, const QString &token)
{
    auto *reply = getJson(QStringLiteral("api/users/%1/avatar").arg(steamId), token);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (!isSuccessful(reply))
        {
            emit avatarFinished(false, QByteArray());
            reply->deleteLater();
            return;
        }

        const QByteArray png = reply->readAll();
        reply->deleteLater();
        emit avatarFinished(!png.isEmpty(), png);
    });
}

void ServerClient::logout(const QString &token, bool waitForDelivery)
{
    QNetworkRequest request(QUrl(m_baseUrl + QStringLiteral("api/presence/offline")));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    if (!token.isEmpty())
        request.setRawHeader("Authorization", "Bearer " + token.toUtf8());
    auto *reply = m_nam.post(request, QByteArrayLiteral("{}"));
    if (!waitForDelivery)
    {
        connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
        Log::line(QStringLiteral("LOGOUT POST api/presence/offline"));
        return;
    }

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(1500);
    loop.exec();
    reply->deleteLater();
    Log::line(QStringLiteral("LOGOUT POST api/presence/offline"));
}

void ServerClient::fetchStoreCatalog(const QString &token,
                                     int page,
                                     int pageSize,
                                     const QString &search,
                                     const QString &category,
                                     const QString &hero,
                                     int type)
{
    QUrl url(m_baseUrl + QStringLiteral("api/store/catalog/page"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("page"), QString::number(qMax(1, page)));
    query.addQueryItem(QStringLiteral("pageSize"), QString::number(qBound(10, pageSize, 100)));
    if (!search.trimmed().isEmpty())
        query.addQueryItem(QStringLiteral("search"), search.trimmed());
    if (!category.trimmed().isEmpty())
        query.addQueryItem(QStringLiteral("category"), category.trimmed());
    if (!hero.trimmed().isEmpty())
        query.addQueryItem(QStringLiteral("hero"), hero.trimmed());
    if (type >= 0)
        query.addQueryItem(QStringLiteral("type"), QString::number(type));
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setRawHeader("Authorization", "Bearer " + token.toUtf8());
    auto *reply = m_nam.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const auto raw = reply->readAll();
        if (!isSuccessful(reply))
        {
            emit storeCatalogFinished(false,
                                      reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 401
                                          ? QStringLiteral("SESSION_EXPIRED")
                                          : errorText(reply, QStringLiteral("No se pudo cargar el catálogo"), raw),
                                      StoreCatalogPageData{});
            reply->deleteLater();
            return;
        }

        const auto document = QJsonDocument::fromJson(raw);
        if (!document.isObject())
        {
            emit storeCatalogFinished(false, QStringLiteral("El catálogo devolvió una respuesta inválida"),
                                      StoreCatalogPageData{});
            reply->deleteLater();
            return;
        }
        emit storeCatalogFinished(true, QString(), catalogPage(document.object()));
        reply->deleteLater();
    });
}

void ServerClient::fetchStoreWallet(const QString &token)
{
    auto *reply = getJson(QStringLiteral("api/store/wallet"), token);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const auto raw = reply->readAll();
        if (!isSuccessful(reply))
        {
            emit storeWalletFinished(false,
                                     reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 401
                                         ? QStringLiteral("SESSION_EXPIRED")
                                         : errorText(reply, QStringLiteral("No se pudo cargar el saldo"), raw),
                                     StoreWalletData{});
            reply->deleteLater();
            return;
        }
        emit storeWalletFinished(true, QString(), walletData(QJsonDocument::fromJson(raw).object()));
        reply->deleteLater();
    });
}

void ServerClient::fetchStoreInventory(const QString &token)
{
    auto *reply = getJson(QStringLiteral("api/store/inventory"), token);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const auto raw = reply->readAll();
        if (!isSuccessful(reply))
        {
            emit storeInventoryFinished(false,
                                        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 401
                                            ? QStringLiteral("SESSION_EXPIRED")
                                            : errorText(reply, QStringLiteral("No se pudo cargar el inventario"), raw),
                                        {});
            reply->deleteLater();
            return;
        }
        emit storeInventoryFinished(true, QString(),
                                    inventoryList(QJsonDocument::fromJson(raw).object()));
        reply->deleteLater();
    });
}

void ServerClient::fetchStoreTransactions(const QString &token, int limit)
{
    QUrl url(m_baseUrl + QStringLiteral("api/store/transactions"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("limit"), QString::number(qBound(1, limit, 50)));
    url.setQuery(query);
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", "Bearer " + token.toUtf8());
    auto *reply = m_nam.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const auto raw = reply->readAll();
        if (!isSuccessful(reply))
        {
            emit storeTransactionsFinished(false,
                                           reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 401
                                               ? QStringLiteral("SESSION_EXPIRED")
                                               : errorText(reply, QStringLiteral("No se pudo cargar la actividad"), raw),
                                           {});
            reply->deleteLater();
            return;
        }

        QVector<StoreTransactionData> items;
        const auto document = QJsonDocument::fromJson(raw);
        for (const auto &value : document.array())
        {
            const auto object = value.toObject();
            StoreTransactionData item;
            item.reference = object.value(QStringLiteral("Reference")).toString();
            item.amountDollars = integerValue(object, "AmountDollars");
            item.createdAt = object.value(QStringLiteral("CreatedAt")).toString();
            items.append(item);
        }
        emit storeTransactionsFinished(true, QString(), items);
        reply->deleteLater();
    });
}

void ServerClient::purchaseStoreItem(const QString &token, quint32 productId, quint32 quantity)
{
    QJsonObject body;
    body.insert(QStringLiteral("ProductId"), static_cast<int>(productId));
    body.insert(QStringLiteral("Quantity"), static_cast<int>(qMax(1u, quantity)));
    auto *reply = postJson(QStringLiteral("api/store/purchase"), body, token);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const auto raw = reply->readAll();
        const auto document = QJsonDocument::fromJson(raw);
        const auto object = document.object();
        StorePurchaseData purchase;
        purchase.success = object.value(QStringLiteral("Success")).toBool(false);
        purchase.code = object.value(QStringLiteral("Code")).toString();
        purchase.message = object.value(QStringLiteral("Message")).toString();
        purchase.wallet = walletData(object.value(QStringLiteral("Wallet")).toObject());
        for (const auto &value : object.value(QStringLiteral("Items")).toArray())
            purchase.items.append(inventoryItem(value.toObject()));

        if (!isSuccessful(reply))
        {
            emit storePurchaseFinished(false,
                                       reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 401
                                           ? QStringLiteral("SESSION_EXPIRED")
                                           : errorText(reply, purchase.message.isEmpty()
                                                           ? QStringLiteral("La compra no pudo completarse")
                                                           : purchase.message,
                                                       raw),
                                       purchase);
            reply->deleteLater();
            return;
        }
        emit storePurchaseFinished(purchase.success,
                                   purchase.message,
                                   purchase);
        reply->deleteLater();
    });
}
