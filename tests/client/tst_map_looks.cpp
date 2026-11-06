#include "client/game_session.h"
#include "client/map_instances.h"
#include "open_instances.h"

#include <QFile>
#include <QSignalSpy>
#include <QTest>

#include <algorithm>
#include <cmath>

using namespace Qt::StringLiterals;

using qtd::client::GameSession;
using qtd::client::MapInstances;

namespace {

QString mapFile()
{
    return QStringLiteral(QTD_CONTENT_DIR "/mire_siege.json");
}

void advance(GameSession &game, int ticks)
{
    game.recorder()->advance(ticks);
    game.refresh();
}

} // namespace

class MapLooksTest : public QObject
{
    Q_OBJECT

private slots:
    static void everyTypeOfTheGameHasAModel()
    {
        GameSession game;
        QVERIFY(game.towerModels().isEmpty());
        game.startGame(mapFile(), 7);
        QCOMPARE(game.towerModels().size(), 7);
        QVERIFY(!game.towerModels().contains(QString()));
        QCOMPARE(game.enemyModels().size(), 10);
        QVERIFY(!game.enemyModels().contains(QString()));
    }

    static void aTypeWithoutAModelFallsBack()
    {
        GameSession game;
        game.startGame(QStringLiteral(QTD_TEST_DIR "/no_models.json"), 7);
        QCOMPARE(game.error(), QString());
        QCOMPARE(game.towerModels(), QStringList{QString()});
        QCOMPARE(game.enemyModels(), QStringList{QString()});
    }

    static void obstaclesShowOnTheirCells()
    {
        GameSession game;
        game.startGame(mapFile(), 7);
        OpenInstances rocks(&game, MapInstances::Layer::Rocks);
        OpenInstances trees(&game, MapInstances::Layer::Trees);
        QCOMPARE(rocks.count(), 3);
        QCOMPARE(rocks.entry(0).getPosition(), QVector3D(2 * 50, 0, 5 * 50));
        QVERIFY(trees.count() > 3);
    }

    // A tap names its cell by the instance, so the ground below obstacles comes last.
    static void theBoardStartsWithAllItsCells()
    {
        GameSession game;
        game.startGame(mapFile(), 7);
        OpenInstances board(&game, MapInstances::Layer::Board);
        const QSize size = game.gridSize();
        QCOMPARE(board.entry(1).getPosition().x(), 50.0F);
        QCOMPARE(board.entry(size.width()).getPosition().z(), 50.0F);
        QVERIFY(board.count() > size.width() * size.height());
        const QVector3D last = board.entry(board.count() - 1).getScale();
        QVERIFY(last.y() < board.entry(5 * size.width() + 2).getScale().y());
    }

    static void towersShowWithAllTheirCells()
    {
        GameSession game;
        game.startGame(QStringLiteral(QTD_CONTENT_DIR "/training_ground.json"), 7);
        OpenInstances towers(&game, MapInstances::Layer::Towers);
        QCOMPARE(towers.count(), 0);
        game.setSelectedTower(1);
        QVERIFY(game.buildTower(10, 6));
        QCOMPARE(towers.count(), 2);
        QCOMPARE(towers.entry(1).getPosition().x(), 11 * 50.0F);
        QCOMPARE(towers.entry(1).getPosition().z(), 6 * 50.0F);
        QVERIFY(towers.entry(1).getPosition().y() > 0);
    }

    static void towerLooksGoByTypeAndGrow()
    {
        GameSession game;
        game.startGame(mapFile(), 7);
        OpenInstances first(&game, MapInstances::Layer::TowerLooks);
        OpenInstances second(&game, MapInstances::Layer::TowerLooks);
        second.setTypeIndex(1);
        QVERIFY(game.buildTower(6, 3));
        QCOMPARE(first.count(), 1);
        QCOMPARE(second.count(), 0);
        const float height = first.entry(0).getScale().y();
        QVERIFY(game.chooseTowerAt(6, 3));
        QVERIFY(game.upgradeChosenTower());
        QVERIFY(first.entry(0).getScale().y() > height);
    }

