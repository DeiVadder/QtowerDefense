#include "client/game_session.h"
#include "client/graphics.h"
#include "client/world_session.h"

#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QLocale>
#include <QPoint>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSet>
#include <QStandardPaths>
#include <QTest>
#include <QVariantMap>
#include <QtQml/qqmlextensionplugin.h>

#include <algorithm>
#include <iterator>

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

/// Whether a visible item of the window shows the text, as a sign of the menu does.
bool showsText(const QQuickWindow *window, const QString &text)
{
    QList<const QQuickItem *> open{window->contentItem()};
    while (!open.isEmpty()) {
        const QQuickItem *item = open.takeLast();
        if (item->isVisible() && item->property("text").toString() == text) {
            return true;
        }
        const QList<QQuickItem *> children = item->childItems();
        std::ranges::copy(children, std::back_inserter(open));
    }
    return false;
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

QString settingsFile()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + u"/settings.ini"_s;
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

/// Wins the first section, then plays its endless game with two towers into the second round.
bool playEndless(const QQuickWindow *window, qtd::client::GameSession *game)
{
    QObject *menu = window->findChild<QObject *>(u"mainMenu"_s);
    QObject *world = window->findChild<QObject *>(u"worldMap"_s);
    if (menu == nullptr || world == nullptr || !win(game) ||
        !QMetaObject::invokeMethod(menu, "endless") ||
        !QMetaObject::invokeMethod(world, "play", 0) || !game->endless() ||
        !game->buildTower(4, 4) || !game->buildTower(10, 6)) {
        return false;
    }
    while (game->round() < 1 && !game->lost()) {
        game->advanceTime(1'000'000);
    }
    return game->round() == 1;
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
    // A test that ends early must not leave its settings to the next, full screen above all.
    static void init()
    {
        QFile::remove(settingsFile());
    }

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

    // The menu leads to the world map and the world map back, by its button or the Back key.
    static void theWindowOpensOnTheMenu()
    {
        QQmlApplicationEngine engine;
        QQuickWindow *window = openWindow(&engine);
        QVERIFY(window != nullptr);
        auto *menu = window->findChild<QQuickItem *>(u"mainMenu"_s);
        auto *world = window->findChild<QQuickItem *>(u"worldMap"_s);
        QVERIFY(menu->isVisible());
        QVERIFY(!world->isVisible());
        QVERIFY(window->findChild<qtd::client::GameSession *>()->mapFile().isEmpty());
        QVERIFY(window->findChild<qtd::client::WorldSession *>()->error().isEmpty());
        QVERIFY(QMetaObject::invokeMethod(menu, "campaign"));
        QVERIFY(world->isVisible());
        QVERIFY(!menu->isVisible());
        QVERIFY(QMetaObject::invokeMethod(world, "menu"));
        QVERIFY(menu->isVisible());
        QVERIFY(QMetaObject::invokeMethod(menu, "campaign"));
        QTest::keyClick(window, Qt::Key_Escape);
        QVERIFY(menu->isVisible());
    }

    // The settings of the test mode, a fresh file each time.
    static void settingsAreKeptForTheNextStart()
    {
        {
            QQmlApplicationEngine engine;
            const QQuickWindow *window = openWindow(&engine);
            QVERIFY(window != nullptr);
            QObject *settings = window->findChild<QObject *>(u"gameSettings"_s);
            QVERIFY(settings != nullptr);
            QCOMPARE(settings->property("volume").toInt(), 80);
            QVERIFY(settings->setProperty("volume", 30));
            QVERIFY(settings->setProperty("heroSpeaks", false));
        }
        QVERIFY(QFile::exists(settingsFile()));
        QQmlApplicationEngine engine;
        const QQuickWindow *window = openWindow(&engine);
        QVERIFY(window != nullptr);
        const QObject *settings = window->findChild<QObject *>(u"gameSettings"_s);
        QCOMPARE(settings->property("volume").toInt(), 30);
        QVERIFY(!settings->property("heroSpeaks").toBool());
        // What reaches the parts that act on it.
        const QObject *hero = window->findChild<QObject *>(u"hero"_s);
        const QObject *sounds = window->findChild<QObject *>(u"gameSounds"_s);
        QVERIFY(hero != nullptr && sounds != nullptr);
        QVERIFY(!hero->property("speaks").toBool());
        QCOMPARE(sounds->property("volume").toReal(), 0.3);
        QVERIFY(QFile::remove(settingsFile()));
    }

    // The file belongs to the player, whatever is in it.
    static void brokenSettingsAreReadKindly()
    {
        QFile file(settingsFile());
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("[General]\nvolume=500\nheroSpeaks=perhaps\n");
        file.close();
        QQmlApplicationEngine engine;
        const QQuickWindow *window = openWindow(&engine);
        QVERIFY(window != nullptr);
        const QObject *settings = window->findChild<QObject *>(u"gameSettings"_s);
        QVERIFY(settings != nullptr);
        QCOMPARE(settings->property("volume").toInt(), 100);
    }

    static void theSettingTurnsTheFullScreenOnAndOff()
    {
        QQmlApplicationEngine engine;
        const QQuickWindow *window = openWindow(&engine);
        QVERIFY(window != nullptr);
        QObject *settings = window->findChild<QObject *>(u"gameSettings"_s);
        QCOMPARE(window->visibility(), QWindow::Windowed);
        QVERIFY(settings->setProperty("fullScreen", true));
        QVERIFY(QTest::qWaitFor([window] { return window->visibility() == QWindow::FullScreen; }));
        QVERIFY(settings->setProperty("fullScreen", false));
        QVERIFY(QTest::qWaitFor([window] { return window->visibility() == QWindow::Windowed; }));
    }

    // The setting reaches the texts of the window, there and back.
    static void theSettingChangesTheLanguage()
    {
        QQmlApplicationEngine engine;
        const QQuickWindow *window = openWindow(&engine);
        QVERIFY(window != nullptr);
        QObject *settings = window->findChild<QObject *>(u"gameSettings"_s);
        const QObject *translation = window->findChild<QObject *>(u"translation"_s);
        QVERIFY(settings != nullptr && translation != nullptr);
        QCOMPARE(translation->property("language").toString(), u"en"_s);
        QVERIFY(settings->setProperty("language", 2)); // German
        QCOMPARE(translation->property("language").toString(), u"de"_s);
        QVERIFY(showsText(window, u"Kampagne"_s));
        QVERIFY(settings->setProperty("language", 1)); // English
        QCOMPARE(translation->property("language").toString(), u"en"_s);
        QVERIFY(showsText(window, u"Campaign"_s));
        QVERIFY(!showsText(window, u"Kampagne"_s));
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

    // Towers hold the first round, without them the second takes the lives. The rounds of the
    // lost game are kept and shown as the best of the next one (E-36).
    static void anEndlessGameKeepsItsRounds()
    {
        QFile::remove(progressFile());
        QQmlApplicationEngine engine;
        qtd::client::GameSession *game = openGame(&engine);
        QVERIFY(game != nullptr);
        const auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        const QObject *hud = window->findChild<QObject *>(u"hud"_s);
        QVERIFY(hud != nullptr);
        QVERIFY(playEndless(window, game));
        QCOMPARE(hud->property("best").toInt(), 0);
        for (const QPoint cell : {QPoint(4, 4), QPoint(10, 6)}) {
            QVERIFY(game->chooseTowerAt(cell.x(), cell.y()) && game->sellChosenTower());
        }
        while (!game->lost()) {
            game->advanceTime(1'000'000);
        }
        const int rounds = game->round();
        QCOMPARE(sectionOf(window, 0)[u"best"_s].toInt(), rounds);
        game->restart();
        QVERIFY(game->endless());
        QCOMPARE(hud->property("best").toInt(), rounds);
    }

    // Who leaves an endless game for the world map keeps its rounds as well.
    static void aLeftEndlessGameKeepsItsRounds()
    {
        QFile::remove(progressFile());
        QQmlApplicationEngine engine;
        qtd::client::GameSession *game = openGame(&engine);
        QVERIFY(game != nullptr);
        const auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        QObject *hud = window->findChild<QObject *>(u"hud"_s);
        QVERIFY(hud != nullptr);
        QVERIFY(playEndless(window, game));
        QCOMPARE(sectionOf(window, 0)[u"best"_s].toInt(), 0);
        QVERIFY(QMetaObject::invokeMethod(hud, "leave"));
        QCOMPARE(sectionOf(window, 0)[u"best"_s].toInt(), 1);
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
    QStandardPaths::setTestModeEnabled(true);       // the progress of a player stays as it is
    QLocale::setDefault(QLocale(QLocale::English)); // the same texts on every machine
    TestWindow test;
    return QTest::qExec(&test, argc, argv);
}
#include "tst_window.moc"
