#pragma once

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QVector>

#include <cstdint>

class QNetworkReply;

struct StoreCatalogItemData
{
    quint32 productId = 0;
    quint32 defIndex = 0;
    QString name;
    int productType = 0;
    qint64 priceDollars = 0;
    QString category;
    QString description;
    bool active = false;
    quint32 ownedQuantity = 0;
    qint64 marketLowestPriceCents = 0;
    qint64 marketMedianPriceCents = 0;
    QString marketPriceStatus;
    QVector<QString> heroes;
};

struct StoreCatalogPageData
{
    QVector<StoreCatalogItemData> items;
    int page = 1;
    int pageSize = 24;
    int totalCount = 0;
    int activeCount = 0;
    QVector<QString> categories;
    QVector<QString> heroes;
};

struct StoreWalletData
{
    qint64 balanceDollars = 0;
    qint64 reservedDollars = 0;
    qint64 availableDollars = 0;
};

struct StoreInventoryItemData
{
    quint64 itemId = 0;
    quint32 defIndex = 0;
    quint32 quantity = 0;
};

struct StoreTransactionData
{
    QString reference;
    qint64 amountDollars = 0;
    QString createdAt;
};

struct StorePurchaseData
{
    bool success = false;
    QString code;
    QString message;
    StoreWalletData wallet;
    QVector<StoreInventoryItemData> items;
};

/// Talks to the D2ST server. The wire format is PascalCase JSON (the server
/// serializes with PropertyNamingPolicy = null), so all keys below are exact.
class ServerClient : public QObject
{
    Q_OBJECT

public:
    explicit ServerClient(QObject *parent = nullptr);

    void setBaseUrl(const QString &url);
    QString baseUrl() const { return m_baseUrl; }

    /// GET /api/version — cheap reachability + version probe.
    void ping();

    /// POST /api/auth/login {Username, Password}. Unknown users are registered
    /// by the server on first login; the reply carries the session token.
    void login(const QString &username, const QString &password);

    /// GET /api/users/me with the bearer token, to validate a saved session.
    void me(const QString &token);

    /// GET /api/users/me/rank — the caller's MMR and medal.
    void fetchRank(const QString &token);

    /// GET /api/users/{steamId}/avatar — the avatar PNG bytes.
    void fetchAvatar(quint64 steamId, const QString &token);

    /// POST /api/presence/offline: tells the server the account went offline.
    /// Fire-and-forget unless waitForDelivery is set (used on app close, so the
    /// server sees the account go offline before the process exits).
    void logout(const QString &token, bool waitForDelivery = false);

    /// Native launcher store API. All calls use the same bearer token returned
    /// by the launcher login, so the embedded store never depends on cookies.
    void fetchStoreCatalog(const QString &token,
                           int page,
                           int pageSize,
                           const QString &search,
                           const QString &category,
                           const QString &hero,
                           int type);
    void fetchStoreWallet(const QString &token);
    void fetchStoreInventory(const QString &token);
    void fetchStoreTransactions(const QString &token, int limit = 8);
    void purchaseStoreItem(const QString &token, quint32 productId, quint32 quantity = 1);

signals:
    void pingFinished(bool reachable, const QString &version);
    void loginFinished(bool ok, const QString &error, const QString &token,
                       quint64 steamId, quint32 accountId);
    void meFinished(bool ok, const QString &error, const QString &personaName,
                    quint32 accountId, quint64 steamId, int playerLevel);
    void rankFinished(bool ok, int mmr, int rankTier, int rankStar);
    void avatarFinished(bool ok, const QByteArray &png);
    void storeCatalogFinished(bool ok, const QString &error,
                              const StoreCatalogPageData &page);
    void storeWalletFinished(bool ok, const QString &error,
                             const StoreWalletData &wallet);
    void storeInventoryFinished(bool ok, const QString &error,
                                const QVector<StoreInventoryItemData> &items);
    void storeTransactionsFinished(bool ok, const QString &error,
                                   const QVector<StoreTransactionData> &items);
    void storePurchaseFinished(bool ok, const QString &error,
                               const StorePurchaseData &purchase);

private:
    QNetworkReply *postJson(const QString &path,
                            const QJsonObject &body,
                            const QString &token = {});
    QNetworkReply *getJson(const QString &path, const QString &token);
    static bool isSuccessful(QNetworkReply *reply);
    static QString errorText(QNetworkReply *reply,
                             const QString &fallback,
                             const QByteArray &body = {});

    QNetworkAccessManager m_nam;
    QString m_baseUrl;
};
