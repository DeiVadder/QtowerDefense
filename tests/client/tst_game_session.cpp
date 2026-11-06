#include "client/game_session.h"
#include "client/map_instances.h"

#include <QDir>
#include <QFile>
#include <QPointF>
#include <QSignalSpy>
#include <QSizeF>
#include <QTemporaryDir>
#include <QTest>

#include <cstddef>
#include <cstring>
#include <vector>

using qtd::client::GameSession;
using qtd::client::MapInstances;

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
class OpenInstances : public MapInstances
{
public:
    int count()
    {
        int instances = -1;
        getInstanceBuffer(&instances);
        return instances;
    }

    InstanceTableEntry entry(int index)
    {
        int instances = 0;
        const QByteArray buffer = getInstanceBuffer(&instances);
        InstanceTableEntry found{};
        std::memcpy(&found, buffer.constData() + index * qsizetype{sizeof(found)}, sizeof(found));
        return found;
    }

    InstanceTableEntry first()
    {
        return entry(0);
    }
};

/// Where the first boss walks among the enemies, -1 if none does.
int indexOfBoss(const qtd::core::Simulation &game)
{
    const std::vector<qtd::core::Enemy> &enemies = game.state().enemies;
    for (std::size_t index = 0; index < enemies.size(); ++index) {
        if (game.content().enemies[enemies[index].type].boss) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

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
        QCOMPARE(game.gridSize(), QSize(17, 11));
        QCOMPARE(game.towerTypes().size(), 7);
        const QVariantMap first = game.towerTypes().at(0).toMap();
        QCOMPARE(first[u"name"_s].toString(), u"ballista"_s);
        QCOMPARE(first[u"cost"_s].toLongLong(), 50);
        QVERIFY(game.grid() != nullptr);
        QVERIFY(game.grid()->terrainAt({8, 2}) == qtd::core::Terrain::Path);
    }

    void theMapIsPartOfTheProgram()
    {
        QVERIFY(QFile::exists(u":/content/training_ground.json"_s));
        QVERIFY(QFile::exists(u":/content/mire_siege.json"_s));
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
        QVERIFY(game.grid() == nullptr);
        QVERIFY(game.towerTypes().isEmpty());
        QCOMPARE(game.gridSize(), QSize());
        QVERIFY(!game.buildTower(4, 4));
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
        QVERIFY(game.buildTower(10, 6));
        QCOMPARE(towers.size(), 1);
        QCOMPARE(game.gold(), 70);
        QCOMPARE(game.towerCount(), 1);
        QCOMPARE(game.towerCells(), QVariantList{QPoint(10, 6)});
    }

    void theSelectedTowerIsBuiltWithItsShape()
    {
        GameSession game;
        game.startGame(mapFile(), 7);
        const QSignalSpy selection(&game, &GameSession::selectionChanged);
        game.setSelectedTower(1);
        QCOMPARE(selection.size(), 1);
        QVERIFY(game.buildTower(10, 6));
        QCOMPARE(game.gold(), 30);
        QCOMPARE(game.towerCells(), (QVariantList{QPoint(10, 6), QPoint(11, 6)}));
        game.setSelectedTower(99);
        QCOMPARE(game.selectedTower(), 6);
        game.setSelectedTower(-1);
        QCOMPARE(game.selectedTower(), 0);
        game.setSelectedTower(2);
        game.restart();
        QCOMPARE(game.selectedTower(), 0);
    }

    void aTowerIsBuiltAsItIsTurned()
    {
        GameSession game;
        game.startGame(mapFile(), 7);
        const QSignalSpy selection(&game, &GameSession::selectionChanged);
        game.rotateSelection();
        game.rotateSelection();
        QCOMPARE(selection.size(), 2);
        game.setSelectedTower(1); // the turn stays with another type
        QVERIFY(game.buildTower(10, 6));
        QCOMPARE(game.towerCells(), (QVariantList{QPoint(10, 6), QPoint(9, 6)}));
        game.restart();
        game.setSelectedTower(1);
        QVERIFY(game.buildTower(10, 6));
        QCOMPARE(game.towerCells(), (QVariantList{QPoint(10, 6), QPoint(11, 6)}));
    }

    // The wide tower: from its anchor one cell to the right, after a half turn one to the left.
    void theShapeShowsHowATowerWillStand()
    {
        GameSession game;
        QVERIFY(game.selectedShape().isEmpty());
        game.startGame(mapFile(), 7);
        game.setSelectedTower(1);
        const auto cell = [&game](qsizetype index) {
            return game.selectedShape().at(index).toMap();
        };
        QCOMPARE(game.selectedShape().size(), 2);
        QCOMPARE(cell(0)[u"x"_s].toInt(), 0);
        QVERIFY(cell(0)[u"anchor"_s].toBool());
        QCOMPARE(cell(1)[u"x"_s].toInt(), 1);
        QVERIFY(!cell(1)[u"anchor"_s].toBool());
        game.rotateSelection();
        QCOMPARE(cell(1)[u"x"_s].toInt(), 0);
        QCOMPARE(cell(1)[u"y"_s].toInt(), 1);
        game.rotateSelection();
        QCOMPARE(cell(0)[u"x"_s].toInt(), 1);
        QVERIFY(cell(0)[u"anchor"_s].toBool());
        QCOMPARE(cell(1)[u"x"_s].toInt(), 0);
    }

    void aChosenTowerTakesAPriority()
    {
        GameSession game;
        QVERIFY(!game.chooseTowerAt(4, 4));
        game.setChosenPriority(1); // no game, nothing happens
        game.startGame(mapFile(), 7);
        QCOMPARE(game.chosenPriority(), -1);
        game.setSelectedTower(1);
        QVERIFY(game.buildTower(4, 4));
        QVERIFY(!game.chooseTowerAt(6, 4));
        game.setChosenPriority(1);         // nothing chosen
        QVERIFY(game.chooseTowerAt(5, 4)); // the second cell of the tower
        QCOMPARE(game.chosenPriority(), 0);
        const QSignalSpy towers(&game, &GameSession::towersChanged);
        game.setChosenPriority(2);
        QCOMPARE(towers.size(), 1);
        QCOMPARE(game.chosenPriority(), 2);
        QVERIFY(game.recorder()->simulation().state().towers.front().priority ==
                qtd::core::TargetPriority::Strongest);
        game.setChosenPriority(4);
        game.setChosenPriority(-1);
        QCOMPARE(game.chosenPriority(), 2);
        QCOMPARE(game.recorder()->replay().commands.size(), 2U);
        game.clearChosenTower();
        QCOMPARE(game.chosenPriority(), -1);
        QVERIFY(game.chooseTowerAt(4, 4));
        game.restart();
        QCOMPARE(game.chosenPriority(), -1);
    }

    void aChosenTowerIsUpgradedAndSold()
    {
        GameSession game;
        QVERIFY(!game.upgradeChosenTower()); // no game
        QVERIFY(!game.sellChosenTower());
        QCOMPARE(game.chosenUpgradeCost(), -1);
        QCOMPARE(game.chosenSellValue(), -1);
        game.startGame(mapFile(), 7);
        QVERIFY(game.buildTower(4, 4));
        QVERIFY(!game.upgradeChosenTower()); // nothing chosen
        QVERIFY(game.chooseTowerAt(4, 4));
        QCOMPARE(game.chosenUpgradeCost(), 40);
        QCOMPARE(game.chosenSellValue(), 35);
        const QSignalSpy towers(&game, &GameSession::towersChanged);
        const QSignalSpy state(&game, &GameSession::stateChanged);
        QVERIFY(game.upgradeChosenTower());
        QCOMPARE(towers.size(), 1);
        QCOMPARE(state.size(), 1);
        QCOMPARE(game.gold(), 120 - 50 - 40);
        QCOMPARE(game.chosenUpgradeCost(), 70);
        QCOMPARE(game.chosenSellValue(), 63);
        QVERIFY(!game.upgradeChosenTower()); // 30 gold are not enough
        QCOMPARE(game.gold(), 30);
        QVERIFY(game.sellChosenTower());
        QCOMPARE(game.gold(), 30 + 63);
        QCOMPARE(game.towerCount(), 0);
        QCOMPARE(game.chosenTower(), 0);
        QCOMPARE(game.chosenSellValue(), -1);
        QVERIFY(!game.sellChosenTower());
    }

    void aSelectedSpellIsCastOnce()
    {
        GameSession game;
        game.selectSpell(0); // no game, nothing happens
        QCOMPARE(game.selectedSpell(), -1);
        QVERIFY(game.spellNames().isEmpty());
        QVERIFY(game.spellSeconds().isEmpty());
        QVERIFY(!game.castSpellAt(3, 2));
        game.startGame(mapFile(), 7);
        QCOMPARE(game.spellNames(), QStringList({u"meteor"_s, u"deep_freeze"_s}));
        QVERIFY(!game.castSpellAt(3, 2)); // nothing selected
        game.selectSpell(2);
        game.selectSpell(-1);
        QCOMPARE(game.selectedSpell(), -1);
        game.selectSpell(1);
        QCOMPARE(game.selectedSpell(), 1);
        game.selectSpell(1); // the same again takes it back
        QCOMPARE(game.selectedSpell(), -1);
        game.selectSpell(0);
        QVERIFY(!game.castSpellAt(99, 2)); // outside: the spell stays selected
        QCOMPARE(game.selectedSpell(), 0);
        const QSignalSpy state(&game, &GameSession::stateChanged);
        QVERIFY(game.castSpellAt(3, 2));
        QCOMPARE(state.size(), 1);
        QCOMPARE(game.selectedSpell(), -1);
        QCOMPARE(game.spellSeconds().at(0).toLongLong(), 20);
        QCOMPARE(game.spellSeconds().at(1).toLongLong(), 0);
        game.selectSpell(0); // not ready
        QCOMPARE(game.selectedSpell(), -1);
    }

    void cooldownsAreRoundedUpToSeconds()
    {
        GameSession game;
        game.startGame(mapFile(), 7);
        game.selectSpell(0);
        QVERIFY(game.castSpellAt(3, 2));
        game.recorder()->advance(569);
        QCOMPARE(game.spellSeconds().at(0).toLongLong(), 2);
        game.recorder()->advance(1);
        QCOMPARE(game.spellSeconds().at(0).toLongLong(), 1);
    }

    void aTowerOrARestartTakesTheSpellBack()
    {
        GameSession game;
        game.startGame(mapFile(), 7);
        game.selectSpell(0);
        game.setSelectedTower(game.selectedTower());
        QCOMPARE(game.selectedSpell(), -1);
        game.selectSpell(1);
        game.restart();
        QCOMPARE(game.selectedSpell(), -1);
    }

    void aRejectedCommandChangesNothing()
    {
        GameSession game;
        game.startGame(mapFile(), 7);
        QVERIFY(game.buildTower(4, 4));
        const QSignalSpy towers(&game, &GameSession::towersChanged);
        QVERIFY(!game.buildTower(4, 4));
        QVERIFY(!game.buildTower(99, 99));
        QVERIFY(!game.buildTower(-1, 0));
        QVERIFY(!game.buildTower(3, 2)); // the path
        QVERIFY(!game.buildTower(2, 5)); // a rock
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
        QVERIFY(game.buildTower(4, 4));
        QVERIFY(game.buildTower(10, 6));
        runTicks(game, 100'000);
        QVERIFY(game.won());
        game.restart();
        QVERIFY(!game.won());
        QCOMPARE(game.tick(), 0);
        QCOMPARE(game.gold(), 120);
        QCOMPARE(game.towerCount(), 0);
    }

    void theBoardShowsEveryCellInTheLookOfItsTerrain()
    {
        GameSession game;
        game.startGame(mapFile(), 7);
        OpenInstances board;
        board.setLayer(MapInstances::Layer::Board);
        board.setFieldSize(50);
        board.setGame(&game);
        QCOMPARE(board.count(), 17 * 11);
        const auto at = [&board](int x, int y) {
            return board.entry(y * 17 + x);
        };
        QCOMPARE(at(16, 3).getPosition().x(), 16 * 50.0F);
        QCOMPARE(at(16, 3).getPosition().z(), 3 * 50.0F);
        QVERIFY(at(5, 5).getColor() != at(6, 5).getColor()); // the chessboard
        QCOMPARE(at(5, 5).getColor(), at(6, 6).getColor());
        const QColor open = at(5, 5).getColor();
        QVERIFY(at(3, 2).getColor() != open && at(6, 5).getColor() != at(3, 2).getColor()); // path
        QVERIFY(at(2, 9).getColor() != open);                                               // water
        QVERIFY(at(2, 5).getScale().y() > at(5, 5).getScale().y());                         // rock
        QVERIFY(at(13, 2).getScale().y() > at(2, 5).getScale().y());                        // tree
    }

    void aHigherLevelStandsTaller()
    {
        GameSession game;
        game.startGame(mapFile(), 7);
        OpenInstances towers;
        towers.setLayer(MapInstances::Layer::Towers);
        towers.setFieldSize(50);
        towers.setGame(&game);
        QVERIFY(game.buildTower(10, 6));
        QCOMPARE(towers.count(), 1);
        const float before = towers.entry(0).getScale().y();
        QVERIFY(game.chooseTowerAt(10, 6));
        QVERIFY(game.upgradeChosenTower());
        QCOMPARE(towers.count(), 1);
        QVERIFY(towers.entry(0).getScale().y() > before);
    }

    // Nobody defends the map: the wave walks on until the first boss shows.
    void aBossIsLargerThanTheRest()
    {
        GameSession game;
        QFile map(QDir(QStringLiteral(QTD_CONTENT_DIR)).filePath(u"mire_siege.json"_s));
        QVERIFY(map.open(QIODevice::ReadOnly));
        const QTemporaryDir folder;
        const QString longGame = folder.filePath(u"long_game.json"_s);
        QFile copy(longGame);
        QVERIFY(copy.open(QIODevice::WriteOnly));
        copy.write(map.readAll().replace(R"("startLives": 15)", R"("startLives": 1000)"));
        copy.close();
        game.startGame(longGame, 7);
        qtd::core::ReplayRecorder *recorder = game.recorder();
        OpenInstances enemies;
        enemies.setLayer(MapInstances::Layer::Enemies);
        enemies.setFieldSize(50);
        enemies.setEnemyScale(1);
        enemies.setGame(&game);
        const qtd::core::Simulation &simulation = recorder->simulation();
        int boss = -1;
        while (boss < 0 && simulation.state().status == qtd::core::GameStatus::Running) {
            recorder->advance(1);
            boss = indexOfBoss(simulation);
        }
        QVERIFY(boss >= 0);
        game.refresh();
        QCOMPARE(enemies.count(), static_cast<int>(simulation.state().enemies.size()));
        QVERIFY(enemies.entry(boss).getScale().y() > 1.0F);
        QVERIFY(boss > 0); // the wave leaves others before it on the path
        QCOMPARE(enemies.entry(0).getScale().y(), 1.0F);
    }

    void towersShowWithAllTheirCells()
    {
        GameSession game;
        game.startGame(mapFile(), 7);
        OpenInstances towers;
        towers.setLayer(MapInstances::Layer::Towers);
        towers.setFieldSize(50);
        towers.setGame(&game);
        QCOMPARE(towers.count(), 0);
        game.setSelectedTower(1);
        QVERIFY(game.buildTower(10, 6));
        QCOMPARE(towers.count(), 2);
        QCOMPARE(towers.entry(1).getPosition().x(), 11 * 50.0F);
        QCOMPARE(towers.entry(1).getPosition().z(), 6 * 50.0F);
        QVERIFY(towers.entry(1).getPosition().y() > 0);
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
