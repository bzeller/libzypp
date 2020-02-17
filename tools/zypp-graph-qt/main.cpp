#include <QApplication>
#include <QQmlApplicationEngine>
#include <QAbstractItemModel>
#include <QSortFilterProxyModel>
#include <QtQuickControls2/QQuickStyle>
#include "graphmodel.h"
#include "graphitem.h"
#include "zypppoolmodel.h"
#include "mainwindow.h"
#include "testcase/testcasesetup.h"
#include "testcase/channel.h"

#include <zypp/ZConfig.h>
#include <zypp/sat/Pool.h>

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setOrganizationName("SUSE");
    QCoreApplication::setOrganizationDomain("suse.de");
    QCoreApplication::setApplicationName("zypp-testcase-viewer");
    QQuickStyle::setStyle("Fusion");
    QApplication::setStyle("Motif");

    QApplication app(argc, argv);

    zypp::ZConfig::instance();
    zypp::Pathname sysRoot("/");
    zypp::sat::Pool satpool( zypp::sat::Pool::instance() );

    qmlRegisterType<GraphItem>("de.suse.zyppgraph", 1, 0, "GraphItem");
    qmlRegisterType<Graph>("de.suse.zyppgraph", 1, 0, "Graph");
    qmlRegisterType<ZyppPoolModel>("de.suse.zyppgraph", 1, 0, "ZyppPool");
    qmlRegisterType<QSortFilterProxyModel>("de.suse.zyppgraph", 1, 0, "SortModel");
    qmlRegisterType<TestcaseSetup>("de.suse.zyppgraph", 1, 0, "TestcaseSetup");
    qmlRegisterUncreatableType<Channel>("de.suse.zyppgraph", 1, 0, "Channel", "Zonkomat");

    MainWindow w;
    w.show();

#if 0

    QQmlApplicationEngine engine;
    const QUrl url(QStringLiteral("qrc:/main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
        &app, [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl)
                QCoreApplication::exit(-1);
        }, Qt::QueuedConnection);
    engine.load(url);
#endif
    return app.exec();
}
