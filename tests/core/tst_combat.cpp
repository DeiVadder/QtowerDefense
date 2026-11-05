#include "core/simulation.h"
#include "scenario.h"

#include <QByteArray>
#include <QTest>

#include <utility>

using qtd::core::BuildTowerCommand;
using qtd::core::CommandResult;
using qtd::core::Content;
using qtd::core::Fixed;
using qtd::core::GameStatus;
using qtd::core::SetTargetPriorityCommand;
using qtd::core::Simulation;
using qtd::core::TargetPriority;
using qtd::tests::contentOf;
using qtd::tests::scenarioJson;

namespace {

/// The scenario with ten health. Its tower does 2 damage every 4 ticks, the first enemy enters at
/// tick 1 and is hit there by a tower at (1, 1), the second enters at tick 5.
QByteArray combatJson()
{
    return scenarioJson().replace(R"("health": "4")", R"("health": "10")");
}

QByteArray withEffect(const QByteArray &kind, const QByteArray &strength, int ticks)
{
    const QByteArray effect = R"("damageType": "physical", "effects": [{"kind": ")" + kind +
                              R"(", "strength": ")" + strength + R"(", "durationTicks": )" +
                              QByteArray::number(ticks) + "}]";
    return combatJson().replace(R"("damageType": "physical")", effect);
}

QByteArray withResistance(QByteArray json, const QByteArray &type, const QByteArray &share)
{
    return json.replace(R"("lifeCost": 1)",
                        R"("lifeCost": 1, "resistances": [{"damageType": ")" + type +
                            R"(", "share": ")" + share + R"("}])");
}

/// A game with a tower at (1, 1), after the given number of ticks.
Simulation gameAfter(const QByteArray &json, qint64 ticks)
{
    Simulation game(contentOf(json), 1);
    game.execute(BuildTowerCommand{{1, 1}, 0});
    game.advance(ticks);
    return game;
}

Fixed decimal(const char *text)
{
    return qtd::tests::required(Fixed::parse(QString::fromLatin1(text)));
}

} // namespace

class CombatTest : public QObject
{
    Q_OBJECT

private slots:
    static void resistancesChangeTheDamage_data()
    {
        QTest::addColumn<QByteArray>("type");
        QTest::addColumn<QByteArray>("share");
        QTest::addColumn<QByteArray>("health");
        QTest::newRow("no resistance")
            << QByteArray("physical") << QByteArray("0") << QByteArray("8");
        QTest::newRow("half") << QByteArray("physical") << QByteArray("0.5") << QByteArray("9");
        QTest::newRow("immune") << QByteArray("physical") << QByteArray("1") << QByteArray("10");
        QTest::newRow("weak") << QByteArray("physical") << QByteArray("-1") << QByteArray("6");
        QTest::newRow("against another type")
            << QByteArray("fire") << QByteArray("1") << QByteArray("8");
    }

    static void resistancesChangeTheDamage()
    {
        QFETCH(const QByteArray, type);
        QFETCH(const QByteArray, share);
        QFETCH(const QByteArray, health);
        const Simulation game = gameAfter(withResistance(combatJson(), type, share), 1);
        QCOMPARE(game.state().enemies.front().health.raw(), decimal(health.constData()).raw());
    }

    static void aSlowHoldsAnEnemyBackForItsTicks()
    {
        const QByteArray json = withEffect("slow", "0.5", 3);
        const Simulation hit = gameAfter(json, 1);
        QCOMPARE(hit.state().enemies.front().slow.ticks, 3);
        QCOMPARE(hit.state().enemies.front().slow.strength.raw(), decimal("0.5").raw());
        const Simulation over = gameAfter(json, 4); // three steps of 0.25
        QCOMPARE(over.state().enemies.front().travelled.raw(), decimal("0.75").raw());
        QCOMPARE(over.state().enemies.front().slow.ticks, 0);
        QCOMPARE(over.state().enemies.front().slow.strength.raw(), Fixed().raw());
        const Simulation free = gameAfter(json, 5); // a full step again, then the next hit
        QCOMPARE(free.state().enemies.front().travelled.raw(), decimal("1.25").raw());
        QCOMPARE(free.state().enemies.front().slow.ticks, 3);
    }

    static void resistancesChangeStatusEffects_data()
    {
        QTest::addColumn<QByteArray>("share");
        QTest::addColumn<QByteArray>("strength");
        QTest::addColumn<qint64>("ticks");
        QTest::newRow("half") << QByteArray("0.5") << QByteArray("0.3") << qint64{3};
        QTest::newRow("immune") << QByteArray("1") << QByteArray("0") << qint64{0};
        QTest::newRow("weak, held at the limit")
            << QByteArray("-1") << QByteArray("0.9") << qint64{3};
    }

