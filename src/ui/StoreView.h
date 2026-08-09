#pragma once

#include "../net/ServerClient.h"

#include <QTimer>
#include <QVector>
#include <QWidget>

class QComboBox;
class QGridLayout;
class QLabel;
class QLayout;
class QLineEdit;
class QPushButton;
class QVBoxLayout;
class QFrame;

/// Native store surface rendered inside the launcher. It deliberately receives
/// the launcher's bearer token instead of creating a second browser session.
class StoreView : public QWidget
{
    Q_OBJECT

public:
    explicit StoreView(ServerClient &server, QWidget *parent = nullptr);

    void setSessionToken(const QString &token);
    void reload();

signals:
    void backRequested();
    void loginRequested();

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void buildUi();
    void loadCatalog(int page = 1);
    void renderCatalog();
    void renderInventory(const QVector<StoreInventoryItemData> &items);
    void renderTransactions(const QVector<StoreTransactionData> &items);
    void renderSessionGate(const QString &title, const QString &message);
    void setBusy(bool busy);
    void populateFilters(const StoreCatalogPageData &page);
    void clearLayout(QLayout *layout);
    void buy(quint32 productId);
    void handleStoreError(const QString &error);
    QWidget *createProductCard(const StoreCatalogItemData &item);

    ServerClient &m_server;
    QString m_token;

    QLabel *m_walletValue = nullptr;
    QLabel *m_walletMeta = nullptr;
    QLabel *m_catalogMeta = nullptr;
    QLabel *m_pageLabel = nullptr;
    QLabel *m_stateLabel = nullptr;
    QLineEdit *m_search = nullptr;
    QComboBox *m_category = nullptr;
    QComboBox *m_hero = nullptr;
    QComboBox *m_type = nullptr;
    QPushButton *m_refreshButton = nullptr;
    QPushButton *m_previousPage = nullptr;
    QPushButton *m_nextPage = nullptr;
    QWidget *m_catalogHost = nullptr;
    QWidget *m_filterCard = nullptr;
    QWidget *m_catalogHeaderHost = nullptr;
    QWidget *m_pagerHost = nullptr;
    QWidget *m_lowerHost = nullptr;
    QGridLayout *m_catalogGrid = nullptr;
    QVBoxLayout *m_inventoryLayout = nullptr;
    QVBoxLayout *m_transactionsLayout = nullptr;

    QTimer m_searchTimer;
    QVector<StoreCatalogItemData> m_catalog;
    int m_page = 1;
    int m_pageSize = 12;
    int m_totalCount = 0;
    int m_totalPages = 1;
    bool m_busy = false;
    bool m_reflowPending = false;
    bool m_sessionInvalid = false;
};
