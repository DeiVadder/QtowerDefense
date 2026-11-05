#include "client/game_session.h"
#include "client/graphics.h"

#include <QGuiApplication>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QSet>
#include <QTest>
#include <QtQml/qqmlextensionplugin.h>

Q_IMPORT_QML_PLUGIN(QtowerDefenseClientPlugin)

namespace {

qsizetype distinctColors(const QImage &image)
{
    constexpr int gridSteps = 32;
    QSet<QRgb> colors;
    for (int y = 0; y < gridSteps; ++y) {
        for (int x = 0; x < gridSteps; ++x) {
            colors.insert(
                image.pixel(x * image.width() / gridSteps, y * image.height() / gridSteps));
        }
    }
    return colors.size();
}

/// The game of the real window, once that is on the screen.
const qtd::client::GameSession *openGame(QQmlApplicationEngine *engine)
{
    engine->loadFromModule("QtowerDefense.Client", "Main");
    auto *window = qobject_cast<QQuickWindow *>(engine->rootObjects().constFirst());
    return window != nullptr && QTest::qWaitForWindowExposed(window)
               ? window->findChild<qtd::client::GameSession *>()
               : nullptr;
}

} // namespace

class TestWindow : public QObject
{
    Q_OBJECT

private slots:
    static void rendersTheScene()
    {
        QQmlApplicationEngine engine;
        engine.loadFromModule("QtowerDefense.Client", "Main");
        QCOMPARE(engine.rootObjects().size(), 1);

        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        QVERIFY(window != nullptr);
        QVERIFY(window->title().startsWith(QStringLiteral("QtowerDefense ")));
        QVERIFY(QTest::qWaitForWindowExposed(window));

        const QImage image = window->grabWindow();
        QCOMPARE(image.size(), window->size() * window->devicePixelRatio());
        // Background, lit ground and lit cube: an empty or single-colored frame stays below this.
        QVERIFY2(distinctColors(image) >= 3, "the frame shows no 3D scene");
    }

    // Real time on purpose: this is about the frames of the real window driving the game.
    static void framesDriveTheGame()
    {
        QQmlApplicationEngine engine;
        const qtd::client::GameSession *game = openGame(&engine);
        QVERIFY(game != nullptr);
        QVERIFY(QTest::qWaitFor([game] { return game->tick() > 0; }));
        QVERIFY(!game->paused());
    }

    static void aMinimizedWindowPausesTheGame()
    {
        QQmlApplicationEngine engine;
        const qtd::client::GameSession *game = openGame(&engine);
        QVERIFY(game != nullptr);
        qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst())->showMinimized();
        QVERIFY(QTest::qWaitFor([game] { return game->paused(); }));
    }
};

int main(int argc, char *argv[])
{
    qtd::client::configureGraphics();
    const QGuiApplication app(argc, argv);
    TestWindow test;
    return QTest::qExec(&test, argc, argv);
}
#include "tst_window.moc"
