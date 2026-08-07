#include "ServerClient.h"

#include "../util/Log.h"

#include <QJsonDocument>
#include <QJsonParseError>
#include <QEventLoop>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>

#include <utility>

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

QNetworkReply *ServerClient::postJson(const QString &path, const QJsonObject &body)
{
    QNetworkRequest request(QUrl(m_baseUrl + path));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    return m_nam.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
}

QNetworkReply *ServerClient::getJson(const QString &path, const QString &token)
{
    QNetworkRequest request(QUrl(m_baseUrl + path));
    if (!token.isEmpty())
        request.setRawHeader("Authorization", "Bearer " + token.toUtf8());
    return m_nam.get(request);
}

QString ServerClient::errorText(QNetworkReply *reply, const QString &fallback)
{
    if (reply->error() == QNetworkReply::OperationCanceledError)
        return QStringLiteral("Sin respuesta del servidor");
    if (reply->error() == QNetworkReply::ConnectionRefusedError)
        return QStringLiteral("Servidor no accesible");
    if (reply->error() == QNetworkReply::AuthenticationRequiredError)
        return QStringLiteral("Usuario o contraseña incorrectos");
    const auto body = QString::fromUtf8(reply->readAll()).trimmed();
    return body.isEmpty() ? fallback : QStringLiteral("%1: %2").arg(fallback, body);
}

void ServerClient::ping()
{
    auto *reply = getJson(QStringLiteral("api/version"), QString());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        QString version;
        if (reply->error() == QNetworkReply::NoError)
        {
            const auto object = QJsonDocument::fromJson(reply->readAll()).object();
            version = object.value(KeyVersion).toString();
        }
        const bool reachable = reply->error() == QNetworkReply::NoError;
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
        if (reply->error() != QNetworkReply::NoError)
        {
            Log::line(QStringLiteral("LOGIN failed: %1").arg(errorText(reply, QStringLiteral("Error de servidor"))));
            emit loginFinished(false, errorText(reply, QStringLiteral("Error de servidor")),
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
        if (reply->error() != QNetworkReply::NoError)
        {
            emit meFinished(false, errorText(reply, QStringLiteral("Sesión no válida")),
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
        if (reply->error() != QNetworkReply::NoError)
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

void ServerClient::fetchAvatar(quint64 steamId, const QString &token)
{
    auto *reply = getJson(QStringLiteral("api/users/%1/avatar").arg(steamId), token);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError)
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
