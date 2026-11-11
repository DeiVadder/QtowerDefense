#include "client/app_info.h"
#include "client/game_session.h"
#include "client/graphics.h"
#include "client/screen_control.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QtQml/qqmlextensionplugin.h>

#ifdef Q_OS_WASM
#include "web_storage.h"
#endif

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

/// Only a game for people saves on its own: a controlled one names its files itself.
void useSaveFile(const QQmlApplicationEngine &engine)
{
    if (engine.rootObjects().isEmpty()) {
        return;
    }
    auto *game = engine.rootObjects().constFirst()->findChild<qtd::client::GameSession *>();
    if (game != nullptr) {
        const QDir folder(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
        game->setSaveFile(folder.filePath(u"running_game.json"_s));
    }
}

/// The whole program. Everything of Qt is gone when it returns.
int run(int argc, char *argv[], bool *controlled)
{
    qtd::client::configureGraphics();
    // The folder of settings and saves is named after it. The browser has no program file to take
    // the name from and calls every program "this.program".
    QCoreApplication::setApplicationName(u"QtowerDefense"_s);
    const QGuiApplication app(argc, argv);
    QGuiApplication::setWindowIcon(QIcon(u":/assets/icon.png"_s));

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
    *controlled = parser.isSet(u"control"_s);
    qtd::client::AppInfo::setControlled(*controlled); // before the window: no intro (E-42)
#ifdef Q_OS_WASM
    // The settings and saves of the last visit come from the browser first (E-47). exec() does not
    // return in the browser, the engine on this stack stays.
    qtd::app::openWebStorage(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation),
                             [&engine] {
                                 engine.loadFromModule("QtowerDefense.Client", "Main");
                                 useSaveFile(engine);
                             });
    return QGuiApplication::exec();
#else
    engine.loadFromModule("QtowerDefense.Client", "Main");
#endif

    if (!*controlled) {
        useSaveFile(engine);
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
