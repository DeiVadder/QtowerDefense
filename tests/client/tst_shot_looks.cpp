#include "client/game_session.h"
#include "client/map_instances.h"
#include "client/shot_looks.h"
#include "core/content.h"
#include "core/content_loader.h"
#include "open_instances.h"

#include <QTest>

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

using namespace Qt::StringLiterals;

using qtd::client::GameSession;
using qtd::client::MapInstances;
using qtd::client::ShotLook;
using Shot = qtd::client::MapInstances::Shot;

namespace {

constexpr float kFieldSize = 50; // OpenInstances draws 50 units to a field
constexpr int kLongEnough = 900; // ticks until the first enemy is in range of every tower below

// Training ground with gold for every tower. The towers by index: 0 ballista, 1 twin ballista,
// 2 bastion, 3 horseshoe, 4 frost spire, 5 venom spire, 6 catapult.
QString mapFile()
{
    return QStringLiteral(QTD_TEST_DIR "/shot_map.json");
}

void advance(GameSession &game, int ticks)
{
    game.recorder()->advance(ticks);
    game.refresh();
}

/// Buys a card of the type and builds it on the cell.
bool build(GameSession &game, int towerType, int x, int y)
{
    if (!game.buyCard(towerType)) {
        return false;
    }
    game.chooseCard(static_cast<int>(game.hand().size()) - 1);
    return game.buildTower(x, y);
}

/// Advances until the layer shows something, at most kLongEnough ticks. The ticks it took, or -1.
int advanceUntilShown(GameSession &game, OpenInstances &layer)
{
    for (int tick = 0; tick < kLongEnough; ++tick) {
        if (layer.count() > 0) {
            return tick;
        }
        advance(game, 1);
    }
    return -1;
}

} // namespace

class ShotLooksTest : public QObject
{
    Q_OBJECT

private slots:
    // The complaint was that every tower shot the same: no two towers of the catalog may look
    // alike.
    static void everyTowerOfTheCatalogShootsItsOwnShot()
    {
        const qtd::core::ContentLoadResult loaded = qtd::core::loadContentFile(mapFile());
        QVERIFY2(loaded.content, qPrintable(loaded.error));
        const std::vector<qtd::core::TowerType> &towers = loaded.content->towers;
        QCOMPARE(towers.size(), std::size_t{7});
        std::vector<std::pair<Shot, QRgb>> looks;
        for (const qtd::core::TowerType &tower : towers) {
            const ShotLook look = qtd::client::shotLookOf(tower);
            QVERIFY2(look.shape != Shot::Dust, qPrintable(tower.id)); // dust is no shot
            looks.emplace_back(look.shape, look.color.rgb());
        }
        std::ranges::sort(looks);
        QVERIFY(std::ranges::adjacent_find(looks) == looks.end());
        const std::vector<std::pair<Shot, QRgb>>::iterator shapesEnd =
            std::ranges::unique(looks, {}, &std::pair<Shot, QRgb>::first).begin();
        QVERIFY(std::distance(looks.begin(), shapesEnd) >= 6);
    }

    // A shot leaves its tower above the walls, not from inside them.
    static void aShotLeavesAboveTheWalls()
    {
        GameSession game;
        game.startGame(mapFile(), 7);
        OpenInstances bolts(&game, MapInstances::Layer::Shots);
        bolts.setShot(Shot::Bolt);
        QVERIFY(build(game, 0, 7, 3));
        QVERIFY(advanceUntilShown(game, bolts) >= 0);
        const QVector3D start = bolts.entry(0).getPosition();
        QVERIFY(start.distanceToPoint(QVector3D(7 * kFieldSize, start.y(), 3 * kFieldSize)) <
                kFieldSize);
        QVERIFY2(start.y() > kFieldSize, qPrintable(QString::number(start.y())));
    }

    // Each layer draws one shape: a catapult throws a boulder and no bolt, then dust rises.
    static void aBoulderLandsInDust()
    {
        GameSession game;
        game.startGame(mapFile(), 7);
        OpenInstances boulders(&game, MapInstances::Layer::Shots);
        boulders.setShot(Shot::Boulder);
        OpenInstances bolts(&game, MapInstances::Layer::Shots);
        OpenInstances dust(&game, MapInstances::Layer::Shots);
        dust.setShot(Shot::Dust);
        QVERIFY(build(game, 6, 9, 9));
        QVERIFY(advanceUntilShown(game, boulders) >= 0);
        QCOMPARE(boulders.count(), 1);
        QCOMPARE(bolts.count(), 0);
        QCOMPARE(dust.count(), 0);
        int flight = 0;
        while (boulders.count() > 0 && flight < kLongEnough) {
            advance(game, 1);
            ++flight;
        }
        QVERIFY(flight > 4); // slower than a bolt
        QCOMPARE(dust.count(), 1);
        const QColor fresh = dust.entry(0).getColor();
        advance(game, 3);
        QCOMPARE(dust.count(), 1);
        QVERIFY(dust.entry(0).getColor().alphaF() < fresh.alphaF());
    }

    // The horseshoe looses its arrows one after the other, three in the air at once.
    static void theHorseshoeLoosesAVolley()
    {
        GameSession game;
        game.startGame(mapFile(), 7);
        OpenInstances arrows(&game, MapInstances::Layer::Shots);
        arrows.setShot(Shot::Arrow);
        QVERIFY(build(game, 3, 10, 6));
        QVERIFY(advanceUntilShown(game, arrows) >= 0);
        QCOMPARE(arrows.count(), 1);
        int most = 1;
        for (int tick = 0; tick < 4; ++tick) {
            advance(game, 1);
            most = std::max(most, arrows.count());
        }
        QCOMPARE(most, 3);
    }

    // A tower that fires gives way and stands up again a few ticks later.
    static void aTowerGivesWayWhenItFires()
    {
        GameSession game;
        game.startGame(mapFile(), 7);
        OpenInstances looks(&game, MapInstances::Layer::TowerLooks);
        looks.setTypeIndex(0);
        OpenInstances bolts(&game, MapInstances::Layer::Shots);
        QVERIFY(build(game, 0, 7, 3));
        const QVector3D rest = looks.entry(0).getScale();
        QVERIFY(advanceUntilShown(game, bolts) >= 0);
        const QVector3D kicked = looks.entry(0).getScale();
        QVERIFY(kicked.y() < rest.y());
        QVERIFY(kicked.x() > rest.x());
        advance(game, 4);
        QCOMPARE(looks.entry(0).getScale(), rest);
    }
};

QTEST_GUILESS_MAIN(ShotLooksTest)
#include "tst_shot_looks.moc"
