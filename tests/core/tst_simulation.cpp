#include "core/simulation.h"
#include "scenario.h"

#include <QTest>

#include <algorithm>
#include <iterator>
#include <vector>

using qtd::core::BuildTowerCommand;
using qtd::core::CommandResult;
using qtd::core::Content;
using qtd::core::Fixed;
using qtd::core::GameStatus;
using qtd::core::kTicksPerSecond;
using qtd::core::loadContentFile;
using qtd::core::Simulation;
using qtd::tests::contentOf;
using qtd::tests::editedJson;
using qtd::tests::required;
using qtd::tests::scenarioJson;

namespace {

static_assert(kTicksPerSecond == 30);

constexpr quint64 kSeed = 42;
constexpr qint64 kManyTicks = 100'000;

Simulation scenarioGame()
{
    return Simulation(contentOf(scenarioJson()), kSeed);
}

Content shippedContent()
{
    return required(
        loadContentFile(QStringLiteral(QTD_CONTENT_DIR "/training_ground.json")).content);
}

/// A whole game with a fixed list of commands. Gives the hash after every tick.
std::vector<quint64> playShippedGame(quint64 seed, std::vector<Fixed> *healthTrace = nullptr)
{
    Simulation game(shippedContent(), seed);
    std::vector<quint64> hashes{game.hash()};
    game.execute(BuildTowerCommand{0, 0});
    while (game.state().status == GameStatus::Running) {
        if (game.state().tick == 200) {
            game.execute(BuildTowerCommand{2, 0});
        }
        game.tick();
        hashes.push_back(game.hash());
        if (healthTrace != nullptr) {
            std::ranges::transform(
                game.state().enemies, std::back_inserter(*healthTrace), &qtd::core::Enemy::health);
        }
    }
    return hashes;
}

} // namespace

class TestSimulation : public QObject
{
    Q_OBJECT

private slots:
    static void startsFromTheContent()
    {
        const Simulation game = scenarioGame();
        QCOMPARE(game.state().tick, 0);
        QCOMPARE(game.state().gold, 100);
        QCOMPARE(game.state().lives, 3);
        QVERIFY(game.state().status == GameStatus::Running);
        QVERIFY(game.state().enemies.empty());
        QVERIFY(game.state().towers.empty());
    }

