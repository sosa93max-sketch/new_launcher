#include "config/ConfigStore.h"
#include "net/ServerClient.h"
#include "ui/MainWindow.h"
#include "util/Log.h"

#include <QApplication>
#include <QFile>

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
    window.show();
    return app.exec();
}