    static void resistancesChangeStatusEffects()
    {
        QFETCH(const QByteArray, share);
        QFETCH(const QByteArray, strength);
        QFETCH(const qint64, ticks);
        const QByteArray json = withResistance(withEffect("slow", "0.6", 3), "physical", share);
        const Simulation game = gameAfter(json, 1);
        QCOMPARE(game.state().enemies.front().slow.strength.raw(),
                 decimal(strength.constData()).raw());
        QCOMPARE(game.state().enemies.front().slow.ticks, ticks);
    }

    static void theSlowestEnemyStillMoves()
    {
        QByteArray json = withEffect("slow", "0.9", 1000);
        json.replace(R"("speedPerTick": "0.5")", R"("speedPerTick": "0.0001")");
        const Simulation game = gameAfter(json, 3);
        QCOMPARE(game.state().enemies.front().travelled.raw(), Fixed::fromRaw(2).raw());
    }

    static void poisonHurtsForItsTicks()
    {
        const QByteArray json = withEffect("poison", "0.5", 3);
        const Simulation hit = gameAfter(json, 1); // poison works before the towers fire
        QCOMPARE(hit.state().enemies.front().health.raw(), decimal("8").raw());
        QCOMPARE(hit.state().enemies.front().poison.ticks, 3);
        const Simulation over = gameAfter(json, 4);
        QCOMPARE(over.state().enemies.front().health.raw(), decimal("6.5").raw());
        QCOMPARE(over.state().enemies.front().poison.ticks, 0);
        QCOMPARE(over.state().enemies.front().poison.strength.raw(), Fixed().raw());
    }

    static void poisonFollowsTheResistance()
    {
        const QByteArray json = withResistance(withEffect("poison", "0.5", 3), "physical", "0.5");
        QCOMPARE(gameAfter(json, 1).state().enemies.front().poison.strength.raw(),
                 decimal("0.25").raw());
    }

    static void aPoisonedEnemyPaysItsBountyOnce()
    {
        QByteArray json = withEffect("poison", "0.5", 3);
        json.replace(R"("health": "10")", R"("health": "2.5")");
        const Simulation before = gameAfter(json, 1);
        QCOMPARE(before.state().gold, 60);
        const Simulation dead = gameAfter(json, 2);
        QVERIFY(dead.state().enemies.empty());
        QCOMPARE(dead.state().gold, 67);
    }

    // The enemy would die of the poison at tick 2, but it reaches the end first.
    static void anEnemyMovesBeforePoisonWorks()
    {
        QByteArray json = withEffect("poison", "0.5", 3);
        json.replace(R"("health": "10")", R"("health": "2.5")");
        json.replace(R"("speedPerTick": "0.5")", R"("speedPerTick": "10")");
        const Simulation game = gameAfter(json, 2);
        QVERIFY(game.state().enemies.empty());
        QCOMPARE(game.state().lives, 2);
        QCOMPARE(game.state().gold, 60);
    }

    static void effectsDoNotAddUp_data()
    {
        QTest::addColumn<QByteArray>("firstStrength");
        QTest::addColumn<int>("firstTicks");
        QTest::addColumn<QByteArray>("secondStrength");
        QTest::addColumn<int>("secondTicks");
        QTest::addColumn<QByteArray>("strength");
        QTest::addColumn<qint64>("ticks");
        const QByteArray weak("0.2");
        const QByteArray strong("0.6");
        QTest::newRow("the stronger comes later")
            << weak << 50 << strong << 7 << strong << qint64{7};
        QTest::newRow("the weaker comes later") << strong << 7 << weak << 50 << strong << qint64{7};
        QTest::newRow("equal, the longer comes later")
            << strong << 5 << strong << 9 << strong << qint64{9};
        QTest::newRow("equal, the shorter comes later")
            << strong << 9 << strong << 5 << strong << qint64{9};
    }

    // Two towers, the one built first fires first, both on tick 1.
    static void effectsDoNotAddUp()
    {
        QFETCH(const QByteArray, firstStrength);
        QFETCH(const int, firstTicks);
        QFETCH(const QByteArray, secondStrength);
        QFETCH(const int, secondTicks);
        QFETCH(const QByteArray, strength);
        QFETCH(const qint64, ticks);
        Content content = contentOf(withEffect("slow", firstStrength, firstTicks));
        content.towers.push_back(
            contentOf(withEffect("slow", secondStrength, secondTicks)).towers[0]);
        Simulation game(std::move(content), 1);
        QVERIFY(game.execute(BuildTowerCommand{{1, 1}, 0}) == CommandResult::Accepted);
        QVERIFY(game.execute(BuildTowerCommand{{2, 1}, 1}) == CommandResult::Accepted);
        game.advance(1);
        QCOMPARE(game.state().enemies.front().slow.strength.raw(),
                 decimal(strength.constData()).raw());
        QCOMPARE(game.state().enemies.front().slow.ticks, ticks);
    }

