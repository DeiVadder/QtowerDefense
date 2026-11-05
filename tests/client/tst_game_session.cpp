#include "client/enemy_instances.h"
#include "client/game_session.h"

#include <QDir>
#include <QFile>
#include <QPointF>
#include <QSignalSpy>
#include <QSizeF>
#include <QTest>

#include <cstring>

using qtd::client::EnemyInstances;
using qtd::client::GameSession;

using namespace Qt::StringLiterals;

namespace {

constexpr qint64 kTickMicroseconds = 33'334; // a little more than a tick at 30 Hz

QString mapFile()
{
    return QDir(QStringLiteral(QTD_CONTENT_DIR)).filePath(u"training_ground.json"_s);
}

bool near(QPointF first, QPointF second)
{
    return (first - second).manhattanLength() < 0.002;
}

void runTicks(GameSession &game, qint64 ticks)
{
    const qint64 target = game.tick() + ticks;
    while (game.tick() < target && !game.won() && !game.lost()) {
        game.advanceTime(kTickMicroseconds);
    }
}

/// Reaches the protected buffer of the instancing.
class OpenInstances : public EnemyInstances
{
public:
    int count()
    {
        int instances = -1;
        getInstanceBuffer(&instances);
        return instances;
    }

    InstanceTableEntry first()
    {
        int instances = 0;
        const QByteArray buffer = getInstanceBuffer(&instances);
        InstanceTableEntry entry{};
        std::memcpy(&entry, buffer.constData(), sizeof(entry));
        return entry;
    }
};

} // namespace

class GameSessionTest : public QObject
{
    Q_OBJECT

private slots:
    void aMapShowsItsPlaces()
    {
        GameSession game;
        game.startGame(mapFile(), 7);
        QCOMPARE(game.error(), QString());
        QCOMPARE(game.gold(), 120);
        QCOMPARE(game.lives(), 5);
        QCOMPARE(game.towerCost(), 50);
        QCOMPARE(game.mapSize(), QSizeF(16, 10));
        QCOMPARE(game.path().size(), 4);
        QCOMPARE(game.path().at(1).toPointF(), QPointF(8, 2));
        QCOMPARE(game.buildSlots().size(), 4);
        QCOMPARE(game.buildSlots().at(2).toPointF(), QPointF(10, 6));
    }

    void theMapIsPartOfTheProgram()
    {
        QVERIFY(QFile::exists(u":/content/training_ground.json"_s));
        GameSession game;
        game.startNewGame(u":/content/training_ground.json"_s);
        QCOMPARE(game.error(), QString());
        QCOMPARE(game.gold(), 120);
    }

    void aBrokenMapLeavesAnErrorAndNoGame()
    {
        GameSession game;
        game.startGame(mapFile(), 7);
        game.startGame(u"none.json"_s, 7);
        QVERIFY(game.error().contains(u"none.json"_s));
        QCOMPARE(game.gold(), 0);
        QVERIFY(game.path().isEmpty());
        QCOMPARE(game.mapSize(), QSizeF());
        QVERIFY(!game.buildTower(0));
        game.advanceTime(kTickMicroseconds);
        QCOMPARE(game.tick(), 0);
        QVERIFY(!game.won());
        QVERIFY(!game.lost());
    }

    void aTowerCostsGoldAndStands()
    {
        GameSession game;
        game.startGame(mapFile(), 7);
        const QSignalSpy towers(&game, &GameSession::towersChanged);
        QVERIFY(game.buildTower(2));
        QCOMPARE(towers.size(), 1);
        QCOMPARE(game.gold(), 70);
        QCOMPARE(game.towerPlaces().size(), 1);
        QCOMPARE(game.towerPlaces().at(0).toPointF(), QPointF(10, 6));
    }

    void aRejectedCommandChangesNothing()
    {
        GameSession game;
        game.startGame(mapFile(), 7);
        QVERIFY(game.buildTower(0));
        const QSignalSpy towers(&game, &GameSession::towersChanged);
        QVERIFY(!game.buildTower(0));
        QVERIFY(!game.buildTower(99));
        QVERIFY(!game.buildTower(-1));
        QCOMPARE(towers.size(), 0);
        QCOMPARE(game.gold(), 70);
    }