    static void enemiesWalkThePath()
    {
        Simulation game = scenarioGame();
        game.tick(); // spawns at the start and does not walk yet
        QCOMPARE(game.state().enemies.size(), 1U);
        QCOMPARE(game.state().enemies[0].travelled.raw(), 0);
        game.advance(4); // tick 5: x = 2, the second enemy comes
        QCOMPARE(game.state().tick, 5);
        QCOMPARE(game.state().enemies.size(), 2U);
        QCOMPARE(game.state().enemies[0].id, 1);
        QCOMPARE(game.state().enemies[1].id, 2);
        QCOMPARE(game.position(game.state().enemies[0]).x.raw(), 20'000);
        QCOMPARE(game.position(game.state().enemies[1]).x.raw(), 0);
    }

    // Group 1 ends at tick 5. Two ticks of delay, so the third enemy comes at tick 8.
    static void groupsFollowEachOther()
    {
        const char *group =
            R"({ "enemy": "grunt", "count": 2, "delayTicks": 0, "intervalTicks": 4 })";
        const QByteArray two =
            QByteArray(group) +
            R"(, { "enemy": "grunt", "count": 1, "delayTicks": 2, "intervalTicks": 1 })";
        Simulation game(contentOf(editedJson(group, two)), kSeed);
        game.advance(7);
        QCOMPARE(game.state().enemies.size(), 2U);
        game.tick();
        QCOMPARE(game.state().enemies.size(), 3U);
    }

    // No tower: enemy 1 reaches x = 10 at tick 21, enemy 2 at tick 25.
    static void lostWhenTheLivesRunOut()
    {
        Simulation game(contentOf(editedJson(R"("startLives": 3)", R"("startLives": 2)")), kSeed);
        game.advance(20);
        QCOMPARE(game.state().lives, 2);
        game.tick();
        QCOMPARE(game.state().lives, 1);
        QCOMPARE(game.state().enemies.size(), 1U);
        game.advance(kManyTicks);
        QVERIFY(game.state().status == GameStatus::Lost);
        QCOMPARE(game.state().tick, 25);
        QCOMPARE(game.state().lives, 0);
        QCOMPARE(game.state().gold, 100);
    }

    static void livesDoNotGoBelowZero()
    {
        Simulation game(contentOf(editedJson(R"("lifeCost": 1)", R"("lifeCost": 5)")), kSeed);
        game.advance(kManyTicks);
        QVERIFY(game.state().status == GameStatus::Lost);
        QCOMPARE(game.state().tick, 21);
        QCOMPARE(game.state().lives, 0);
    }

    static void wonWhenSomeLivesAreLeft()
    {
        Simulation game = scenarioGame();
        game.advance(kManyTicks);
        QVERIFY(game.state().status == GameStatus::Won);
        QCOMPARE(game.state().tick, 25);
        QCOMPARE(game.state().lives, 1);
    }

    // The tower at (5, 1) reaches x >= 2.17, range 3: enemy 1 at tick 6 (x = 2.5). Shots every four
    // ticks: tick 6 and 10 at enemy 1, tick 14 and 18 at enemy 2, two damage each, four health.
    static void towerDefendsAndWins()
    {
        Simulation game = scenarioGame();
        QVERIFY(game.execute(BuildTowerCommand{0, 0}) == CommandResult::Accepted);
        QCOMPARE(game.state().gold, 60);
        game.advance(5);
        QCOMPARE(game.state().enemies[0].health.raw(), 40'000);
        game.tick();
        QCOMPARE(game.state().enemies[0].health.raw(), 20'000);
        QCOMPARE(game.state().towers[0].cooldownTicks, 4);
        game.advance(3);
        QCOMPARE(game.state().enemies[0].health.raw(), 20'000);
        game.tick();
        QCOMPARE(game.state().enemies.size(), 1U); // tick 10: the leading enemy is gone
        QCOMPARE(game.state().enemies[0].id, 3);
        QCOMPARE(game.state().enemies[0].health.raw(), 40'000);
        QCOMPARE(game.state().gold, 67);
        game.advance(kManyTicks);
        QVERIFY(game.state().status == GameStatus::Won);
        QCOMPARE(game.state().tick, 18);
        QCOMPARE(game.state().gold, 74);
        QCOMPARE(game.state().lives, 3);
    }

    // A fast enemy catches up with a slow one at tick 5, x = 2. Both are the same distance from the
    // tower, which must pick the lower id.
    static void equalProgressGoesToTheLowerId()
    {
        const QByteArray enemies = R"("enemies": [{
            "id": "runner", "health": "4", "speedPerTick": "1", "bounty": 1, "lifeCost": 1 }, {)";
        const char *group =
            R"({ "enemy": "grunt", "count": 2, "delayTicks": 0, "intervalTicks": 4 })";
        const QByteArray wave =
            R"({ "enemy": "grunt", "count": 1, "delayTicks": 0, "intervalTicks": 1 },
            { "enemy": "runner", "count": 1, "delayTicks": 1, "intervalTicks": 1 })";
        QByteArray json = editedJson(group, wave);
        json.replace(R"("enemies": [{)", enemies);
        Simulation game(contentOf(json), kSeed);
        game.advance(4);
        game.execute(BuildTowerCommand{1, 0});
        game.tick();
        const auto &state = game.state();
        QCOMPARE(state.enemies[0].travelled.raw(), state.enemies[1].travelled.raw());
        QCOMPARE(state.enemies[0].health.raw(), 20'000);
        QCOMPARE(state.enemies[1].health.raw(), 40'000);
    }

    static void rangeIncludesItsEdge_data()
    {
        QTest::addColumn<QByteArray>("range");
        QTest::addColumn<int>("healthAfterTickOne");
        QTest::newRow("on the edge") << QByteArray(R"("range": "5")") << 20'000;
        QTest::newRow("just short") << QByteArray(R"("range": "4.9999")") << 40'000;
    }

    // The enemy spawns at x = 0 in tick 1, and towers fire after spawning, so a tower can hit it in
    // that same tick. A tower at (4, 3) stands exactly 5 tiles away (a 3-4-5 triangle).
    static void rangeIncludesItsEdge()
    {
        QFETCH(const QByteArray, range);
        QFETCH(const int, healthAfterTickOne);
        QByteArray json = editedJson(R"({ "x": 5, "y": 1 })", R"({ "x": 4, "y": 3 })");
        json.replace(R"("range": "3")", range);
        Simulation game(contentOf(json), kSeed);
        game.execute(BuildTowerCommand{0, 0});
        game.tick();
        QCOMPARE(game.state().enemies.size(), std::size_t{1});
        QCOMPARE(game.state().enemies[0].health.raw(), healthAfterTickOne);
    }

    // Both towers reach the enemy at tick 8 (x = 3.5). The first one kills it, the second must not
    // shoot the corpse: one bounty, no cooldown.
    static void deadEnemiesAreNoTargets()
    {
        QByteArray json = editedJson(R"("health": "4")", R"("health": "2")");
        json.replace(R"("count": 2)", R"("count": 1)");
        Simulation game(contentOf(json), kSeed);
        game.advance(7);
        game.execute(BuildTowerCommand{0, 0});
        game.execute(BuildTowerCommand{1, 0});
        game.tick();
        QVERIFY(game.state().status == GameStatus::Won);
        QCOMPARE(game.state().gold, 27);
        QCOMPARE(game.state().towers[0].cooldownTicks, 4);
        QCOMPARE(game.state().towers[1].cooldownTicks, 0);
    }

    // Seed 42, stream 54 and a spread of 20000 raw: the Python copy of PCG32 rolls 6432 and 9687.
    static void damageComesFromTheGenerator()
    {
        Simulation game(contentOf(editedJson(R"("damageMin": "2",
        "damageMax": "2")",
                                             R"("damageMin": "1",
        "damageMax": "3")")),
                        kSeed);
        game.execute(BuildTowerCommand{0, 0});
        game.advance(6);
        QCOMPARE(game.state().enemies[0].health.raw(), 40'000 - 16'432);
        game.advance(4);
        QCOMPARE(game.state().enemies[0].health.raw(), 40'000 - 16'432 - 19'687);
    }

    static void invalidCommandsChangeNothing_data()
    {
        QTest::addColumn<qint64>("slot");
        QTest::addColumn<qint64>("towerType");
        QTest::addColumn<CommandResult>("result");
        QTest::newRow("slot below") << qint64(-1) << qint64(0) << CommandResult::UnknownSlot;
        QTest::newRow("slot above") << qint64(3) << qint64(0) << CommandResult::UnknownSlot;
        QTest::newRow("type below") << qint64(1) << qint64(-1) << CommandResult::UnknownTowerType;
        QTest::newRow("type above") << qint64(1) << qint64(1) << CommandResult::UnknownTowerType;
        QTest::newRow("occupied") << qint64(0) << qint64(0) << CommandResult::SlotOccupied;
    }

    static void invalidCommandsChangeNothing()
    {
        QFETCH(const qint64, slot);
        QFETCH(const qint64, towerType);
        QFETCH(const CommandResult, result);
        Simulation game = scenarioGame();
        game.execute(BuildTowerCommand{0, 0});
        game.advance(2);
        const quint64 before = game.hash();
        QVERIFY(game.execute(BuildTowerCommand{slot, towerType}) == result);
        QCOMPARE(game.hash(), before);
        QCOMPARE(game.state().gold, 60);
        QCOMPARE(game.state().towers.size(), 1U);
    }

    static void towersCostGold()
    {
        Simulation game = scenarioGame();
        QVERIFY(game.execute(BuildTowerCommand{0, 0}) == CommandResult::Accepted);
        QVERIFY(game.execute(BuildTowerCommand{1, 0}) == CommandResult::Accepted);
        QCOMPARE(game.state().gold, 20);
        const quint64 before = game.hash();
        QVERIFY(game.execute(BuildTowerCommand{2, 0}) == CommandResult::NotEnoughGold);
        QCOMPARE(game.hash(), before);
    }

    static void theExactPriceIsEnough()
    {
        Simulation game(contentOf(editedJson(R"("startGold": 100)", R"("startGold": 40)")), kSeed);
        QVERIFY(game.execute(BuildTowerCommand{0, 0}) == CommandResult::Accepted);
        QCOMPARE(game.state().gold, 0);
    }

    static void nothingHappensAfterTheEnd()
    {
        Simulation game = scenarioGame();
        game.advance(kManyTicks);
        QVERIFY(game.state().status != GameStatus::Running);
        const quint64 before = game.hash();
        game.tick();
        QVERIFY(game.execute(BuildTowerCommand{0, 0}) == CommandResult::GameOver);
        QCOMPARE(game.hash(), before);
    }

    static void sameSeedGivesTheSameGame()
    {
        const std::vector<quint64> first = playShippedGame(kSeed);
        const std::vector<quint64> second = playShippedGame(kSeed);
        QVERIFY(first.size() > 100);
        QVERIFY(first == second);
    }

    static void everyTickChangesTheHash()
    {
        const std::vector<quint64> hashes = playShippedGame(kSeed);
        for (std::size_t tick = 1; tick < hashes.size(); ++tick) {
            QVERIFY(hashes[tick] != hashes[tick - 1]);
        }
    }

    static void otherSeedsGiveOtherGames()
    {
        std::vector<Fixed> first;
        std::vector<Fixed> second;
        const std::vector<quint64> hashes = playShippedGame(1, &first);
        const std::vector<quint64> others = playShippedGame(2, &second);
        QVERIFY(hashes[0] != others[0]);
        QVERIFY(first != second); // not only the generator: the fights differ as well
    }

    static void pacingDoesNotMatter()
    {
        Simulation inSteps = scenarioGame();
        Simulation atOnce = scenarioGame();
        inSteps.execute(BuildTowerCommand{0, 0});
        atOnce.execute(BuildTowerCommand{0, 0});
        for (int tick = 0; tick < 12; ++tick) {
            inSteps.tick();
        }
        atOnce.advance(12);
        QCOMPARE(inSteps.hash(), atOnce.hash());
    }

    static void aCopyGoesOnTheSameWay()
    {
        Simulation game(shippedContent(), kSeed);
        game.execute(BuildTowerCommand{0, 0});
        game.advance(150);
        Simulation copy = game;
        game.advance(150);
        copy.advance(150);
        QCOMPARE(copy.hash(), game.hash());
    }
};

QTEST_APPLESS_MAIN(TestSimulation)
#include "tst_simulation.moc"
