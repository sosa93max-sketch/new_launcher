#pragma once

#include "../net/ServerClient.h"

#include <QWidget>

class QLabel;
class QLayout;
class QPushButton;
class QVBoxLayout;

/// Native ranking surface rendered inside the launcher. It uses the same
/// bearer token as the dashboard and the embedded store.
class RankingView : public QWidget
{
    Q_OBJECT

public:
    explicit RankingView(ServerClient &server, QWidget *parent = nullptr);

    void setSessionToken(const QString &token);
    void reload();

signals:
    void backRequested();
    void loginRequested();

private:
    void buildUi();
    void renderRanking();
    void renderLoadingState();
    void renderSessionGate(const QString &title, const QString &message);
    void renderEmptyState();
    void clearLayout(QLayout *layout);
    void setBusy(bool busy);
    void handleRankingError(const QString &error);
    QWidget *createRankingRow(const RankingEntryData &entry);

    ServerClient &m_server;
    QString m_token;
    QLabel *m_countLabel = nullptr;
    QLabel *m_stateLabel = nullptr;
    QPushButton *m_refreshButton = nullptr;
    QWidget *m_listHost = nullptr;
    QVBoxLayout *m_listLayout = nullptr;

    QVector<RankingEntryData> m_entries;
    int m_totalCount = 0;
    bool m_busy = false;
    bool m_sessionInvalid = false;
    bool m_hasLoaded = false;
    quint64 m_activeRequestId = 0;
};
