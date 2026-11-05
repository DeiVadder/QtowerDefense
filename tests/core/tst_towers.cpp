#include "core/simulation.h"
#include "scenario.h"

#include <QByteArray>
#include <QTest>

using namespace Qt::StringLiterals;

using qtd::core::BuildTowerCommand;
using qtd::core::CommandResult;
using qtd::core::Fixed;
using qtd::core::SellTowerCommand;
using qtd::core::Simulation;
using qtd::core::UpgradeTowerCommand;
using qtd::tests::contentOf;
using qtd::tests::scenarioJson;

namespace {

constexpr qint64 kFirstTower = 1;

/// The scenario with ten health. Its tower does 2 damage every 4 ticks. A tower at (1, 1) hits the
/// first enemy at ticks 1 and 5. At tick 5 the second enemy enters, 2 tiles behind the first.
QByteArray combatJson()
{
    return scenarioJson().replace(R"("health": "4")", R"("health": "10")");
}

QByteArray withSplash(QByteArray json, const QByteArray &radius)
{
    return json.replace(R"("damageType": "physical")",
                        R"("damageType": "physical", "splashRadius": ")" + radius + R"(")");
}

/// The second enemy of the scenario is of a type of its own.
QByteArray withSecondEnemy(QByteArray json, const QByteArray &fields)
{
    json.replace(R"(}],
    "wave")",
                 R"(}, { "id": "other", "speedPerTick": "0.5", "bounty": 7, "lifeCost": 1, )" +
                     fields + R"(}],
    "wave")");
    return json.replace(
        R"("wave": [{ "enemy": "grunt", "count": 2, "delayTicks": 0, "intervalTicks": 4 }])",
        R"("wave": [{ "enemy": "grunt", "count": 1, "delayTicks": 0, "intervalTicks": 1 },
                    { "enemy": "other", "count": 1, "delayTicks": 3, "intervalTicks": 1 }])");
}

QByteArray withUpgrades(QByteArray json, const QByteArray &upgrades)
{
    return json.replace(R"("footprint": [{"x": 0, "y": 0}])",
                        R"("footprint": [{"x": 0, "y": 0}], "upgrades": )" + upgrades);
}

/// A second level with 3 damage for 30 gold, a third with more range for 20 gold.
QByteArray upgradedJson()
{
    return withUpgrades(combatJson(), R"([{"cost": 30, "damageMin": "3", "damageMax": "3"},
                                          {"cost": 20, "range": "4"}])");
}

Simulation gameWithTower(const QByteArray &json)
{
    Simulation game(contentOf(json), 1);
    game.execute(BuildTowerCommand{{1, 1}, 0});
    return game;
}

Simulation gameAfter(const QByteArray &json, qint64 ticks)
{
    Simulation game = gameWithTower(json);
    game.advance(ticks);
    return game;
}

Fixed decimal(const char *text)
{
    return qtd::tests::required(Fixed::parse(QString::fromLatin1(text)));
}

} // namespace

class TowersTest : public QObject
{
    Q_OBJECT

private slots:
    static void aSplashReachesItsRadiusAndNoFurther_data()
    {
        QTest::addColumn<QByteArray>("radius");
        QTest::addColumn<QByteArray>("health");
        QTest::newRow("just enough") << QByteArray("2") << QByteArray("8");
        QTest::newRow("too short") << QByteArray("1.9999") << QByteArray("10");
    }

    static void aSplashReachesItsRadiusAndNoFurther()
    {
        QFETCH(const QByteArray, radius);
        QFETCH(const QByteArray, health);
        const Simulation game = gameAfter(withSplash(combatJson(), radius), 5);
        QCOMPARE(game.state().enemies.at(0).health.raw(), decimal("6").raw());
        QCOMPARE(game.state().enemies.at(1).health.raw(), decimal(health.constData()).raw());
    }

    static void withoutASplashOnlyTheTargetIsHit()
    {
        const Simulation game = gameAfter(combatJson(), 5);
        QCOMPARE(game.state().enemies.at(1).health.raw(), decimal("10").raw());
    }

