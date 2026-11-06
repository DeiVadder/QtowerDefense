#include "core/simulation.h"
#include "scenario.h"

#include <QByteArray>
#include <QTest>

using namespace Qt::StringLiterals;

using qtd::core::CastSpellCommand;
using qtd::core::Cell;
using qtd::core::CommandResult;
using qtd::core::Fixed;
using qtd::core::Point;
using qtd::core::Simulation;
using qtd::tests::contentOf;
using qtd::tests::scenarioJson;

namespace {

/// Ten health. The first enemy enters at tick 1 and stands at x = (t - 1) / 2, the second enters at
/// tick 5, 2 tiles behind it. A blast does 3 fire damage, then needs 5 ticks.
QByteArray spellJson(const QByteArray &fields = {})
{
    QByteArray json = scenarioJson();
    json.replace(R"("health": "4")", R"("health": "10")");
    return json.replace(R"("wave")",
                        R"("spells": [{"id": "blast", "cooldownTicks": 5, "radius": "1",
                           "damageMin": "3", "damageMax": "3", "damageType": "fire")" +
                            fields + R"(}], "wave")");
}

Fixed decimal(const char *text)
{
    return qtd::tests::required(Fixed::parse(QString::fromLatin1(text)));
}

} // namespace

class SpellsTest : public QObject
{
    Q_OBJECT

private slots:
    static void aSpellHitsAroundItsCell_data()
    {
        QTest::addColumn<int>("x");
        QTest::addColumn<int>("y");
        QTest::addColumn<QByteArray>("health");
        QTest::newRow("on the enemy") << 2 << 0 << QByteArray("7");
        QTest::newRow("at the edge") << 3 << 0 << QByteArray("7");
        QTest::newRow("beside the path") << 2 << 1 << QByteArray("7");
        QTest::newRow("too far") << 4 << 0 << QByteArray("10");
        QTest::newRow("too far aside") << 3 << 1 << QByteArray("10");
    }

    // At tick 5 the first enemy stands at (2, 0).
    static void aSpellHitsAroundItsCell()
    {
        QFETCH(const int, x);
        QFETCH(const int, y);
        QFETCH(const QByteArray, health);
        Simulation game(contentOf(spellJson()), 1);
        game.advance(5);
        QVERIFY(game.execute(CastSpellCommand{0, {x, y}}) == CommandResult::Accepted);
        QCOMPARE(game.state().enemies.at(0).health.raw(), decimal(health.constData()).raw());
    }

    static void aSpellHitsAllItReaches()
    {
        QByteArray json = spellJson();
        json.replace(R"("radius": "1")", R"("radius": "2")");
        Simulation game(contentOf(json), 1);
        game.advance(5);
        game.execute(CastSpellCommand{0, {1, 0}});
        QCOMPARE(game.state().enemies.at(0).health.raw(), decimal("7").raw());
        QCOMPARE(game.state().enemies.at(1).health.raw(), decimal("7").raw());
    }

    static void aSpellMeetsResistances()
    {
        QByteArray json = spellJson();
        json.replace(R"("lifeCost": 1)",
                     R"("lifeCost": 1, "resistances": [{"damageType": "fire", "share": "0.5"}])");
        Simulation game(contentOf(json), 1);
        game.advance(1);
        game.execute(CastSpellCommand{0, {0, 0}});
        QCOMPARE(game.state().enemies.at(0).health.raw(), decimal("8.5").raw());
    }

    static void aSpellLeavesItsEffects()
    {
        const QByteArray effects =
            R"(, "effects": [{"kind": "slow", "strength": "0.5", "durationTicks": 9}])";
        QByteArray json = spellJson(effects);
        json.replace(R"("damageMin": "3", "damageMax": "3")",
                     R"("damageMin": "0", "damageMax": "0")");
        Simulation game(contentOf(json), 1);
        game.advance(1);
        game.execute(CastSpellCommand{0, {0, 0}});
        QCOMPARE(game.state().enemies.at(0).health.raw(), decimal("10").raw());
        QCOMPARE(game.state().enemies.at(0).slow.ticks, 9);
    }

    static void aSpellKillsAndPays()
    {
        QByteArray json = spellJson();
        json.replace(R"("health": "10")", R"("health": "3")");
        Simulation game(contentOf(json), 1);
        game.advance(1);
        QVERIFY(game.execute(CastSpellCommand{0, {0, 0}}) == CommandResult::Accepted);
        QVERIFY(game.state().enemies.empty());
        QCOMPARE(game.state().gold, 107);
    }

    static void theLastKillWinsAtTheNextTick()
    {
        QByteArray json = spellJson();
        json.replace(R"("health": "10")", R"("health": "3")");
        json.replace(R"("count": 2)", R"("count": 1)");
        Simulation game(contentOf(json), 1);
        game.advance(1);
        game.execute(CastSpellCommand{0, {0, 0}});
        game.tick();
        QVERIFY(game.state().status == qtd::core::GameStatus::Won);
    }

    static void aSpellWithoutEnemiesStillCoolsDown()
    {
        Simulation game(contentOf(spellJson()), 1);
        QVERIFY(game.execute(CastSpellCommand{0, {5, 5}}) == CommandResult::Accepted);
        QCOMPARE(game.state().spellCooldowns.at(0), 5);
    }

    static void aSpellIsReadyAfterItsCooldown()
    {
        Simulation game(contentOf(spellJson()), 1);
        game.execute(CastSpellCommand{0, {5, 5}});
        game.advance(4);
        QCOMPARE(game.state().spellCooldowns.at(0), 1);
        const qint64 gold = game.state().gold;
        QVERIFY(game.check(CastSpellCommand{0, {5, 5}}) == CommandResult::SpellCooling);
        QVERIFY(game.execute(CastSpellCommand{0, {5, 5}}) == CommandResult::SpellCooling);
        QCOMPARE(game.state().spellCooldowns.at(0), 1);
        QCOMPARE(game.state().gold, gold);
        game.tick();
        QVERIFY(game.execute(CastSpellCommand{0, {5, 5}}) == CommandResult::Accepted);
    }

