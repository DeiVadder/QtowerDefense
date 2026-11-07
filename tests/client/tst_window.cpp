#include "client/game_session.h"
#include "client/graphics.h"
#include "client/world_session.h"

#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSet>
#include <QStandardPaths>
#include <QTest>
#include <QVariantMap>
#include <QtQml/qqmlextensionplugin.h>

Q_IMPORT_QML_PLUGIN(QtowerDefenseClientPlugin)

using namespace Qt::StringLiterals;

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

QQuickWindow *openWindow(QQmlApplicationEngine *engine)
{
    engine->loadFromModule("QtowerDefense.Client", "Main");
    auto *window = qobject_cast<QQuickWindow *>(engine->rootObjects().constFirst());
    return window != nullptr && QTest::qWaitForWindowExposed(window) ? window : nullptr;
}

/// The game of the real window, with the first section of the world chosen on its map.
qtd::client::GameSession *openGame(QQmlApplicationEngine *engine)
{
    const QQuickWindow *window = openWindow(engine);
    QObject *world = window != nullptr ? window->findChild<QObject *>(u"worldMap"_s) : nullptr;
    if (world == nullptr || !QMetaObject::invokeMethod(world, "play", 0)) {
        return nullptr;
    }
    return window->findChild<qtd::client::GameSession *>();
}

QString progressFile()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + u"/progress.json"_s;
}

/// Plays a game of the training ground to its end with two towers that hold it.
bool win(qtd::client::GameSession *game)
{
    if (!game->buildTower(4, 4) || !game->buildTower(10, 6)) {
        return false;
    }
    while (!game->won() && !game->lost()) {
        game->advanceTime(1'000'000);
    }
    return game->won();
}

QVariantMap sectionOf(const QQuickWindow *window, int section)
{
    return window->findChild<qtd::client::WorldSession *>()->sections().at(section).toMap();
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

    static void theWindowOpensOnTheWorld()
    {
        QQmlApplicationEngine engine;
        const QQuickWindow *window = openWindow(&engine);
        QVERIFY(window != nullptr);
        QVERIFY(window->findChild<QQuickItem *>(u"worldMap"_s)->isVisible());
        QVERIFY(window->findChild<qtd::client::GameSession *>()->mapFile().isEmpty());
        QVERIFY(window->findChild<qtd::client::WorldSession *>()->error().isEmpty());
    }

    // Stars are written to the progress of the test mode, a fresh one each time.
    static void aWonSectionKeepsItsStarsAndOpensTheWay()
    {
        QFile::remove(progressFile());
        QQmlApplicationEngine engine;
        qtd::client::GameSession *game = openGame(&engine);
        QVERIFY(game != nullptr);
        const auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        QVERIFY(!window->findChild<QQuickItem *>(u"worldMap"_s)->isVisible());
        QVERIFY(game->mapFile().endsWith(u"training_ground.json"_s));
        QVERIFY(!sectionOf(window, 1)[u"open"_s].toBool());
        QVERIFY(win(game));
        QCOMPARE(sectionOf(window, 0)[u"stars"_s].toInt(), game->stars());
        QVERIFY(game->stars() > 0);
        QVERIFY(sectionOf(window, 1)[u"open"_s].toBool());
        QVERIFY(QFile::exists(progressFile()));
        QVERIFY(QMetaObject::invokeMethod(window->findChild<QObject *>(u"hud"_s), "leave"));
        QVERIFY(window->findChild<QQuickItem *>(u"worldMap"_s)->isVisible());
    }

    // The file only appears again when the second game counts.
    static void aRestartCountsAgainAnotherMapDoesNot()
    {
        QQmlApplicationEngine engine;
        qtd::client::GameSession *game = openGame(&engine);
        QVERIFY(game != nullptr);
        QVERIFY(win(game));
        QVERIFY(QFile::remove(progressFile()));
        game->restart();
        QVERIFY(win(game));
        QVERIFY(QFile::remove(progressFile()));
        // The same map from elsewhere is no section of the world.
        game->startNewGame(QStringLiteral(QTD_CONTENT_DIR "/training_ground.json"));
        QVERIFY(win(game));
        QVERIFY(!QFile::exists(progressFile()));
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
    QStandardPaths::setTestModeEnabled(true); // the progress of a player stays as it is
    TestWindow test;
    return QTest::qExec(&test, argc, argv);
}
#include "tst_window.moc"
