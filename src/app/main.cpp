#include "client/game_session.h"
#include "client/graphics.h"
#include "client/screen_control.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QtQml/qqmlextensionplugin.h>

#include <cstdio>
#include <cstdlib>
#include <memory>

Q_IMPORT_QML_PLUGIN(QtowerDefenseClientPlugin)

using namespace Qt::StringLiterals;

namespace {

/// The control interface on the window of the engine, nothing if the window is not there.
std::unique_ptr<qtd::client::ScreenControl> controlOf(const QQmlApplicationEngine &engine)
{
    if (engine.rootObjects().isEmpty()) {
        return nullptr;
    }
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    auto *game = window != nullptr ? window->findChild<qtd::client::GameSession *>() : nullptr;
    return game != nullptr ? std::make_unique<qtd::client::ScreenControl>(game, window) : nullptr;
}

/// The whole program. Everything of Qt is gone when it returns.
int run(int argc, char *argv[], bool *controlled)
{
    qtd::client::configureGraphics();
    const QGuiApplication app(argc, argv);

    QCommandLineParser parser;
    parser.addOption({u"control"_s, u"Play through JSON lines on stdin and stdout."_s});
    parser.process(app);

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        [] { QCoreApplication::exit(1); },
        Qt::QueuedConnection);
    engine.loadFromModule("QtowerDefense.Client", "Main");

    *controlled = parser.isSet(u"control"_s);
    if (!*controlled) {
        return QGuiApplication::exec();
    }
    const std::unique_ptr<qtd::client::ScreenControl> control = controlOf(engine);
    if (control == nullptr) {
        return 1;
    }
    control->listenOnStandardInput();
    return QGuiApplication::exec();
}

} // namespace

int main(int argc, char *argv[])
{
    bool controlled = false;
    const int result = run(argc, argv, &controlled);
    if (controlled) {
        // The reading thread may still sit in stdin, and the normal end of a process wants the lock
        // of that stream. Nothing is left to do here anyway.
        std::fflush(stdout);
        std::_Exit(result);
    }
    return result;
}