    static void aTowerAimsAsItIsTold_data()
    {
        QTest::addColumn<TargetPriority>("priority");
        QTest::addColumn<qint64>("towerX");
        QTest::addColumn<qint64>("buildTick");
        QTest::addColumn<int>("firstHealth");
        QTest::addColumn<int>("secondHealth");
        QTest::newRow("first") << TargetPriority::First << qint64{0} << qint64{0} << 6 << 10;
        QTest::newRow("last") << TargetPriority::Last << qint64{0} << qint64{0} << 8 << 8;
        QTest::newRow("strongest") << TargetPriority::Strongest << qint64{0} << qint64{0} << 8 << 8;
        QTest::newRow("strongest, a tie goes to the lowest id")
            << TargetPriority::Strongest << qint64{0} << qint64{4} << 8 << 10;
        QTest::newRow("nearest is the second")
            << TargetPriority::Nearest << qint64{0} << qint64{4} << 10 << 8;
        QTest::newRow("nearest is the first")
            << TargetPriority::Nearest << qint64{2} << qint64{4} << 8 << 10;
    }

    // At tick 5 the first enemy stands at x = 2 and the second at x = 0. A tower that was there
    // from the start has already hit the first one at tick 1.
    static void aTowerAimsAsItIsTold()
    {
        QFETCH(const TargetPriority, priority);
        QFETCH(const qint64, towerX);
        QFETCH(const qint64, buildTick);
        QFETCH(const int, firstHealth);
        QFETCH(const int, secondHealth);
        Simulation game(contentOf(combatJson()), 1);
        game.advance(buildTick);
        QVERIFY(game.execute(BuildTowerCommand{{towerX, 1}, 0}) == CommandResult::Accepted);
        const qint64 tower = game.state().towers.front().id;
        QVERIFY(game.execute(SetTargetPriorityCommand{tower, priority}) == CommandResult::Accepted);
        QVERIFY(game.state().towers.front().priority == priority);
        game.advance(5 - buildTick);
        QCOMPARE(game.state().enemies.size(), 2U);
        QCOMPARE(game.state().enemies[0].health.raw(), Fixed::fromInt(firstHealth).raw());
        QCOMPARE(game.state().enemies[1].health.raw(), Fixed::fromInt(secondHealth).raw());
    }

    static void anEnemyOutOfRangeIsNoTarget()
    {
        Simulation game(contentOf(combatJson()), 1);
        game.execute(BuildTowerCommand{{10, 1}, 0});
        const qint64 tower = game.state().towers.front().id;
        game.execute(SetTargetPriorityCommand{tower, TargetPriority::Last});
        game.advance(5);
        QCOMPARE(game.state().enemies[0].health.raw(), Fixed::fromInt(10).raw());
        QCOMPARE(game.state().enemies[1].health.raw(), Fixed::fromInt(10).raw());
    }

    static void onlyTowersTakeAPriority()
    {
        Simulation game(contentOf(combatJson()), 1);
        game.execute(BuildTowerCommand{{10, 1}, 0});
        game.advance(1);
        const quint64 before = game.hash();
        const qint64 enemy = game.state().enemies.front().id;
        for (const qint64 id : {qint64{0}, qint64{-1}, enemy, game.state().nextEntityId}) {
            const SetTargetPriorityCommand command{id, TargetPriority::Last};
            QVERIFY(game.check(command) == CommandResult::UnknownTower);
            QVERIFY(game.execute(command) == CommandResult::UnknownTower);
        }
        QCOMPARE(game.hash(), before);
    }

    static void noPriorityAfterTheEnd()
    {
        Simulation game(contentOf(combatJson()), 1);
        game.execute(BuildTowerCommand{{10, 1}, 0});
        const qint64 tower = game.state().towers.front().id;
        game.advance(100'000);
        QVERIFY(game.state().status != GameStatus::Running);
        QVERIFY(game.execute(SetTargetPriorityCommand{tower, TargetPriority::Last}) ==
                CommandResult::GameOver);
    }
};

QTEST_APPLESS_MAIN(CombatTest)
#include "tst_combat.moc"
