#pragma once

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>

#include <cstdint>

class QNetworkReply;

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

signals:
    void pingFinished(bool reachable, const QString &version);
    void loginFinished(bool ok, const QString &error, const QString &token,
                       quint64 steamId, quint32 accountId);
    void meFinished(bool ok, const QString &error, const QString &personaName,
                    quint32 accountId, quint64 steamId, int playerLevel);

private:
    QNetworkReply *postJson(const QString &path, const QJsonObject &body);
    QNetworkReply *getJson(const QString &path, const QString &token);
    static QString errorText(QNetworkReply *reply, const QString &fallback);

    QNetworkAccessManager m_nam;
    QString m_baseUrl;
};
