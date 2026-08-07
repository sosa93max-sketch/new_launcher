#include "ServerClient.h"

#include "../util/Log.h"

#include <QJsonDocument>
#include <QJsonParseError>
#include <QNetworkReply>
#include <QNetworkRequest>
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

        const auto object = QJsonDocument::fromJson(reply->readAll()).object();
        const auto token = object.value(KeyToken).toString();
        const auto steamId = object.value(KeySteamId).toVariant().toULongLong();
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

        const auto object = QJsonDocument::fromJson(reply->readAll()).object();
        const auto personaName = object.value(KeyPersonaName).toString();
        const auto accountId = static_cast<quint32>(object.value(KeyAccountId).toDouble(0));
        const auto steamId = object.value(KeySteamId).toVariant().toULongLong();
        const auto playerLevel = object.value(KeyPlayerLevel).toInt(0);
        reply->deleteLater();
        emit meFinished(true, QString(), personaName, accountId, steamId, playerLevel);
    });
}

void ServerClient::logout(const QString &token)
{
    QNetworkRequest request(QUrl(m_baseUrl + QStringLiteral("api/presence/offline")));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    if (!token.isEmpty())
        request.setRawHeader("Authorization", "Bearer " + token.toUtf8());
    auto *reply = m_nam.post(request, QByteArrayLiteral("{}"));
    connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
    Log::line(QStringLiteral("LOGOUT POST api/presence/offline"));
}
