#include "config/ConfigStore.h"
#include "net/ServerClient.h"
#include "ui/MainWindow.h"
#include "util/Log.h"

#include <QApplication>
#include <QFile>

namespace
{
/// Whether the current profile carries a session token (a prior successful
/// login). Expired tokens are re-validated in the background by the window.
bool hasLoggedInProfile(const AppConfig &config)
{
    if (config.currentUsername.isEmpty())
        return false;
    for (const auto &profile : config.profiles)
    {
        if (profile.username == config.currentUsername)
            return !profile.token.isEmpty();
    }
    return false;
}
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("D2Max Launcher"));
    QApplication::setOrganizationName(QStringLiteral("D2Max"));
    QApplication::setApplicationVersion(QStringLiteral("1.0.0"));

    QFile qss(QStringLiteral(":/theme.qss"));
    if (qss.open(QIODevice::ReadOnly))
        app.setStyleSheet(QString::fromUtf8(qss.readAll()));

    ConfigStore store;
    store.load();
    Log::init();

    ServerClient server;
    server.setBaseUrl(store.config().serverUrl);

    MainWindow window(store, server);
    // The dashboard is only reachable after a successful login. Without a
    // logged-in profile the login dialog is required, and cancelling it closes
    // the launcher instead of opening an empty dashboard.
    if (!hasLoggedInProfile(store.config()) && !window.addAccount())
        return 0;
    window.show();
    return app.exec();
}
