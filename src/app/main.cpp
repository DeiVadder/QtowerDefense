#include "client/graphics.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtQml/qqmlextensionplugin.h>

Q_IMPORT_QML_PLUGIN(QtowerDefenseClientPlugin)

int main(int argc, char *argv[])
{
    qtd::client::configureGraphics();
    const QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        [] { QCoreApplication::exit(1); },
        Qt::QueuedConnection);
    engine.loadFromModule("QtowerDefense.Client", "Main");

    return QGuiApplication::exec();
}