    // The range of the roll is wide, so that two rolls would hardly ever be equal.
    static void aShotRollsItsDamageOnce()
    {
        QByteArray json = withSplash(combatJson(), "2");
        json.replace(R"("health": "10")", R"("health": "100")");
        json.replace(R"("damageMax": "2")", R"("damageMax": "9")");
        const Simulation before = gameAfter(json, 4);
        const Simulation game = gameAfter(json, 5);
        const Fixed first = before.state().enemies.at(0).health - game.state().enemies.at(0).health;
        const Fixed second = decimal("100") - game.state().enemies.at(1).health;
        QCOMPARE(second.raw(), first.raw());
        QByteArray single = json;
        single.replace(R"("splashRadius": "2", )", "");
        single.replace(R"(, "splashRadius": "2")", "");
        QCOMPARE(gameAfter(single, 5).state().random.state(), game.state().random.state());
    }

    static void eachEnemyInASplashHasItsOwnResistance()
    {
        const QByteArray other =
            R"("health": "10", "resistances": [{"damageType": "physical", "share": "0.5"}])";
        const Simulation game = gameAfter(withSecondEnemy(withSplash(combatJson(), "2"), other), 5);
        QCOMPARE(game.state().enemies.at(0).health.raw(), decimal("6").raw());
        QCOMPARE(game.state().enemies.at(1).health.raw(), decimal("9").raw());
    }