    static void aCooldownStopsAtZero()
    {
        Simulation game(contentOf(spellJson()), 1);
        game.advance(3);
        QCOMPARE(game.state().spellCooldowns.at(0), 0);
    }

    static void aRejectedSpellChangesNothing_data()
    {
        QTest::addColumn<qint64>("spell");
        QTest::addColumn<qint64>("x");
        QTest::addColumn<int>("result");
        const auto unknown = static_cast<int>(CommandResult::UnknownSpell);
        const auto outside = static_cast<int>(CommandResult::OutsideMap);
        QTest::newRow("below") << qint64{-1} << qint64{0} << unknown;
        QTest::newRow("above") << qint64{1} << qint64{0} << unknown;
        QTest::newRow("left of the map") << qint64{0} << qint64{-1} << outside;
        QTest::newRow("right of the map") << qint64{0} << qint64{11} << outside;
    }

    static void aRejectedSpellChangesNothing()
    {
        QFETCH(const qint64, spell);
        QFETCH(const qint64, x);
        QFETCH(const int, result);
        Simulation game(contentOf(spellJson()), 1);
        game.advance(1);
        const quint64 before = game.hash();
        QCOMPARE(static_cast<int>(game.execute(CastSpellCommand{spell, {x, 0}})), result);
        QCOMPARE(game.hash(), before);
    }

    static void noSpellIsCastAfterTheEnd()
    {
        QByteArray json = spellJson();
        json.replace(R"("lifeCost": 1)", R"("lifeCost": 3)");
        Simulation game(contentOf(json), 1);
        game.advance(100);
        QVERIFY(game.execute(CastSpellCommand{0, {5, 5}}) == CommandResult::GameOver);
    }

    static void aMapMayHaveNoSpells()
    {
        const Simulation game(contentOf(scenarioJson()), 1);
        QVERIFY(game.state().spellCooldowns.empty());
        QVERIFY(game.check(CastSpellCommand{0, {5, 5}}) == CommandResult::UnknownSpell);
    }

    // The roll of a spell comes from the game, so that a replay gets the same damage.
    static void theRollOfASpellIsPartOfTheGame()
    {
        QByteArray json = spellJson();
        json.replace(R"("damageMax": "3")", R"("damageMax": "9")");
        const auto healthAfterASpell = [&json](quint64 seed) {
            Simulation game(contentOf(json), seed);
            game.advance(1);
            game.execute(CastSpellCommand{0, {0, 0}});
            return game.state().enemies.at(0).health.raw();
        };
        QCOMPARE(healthAfterASpell(1), healthAfterASpell(1));
        QVERIFY(healthAfterASpell(1) != healthAfterASpell(2));
    }

    static void aPointLiesOnTheNearestCell_data()
    {
        QTest::addColumn<QByteArray>("x");
        QTest::addColumn<qint64>("cell");
        QTest::newRow("centre") << QByteArray("3") << qint64{3};
        QTest::newRow("below half") << QByteArray("3.4999") << qint64{3};
        QTest::newRow("half") << QByteArray("3.5") << qint64{4};
        QTest::newRow("start") << QByteArray("0") << qint64{0};
    }

    static void aPointLiesOnTheNearestCell()
    {
        QFETCH(const QByteArray, x);
        QFETCH(const qint64, cell);
        const Cell found = qtd::core::cellAt(Point{decimal(x.constData()), decimal("1.2")});
        QCOMPARE(found.x, cell);
        QCOMPARE(found.y, 1);
    }

    static void brokenSpellsAreNamed_data()
    {
        QTest::addColumn<QByteArray>("from");
        QTest::addColumn<QByteArray>("to");
        QTest::addColumn<QString>("error");
        QTest::newRow("cooldown") << QByteArray(R"("cooldownTicks": 5)")
                                  << QByteArray(R"("cooldownTicks": 0)")
                                  << u"spells[0].cooldownTicks: must be"_s;
        QTest::newRow("radius") << QByteArray(R"("radius": "1")") << QByteArray(R"("radius": "0")")
                                << u"spells[0].radius: must be"_s;
        QTest::newRow("damage") << QByteArray(R"("damageMax": "3")")
                                << QByteArray(R"("damageMax": "2")")
                                << u"spells[0].damageMax: must not be below damageMin"_s;
        QTest::newRow("type") << QByteArray(R"("damageType": "fire")")
                              << QByteArray(R"("damageType": "wind")")
                              << u"spells[0].damageType: unknown damage type"_s;
        QTest::newRow("twice") << QByteArray(R"("damageType": "fire"})")
                               << QByteArray(
                                      R"("damageType": "fire"}, {"id": "blast", "cooldownTicks": 1,
                                  "radius": "1", "damageMin": "0", "damageMax": "0",
                                  "damageType": "fire"})")
                               << u"spells: id \"blast\" is used twice"_s;
    }

    static void brokenSpellsAreNamed()
    {
        QFETCH(const QByteArray, from);
        QFETCH(const QByteArray, to);
        QFETCH(const QString, error);
        QByteArray json = spellJson();
        QVERIFY(json.contains(from));
        const QString found = qtd::core::parseContent(json.replace(from, to)).error;
        QVERIFY2(found.startsWith(error), qPrintable(found));
    }
};

QTEST_APPLESS_MAIN(SpellsTest)
#include "tst_spells.moc"