    static void aPlainShapeIsAsBigAsAModel()
    {
        GameSession game;
        game.startGame(mapFile(), 7);
        OpenInstances model(&game, MapInstances::Layer::TowerLooks);
        OpenInstances shape(&game, MapInstances::Layer::TowerLooks);
        shape.setModelSize(100);
        QVERIFY(game.buildTower(6, 3));
        QCOMPARE(shape.entry(0).getScale().x() * 100, model.entry(0).getScale().x());
        QCOMPARE(shape.entry(0).getScale().y() * 100, model.entry(0).getScale().y());
    }

    static void aNewGameRenewsTheTable()
    {
        GameSession game;
        game.startGame(mapFile(), 7);
        OpenInstances looks(&game, MapInstances::Layer::TowerLooks);
        const QSignalSpy renewed(&looks, &MapInstances::instanceTableChanged);
        QVERIFY(game.buildTower(6, 3));
        QCOMPARE(renewed.size(), 1);
        game.startGame(mapFile(), 7);
        QCOMPARE(renewed.size(), 2);
        QCOMPARE(looks.count(), 0);
    }

    // Unlike the models of a type, these do not depend on the content.
    static void obstaclesAlwaysHaveAModel()
    {
        QVERIFY(QFile::exists(u":/assets/rock.mesh"_s));
        QVERIFY(QFile::exists(u":/assets/tree.mesh"_s));
    }

    static void aWonGameIsHeardOnce()
    {
        GameSession game;
        game.startGame(QStringLiteral(QTD_TEST_DIR "/no_models.json"), 7);
        const QSignalSpy cues(&game, &GameSession::cue);
        for (int frame = 0; frame < 1000; ++frame) {
            game.advanceTime(100'000);
        }
        QVERIFY(game.won());
        QCOMPARE(cues.size(), 1);
        QCOMPARE(cues.at(0).at(0).value<GameSession::Cue>(), GameSession::Cue::Won);
    }

    static void theChosenTowerIsBrighter()
    {
        GameSession game;
        game.startGame(mapFile(), 7);
        OpenInstances looks(&game, MapInstances::Layer::TowerLooks);
        QVERIFY(game.buildTower(6, 3));
        const QColor plain = looks.entry(0).getColor();
        QVERIFY(game.chooseTowerAt(6, 3));
        QVERIFY(looks.entry(0).getColor().lightness() > plain.lightness());
    }

    // Every tower that fired shows one shot for a few ticks, on the way to an enemy.
    static void aShotFliesFromItsTower()
    {
        GameSession game;
        game.startGame(mapFile(), 7);
        OpenInstances shots(&game, MapInstances::Layer::Shots);
        QVERIFY(game.buildTower(6, 3));
        QCOMPARE(shots.count(), 0);
        int seen = 0;
        for (int tick = 0; tick < 600 && seen == 0; ++tick) {
            advance(game, 1);
            seen = shots.count();
        }
        QCOMPARE(seen, 1);
        const QVector3D start = shots.entry(0).getPosition();
        QVERIFY(start.distanceToPoint(QVector3D(6 * 50, start.y(), 3 * 50)) < 50);
        advance(game, 2);
        QVERIFY(shots.count() == 0 || shots.entry(0).getPosition().distanceToPoint(start) > 0);
        advance(game, 8);
        QCOMPARE(shots.count(), 0);
    }

    static void enemiesLookWhereTheyGoAndHop()
    {
        GameSession game;
        game.startGame(mapFile(), 7);
        OpenInstances enemies(&game, MapInstances::Layer::Enemies);
        enemies.setEnemyScale(1);
        advance(game, 80); // along the first stretch, to the right
        QCOMPARE(enemies.count(), 1);
        QCOMPARE(std::round(enemies.entry(0).getRotation().toEulerAngles().y()), 90.0F);
        float highest = 0;
        for (int tick = 0; tick < 30; ++tick) {
            advance(game, 1);
            highest = std::max(highest, enemies.entry(0).getPosition().y());
        }
        QVERIFY(highest > 0);
        QVERIFY(highest < 50.0F / 4);
    }
};

QTEST_GUILESS_MAIN(MapLooksTest)
#include "tst_map_looks.moc"