    static void aSplashCarriesTheEffectsOfTheTower()
    {
        QByteArray json = withSplash(combatJson(), "2");
        json.replace(R"("splashRadius": "2")",
                     R"("splashRadius": "2", "effects": [
                         {"kind": "slow", "strength": "0.5", "durationTicks": 9}])");
        const Simulation game = gameAfter(json, 5);
        QCOMPARE(game.state().enemies.at(1).slow.ticks, 9);
    }

    static void aSplashKillsAndPays()
    {
        QByteArray json = withSplash(scenarioJson(), "2");
        json.replace(R"("health": "4")", R"("health": "2")");
        Simulation game(contentOf(json), 1);
        game.advance(5);
        game.execute(BuildTowerCommand{{1, 1}, 0});
        game.tick();
        QVERIFY(game.state().enemies.empty());
        QCOMPARE(game.state().gold, 100 - 40 + 7 + 7);
    }

    // The first tower kills the second enemy in passing, the second tower must not pay for it
    // again.
    static void aDeadEnemyPaysOnce()
    {
        const QByteArray json =
            withSecondEnemy(withSplash(scenarioJson(), "2"), R"("health": "2")");
        Simulation game(contentOf(json), 1);
        game.advance(5);
        game.execute(BuildTowerCommand{{1, 1}, 0});
        game.execute(BuildTowerCommand{{2, 1}, 0});
        game.tick();
        QVERIFY(game.state().enemies.empty());
        QCOMPARE(game.state().gold, 100 - 80 + 7 + 7);
    }

    static void anUpgradeCostsGoldAndChangesTheTower()
    {
        Simulation game = gameWithTower(upgradedJson());
        QVERIFY(game.execute(UpgradeTowerCommand{kFirstTower}) == CommandResult::Accepted);
        QCOMPARE(game.state().gold, 100 - 40 - 30);
        QCOMPARE(game.state().towers.at(0).level, 1U);
        QCOMPARE(game.state().towers.at(0).spent, 70);
        game.tick();
        QCOMPARE(game.state().enemies.at(0).health.raw(), decimal("7").raw());
    }

    // The first enemy reaches (5, 0) at tick 11: out of the first range, inside the last.
    static void anUpgradeWidensTheRangeInTheGame()
    {
        Simulation game(contentOf(upgradedJson()), 1);
        game.execute(BuildTowerCommand{{5, 4}, 0});
        game.execute(UpgradeTowerCommand{kFirstTower});
        game.execute(UpgradeTowerCommand{kFirstTower});
        game.advance(11);
        QCOMPARE(game.state().enemies.at(0).health.raw(), decimal("7").raw());
    }

    // Shots at ticks 1 and 3 instead of 1 and 5.
    static void anUpgradeShortensTheCooldownInTheGame()
    {
        Simulation game =
            gameWithTower(withUpgrades(combatJson(), R"([{"cost": 0, "cooldownTicks": 2}])"));
        game.execute(UpgradeTowerCommand{kFirstTower});
        game.advance(3);
        QCOMPARE(game.state().enemies.at(0).health.raw(), decimal("6").raw());
    }

    static void anUpgradeBringsItsEffectsIntoTheGame()
    {
        Simulation game = gameWithTower(withUpgrades(combatJson(), R"([{"cost": 0, "effects": [
            {"kind": "slow", "strength": "0.5", "durationTicks": 9}]}])"));
        game.execute(UpgradeTowerCommand{kFirstTower});
        game.tick();
        QCOMPARE(game.state().enemies.at(0).slow.ticks, 9);
    }

    static void anUpgradeKeepsItsSplash()
    {
        const qtd::core::Content content =
            contentOf(withUpgrades(withSplash(combatJson(), "2"), R"([{"cost": 1}])"));
        QCOMPARE(content.towers.at(0).levels.at(1).splashRadius.raw(), decimal("2").raw());
    }

    static void anUpgradeKeepsWhatItDoesNotName()
    {
        const qtd::core::Content content = contentOf(upgradedJson());
        const qtd::core::TowerLevel &last = content.towers.at(0).levels.at(2);
        QCOMPARE(last.range.raw(), decimal("4").raw());
        QCOMPARE(last.damageMin.raw(), decimal("3").raw());
        QCOMPARE(last.cooldownTicks, 4);
        QCOMPARE(last.cost, 20);
    }

    static void anUpgradeReplacesEffectsItNames()
    {
        QByteArray json = upgradedJson();
        json.replace(R"("damageType": "physical")",
                     R"("damageType": "physical", "effects": [
                         {"kind": "slow", "strength": "0.5", "durationTicks": 9}])");
        json.replace(R"("cost": 20, "range": "4")",
                     R"("cost": 20, "effects": [
                         {"kind": "poison", "strength": "1", "durationTicks": 2}])");
        const qtd::core::Content content = contentOf(json);
        QCOMPARE(content.towers.at(0).levels.at(1).effects.at(0).durationTicks, 9);
        QCOMPARE(content.towers.at(0).levels.at(2).effects.size(), 1U);
        QVERIFY(content.towers.at(0).levels.at(2).effects.at(0).kind ==
                qtd::core::StatusKind::Poison);
    }

    static void anUpgradeCanWidenASplash()
    {
        const QByteArray json =
            withUpgrades(withSplash(combatJson(), "1"), R"([{"cost": 0, "splashRadius": "2"}])");
        Simulation game = gameWithTower(json);
        game.advance(4);
        game.execute(UpgradeTowerCommand{kFirstTower});
        game.tick();
        QCOMPARE(game.state().enemies.at(1).health.raw(), decimal("8").raw());
    }

    static void aTowerStopsAtItsHighestLevel()
    {
        Simulation game = gameWithTower(upgradedJson());
        game.execute(UpgradeTowerCommand{kFirstTower});
        game.execute(UpgradeTowerCommand{kFirstTower});
        const qint64 gold = game.state().gold;
        QVERIFY(game.check(UpgradeTowerCommand{kFirstTower}) == CommandResult::HighestLevel);
        QVERIFY(game.execute(UpgradeTowerCommand{kFirstTower}) == CommandResult::HighestLevel);
        QCOMPARE(game.state().towers.at(0).level, 2U);
        QCOMPARE(game.state().gold, gold);
        QCOMPARE(game.nextLevelOf(game.state().towers.at(0)), nullptr);
    }

    static void aTowerWithoutUpgradesHasNone()
    {
        Simulation game = gameWithTower(combatJson());
        QVERIFY(game.execute(UpgradeTowerCommand{kFirstTower}) == CommandResult::HighestLevel);
    }

    static void anUpgradeNeedsTheGold()
    {
        QByteArray json = upgradedJson();
        json.replace(R"("startGold": 100)", R"("startGold": 69)");
        Simulation game = gameWithTower(json);
        QVERIFY(game.execute(UpgradeTowerCommand{kFirstTower}) == CommandResult::NotEnoughGold);
        QCOMPARE(game.state().towers.at(0).level, 0U);
        QCOMPARE(game.state().gold, 29);
    }

    static void theExactGoldIsEnough()
    {
        QByteArray json = upgradedJson();
        json.replace(R"("startGold": 100)", R"("startGold": 70)");
        Simulation game = gameWithTower(json);
        QVERIFY(game.execute(UpgradeTowerCommand{kFirstTower}) == CommandResult::Accepted);
        QCOMPARE(game.state().gold, 0);
    }

    static void commandsNeedATowerAndARunningGame()
    {
        Simulation game = gameWithTower(upgradedJson());
        QVERIFY(game.execute(UpgradeTowerCommand{99}) == CommandResult::UnknownTower);
        QVERIFY(game.execute(SellTowerCommand{99}) == CommandResult::UnknownTower);
        QByteArray lost = upgradedJson();
        lost.replace(R"("lifeCost": 1)", R"("lifeCost": 3)");
        lost.replace(R"("health": "10")", R"("health": "1000")");
        Simulation over = gameWithTower(lost);
        over.advance(100);
        QVERIFY(over.state().status == qtd::core::GameStatus::Lost);
        QVERIFY(over.execute(UpgradeTowerCommand{kFirstTower}) == CommandResult::GameOver);
        QVERIFY(over.execute(SellTowerCommand{kFirstTower}) == CommandResult::GameOver);
    }

    static void sellingPaysAShareOfAllTheGold_data()
    {
        QTest::addColumn<QByteArray>("share");
        QTest::addColumn<int>("upgrades");
        QTest::addColumn<qint64>("value");
        QTest::newRow("standard") << QByteArray() << 0 << qint64{28};
        QTest::newRow("upgraded") << QByteArray() << 1 << qint64{49};
        QTest::newRow("rounds down") << QByteArray(R"("sellPercent": 33,)") << 0 << qint64{13};
        QTest::newRow("nothing") << QByteArray(R"("sellPercent": 0,)") << 1 << qint64{0};
        QTest::newRow("everything") << QByteArray(R"("sellPercent": 100,)") << 2 << qint64{90};
    }

    static void sellingPaysAShareOfAllTheGold()
    {
        QFETCH(const QByteArray, share);
        QFETCH(const int, upgrades);
        QFETCH(const qint64, value);
        QByteArray json = upgradedJson();
        json.replace(R"("startLives": 3,)", R"("startLives": 3, )" + share);
        Simulation game = gameWithTower(json);
        for (int done = 0; done < upgrades; ++done) {
            game.execute(UpgradeTowerCommand{kFirstTower});
        }
        const qint64 gold = game.state().gold;
        QCOMPARE(game.sellValue(game.state().towers.at(0)), value);
        QVERIFY(game.check(SellTowerCommand{kFirstTower}) == CommandResult::Accepted);
        QVERIFY(game.execute(SellTowerCommand{kFirstTower}) == CommandResult::Accepted);
        QCOMPARE(game.state().gold, gold + value);
        QVERIFY(game.state().towers.empty());
    }

    static void aSoldTowerLeavesItsCellAndStopsFiring()
    {
        Simulation game = gameWithTower(combatJson());
        game.execute(SellTowerCommand{kFirstTower});
        game.tick();
        QCOMPARE(game.state().enemies.at(0).health.raw(), decimal("10").raw());
        QVERIFY(game.check(BuildTowerCommand{{1, 1}, 0}) == CommandResult::Accepted);
    }

    static void sellingOneTowerLeavesTheOthers()
    {
        Simulation game = gameWithTower(combatJson());
        game.execute(BuildTowerCommand{{3, 1}, 0});
        game.execute(SellTowerCommand{kFirstTower});
        QCOMPARE(game.state().towers.size(), 1U);
        QCOMPARE(game.state().towers.at(0).id, 2);
    }

    static void enemiesRegenerate()
    {
        QByteArray json = combatJson();
        json.replace(R"("lifeCost": 1)", R"("lifeCost": 1, "regeneration": "0.5")");
        QCOMPARE(gameAfter(json, 4).state().enemies.at(0).health.raw(), decimal("9.5").raw());
    }

    static void regenerationStopsAtTheStartValue()
    {
        QByteArray json = combatJson();
        json.replace(R"("lifeCost": 1)", R"("lifeCost": 1, "regeneration": "5")");
        QCOMPARE(gameAfter(json, 2).state().enemies.at(0).health.raw(), decimal("10").raw());
    }

    // The poison lasts for tick 2, the enemy heals again at tick 3.
    static void poisonStopsRegeneration()
    {
        QByteArray json = combatJson();
        json.replace(R"("lifeCost": 1)", R"("lifeCost": 1, "regeneration": "0.5")");
        json.replace(R"("damageType": "physical")",
                     R"("damageType": "physical", "effects": [
                         {"kind": "poison", "strength": "0.5", "durationTicks": 1}])");
        QCOMPARE(gameAfter(json, 2).state().enemies.at(0).health.raw(), decimal("7.5").raw());
        QCOMPARE(gameAfter(json, 3).state().enemies.at(0).health.raw(), decimal("8").raw());
    }

    static void aBossIsMarked()
    {
        QByteArray json = scenarioJson();
        json.replace(R"("lifeCost": 1)", R"("lifeCost": 1, "boss": true)");
        QVERIFY(contentOf(json).enemies.at(0).boss);
        QVERIFY(!contentOf(scenarioJson()).enemies.at(0).boss);
    }

    static void brokenValuesAreNamed_data()
    {
        QTest::addColumn<QByteArray>("json");
        QTest::addColumn<QString>("error");
        const QByteArray lifeCost = R"("lifeCost": 1)";
        QByteArray boss = scenarioJson();
        boss.replace(lifeCost, R"("lifeCost": 1, "boss": "yes")");
        QByteArray regeneration = scenarioJson();
        regeneration.replace(lifeCost, R"("lifeCost": 1, "regeneration": "0")");
        QByteArray percent = scenarioJson();
        percent.replace(R"("startLives": 3)", R"("startLives": 3, "sellPercent": 101)");
        QTest::newRow("boss") << boss << u"enemies[0].boss: must be true or false"_s;
        QTest::newRow("regeneration") << regeneration << u"enemies[0].regeneration: must be"_s;
        QTest::newRow("sell percent") << percent << u"sellPercent: must be"_s;
        QTest::newRow("no splash")
            << withSplash(scenarioJson(), "0") << u"towers[0].splashRadius: must be"_s;
        QTest::newRow("upgrade without cost") << withUpgrades(scenarioJson(), R"([{"range": "4"}])")
                                              << u"towers[0].upgrades[0].cost: is missing"_s;
        QTest::newRow("unknown key in an upgrade")
            << withUpgrades(scenarioJson(), R"([{"cost": 1, "footprint": []}])")
            << u"towers[0].upgrades[0].footprint: unknown key"_s;
        QTest::newRow("upgrade below its own minimum")
            << withUpgrades(scenarioJson(), R"([{"cost": 1, "damageMax": "1"}])")
            << u"towers[0].upgrades[0].damageMax: must not be below damageMin"_s;
    }

    static void brokenValuesAreNamed()
    {
        QFETCH(const QByteArray, json);
        QFETCH(const QString, error);
        const QString found = qtd::core::parseContent(json).error;
        QVERIFY2(found.startsWith(error), qPrintable(found));
    }
};

QTEST_APPLESS_MAIN(TowersTest)
#include "tst_towers.moc"