    void timeBecomesTicks()
    {
        GameSession game;
        game.startGame(mapFile(), 7);
        const QSignalSpy state(&game, &GameSession::stateChanged);
        game.advanceTime(100'000);
        QCOMPARE(game.tick(), 3);
        QCOMPARE(state.size(), 1);
        game.advanceTime(1); // not yet a tick, nothing to tell
        QCOMPARE(state.size(), 1);
    }

    void aLongFrameDoesNotJump()
    {
        GameSession game;
        game.startGame(mapFile(), 7);
        game.advanceTime(60'000'000);
        QCOMPARE(game.tick(), 7); // 250 ms
    }

    void negativeTimeIsNoTime()
    {
        GameSession game;
        game.startGame(mapFile(), 7);
        game.advanceTime(-1'000'000);
        QCOMPARE(game.tick(), 0);
    }

    void aPausedGameStands()
    {
        GameSession game;
        game.startGame(mapFile(), 7);
        game.setPaused(true);
        game.advanceTime(100'000);
        QCOMPARE(game.tick(), 0);
        game.setPaused(false);
        game.advanceTime(100'000);
        QCOMPARE(game.tick(), 3);
    }

    void aGameInTheBackgroundPauses()
    {
        GameSession game;
        game.applicationStateChanged(Qt::ApplicationInactive); // only another window has the focus
        QVERIFY(!game.paused());
        game.applicationStateChanged(Qt::ApplicationHidden);
        QVERIFY(game.paused());
        game.setPaused(false);
        game.applicationStateChanged(Qt::ApplicationSuspended);
        QVERIFY(game.paused());
        game.applicationStateChanged(Qt::ApplicationActive); // the player decides when it goes on
        QVERIFY(game.paused());
    }

    void speedRunsMoreTicks()
    {
        GameSession game;
        game.startGame(mapFile(), 7);
        game.setSpeed(3);
        game.advanceTime(100'000);
        QCOMPARE(game.tick(), 9);
    }

    void speedStaysInItsRange()
    {
        GameSession game;
        game.setSpeed(0);
        QCOMPARE(game.speed(), 1);
        game.setSpeed(99);
        QCOMPARE(game.speed(), game.maxSpeed());
    }

    void aNewGameKeepsTheSpeedAndRuns()
    {
        GameSession game;
        game.setSpeed(2);
        game.startGame(mapFile(), 7);
        game.setPaused(true);
        game.startGame(mapFile(), 7);
        QVERIFY(!game.paused());
        game.advanceTime(100'000);
        QCOMPARE(game.tick(), 6);
    }

    void enemiesWalkTheMap()
    {
        GameSession game;
        game.startGame(mapFile(), 7);
        runTicks(game, 61);
        QCOMPARE(game.enemyCount(), 1);
        QCOMPARE(game.enemyPlaces().front(), QPointF(0, 2)); // new: nothing to come from
        runTicks(game, 20);
        QVERIFY(near(game.enemyPlaces().front(), QPointF(0.95, 2))); // a tick behind: 19 x 0.05
    }

    void enemiesMoveBetweenTicks()
    {
        GameSession game;
        game.startGame(mapFile(), 7);
        runTicks(game, 81);
        const QSignalSpy frames(&game, &GameSession::frameChanged);
        const qint64 tick = game.tick();
        game.advanceTime(16'667); // half a tick
        QCOMPARE(game.tick(), tick);
        QCOMPARE(frames.size(), 1);
        QVERIFY(near(game.enemyPlaces().front(), QPointF(0.975, 2)));
    }

    void aPausedEnemyStandsStill()
    {
        GameSession game;
        game.startGame(mapFile(), 7);
        runTicks(game, 81);
        game.advanceTime(16'667);
        game.setPaused(true);
        const QPointF place = game.enemyPlaces().front();
        game.advanceTime(16'667);
        QCOMPARE(game.enemyPlaces().front(), place);
    }

    void severalTicksInAFrameComeFromTheLastOne()
    {
        GameSession game;
        game.startGame(mapFile(), 7);
        runTicks(game, 71);
        game.advanceTime(100'000); // three ticks, tick 74 is shown from tick 73
        QVERIFY(near(game.enemyPlaces().front(), QPointF(0.6, 2)));
    }

    void anUndefendedMapIsLost()
    {
        GameSession game;
        game.startGame(mapFile(), 7);
        runTicks(game, 100'000);
        QVERIFY(game.lost());
        QVERIFY(!game.won());
    }

    void aDefendedMapIsWonAndARestartBeginsAnew()
    {
        GameSession game;
        game.startGame(mapFile(), 7);
        QVERIFY(game.buildTower(0));
        QVERIFY(game.buildTower(2));
        runTicks(game, 100'000);
        QVERIFY(game.won());
        game.restart();
        QVERIFY(!game.won());
        QCOMPARE(game.tick(), 0);
        QCOMPARE(game.gold(), 120);
        QVERIFY(game.towerPlaces().isEmpty());
    }

    void instancesFollowTheEnemies()
    {
        GameSession game;
        game.startGame(mapFile(), 7);
        OpenInstances instances;
        QCOMPARE(instances.count(), 0);
        instances.setGame(&game);
        instances.setFieldSize(50);
        instances.setEnemyScale(0.25F);
        QCOMPARE(instances.count(), 0);
        runTicks(game, 61);
        QCOMPARE(instances.count(), 1);
        const QQuick3DInstancing::InstanceTableEntry entry = instances.first();
        QCOMPARE(entry.getPosition(), QVector3D(0, 0, 100));
        QCOMPARE(entry.getScale(), QVector3D(0.25F, 0.25F, 0.25F));
    }
};

QTEST_GUILESS_MAIN(GameSessionTest)
#include "tst_game_session.moc"
