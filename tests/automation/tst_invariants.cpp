#include "../core/scenario.h"
#include "automation/invariants.h"

#include <QTest>

#include <functional>

using qtd::automation::checkInvariants;
using qtd::core::BuildTowerCommand;
using qtd::core::Content;
using qtd::core::Fixed;
using qtd::core::Simulation;
using qtd::core::SimulationState;
using qtd::tests::contentOf;
using qtd::tests::scenarioJson;

using namespace Qt::StringLiterals;

using Damage = std::function<void(SimulationState &)>;
Q_DECLARE_METATYPE(Damage)

namespace {

/// A game with one enemy on the path and one tower on slot 0, after a few ticks.
SimulationState healthyState(const Content &content)
{
    Simulation game(content, 7);
    game.execute(BuildTowerCommand{0, 0});
    game.advance(3);
    return game.state();
}

} // namespace

class InvariantsTest : public QObject
{
    Q_OBJECT

private slots:
    void aRealGameIsFine()
    {
        const Content content = contentOf(scenarioJson());
        Simulation game(content, 7);
        game.execute(BuildTowerCommand{0, 0});
        for (int tick = 0; tick < 40; ++tick) {
            game.tick();
            QCOMPARE(checkInvariants(content, game.state()), QString());
        }
    }

    void damageIsFound_data()
    {
        QTest::addColumn<Damage>("damage");
        QTest::addColumn<QString>("expected");
        const auto row = [](const char *name, const Damage &damage, const QString &expected) {
            QTest::newRow(name) << damage << expected;
        };
        row("gold", [](SimulationState &s) { s.gold = -1; }, u"gold is negative: -1"_s);
        row("lives", [](SimulationState &s) { s.lives = -2; }, u"lives are negative: -2"_s);
        row(
            "enemy id order",
            [](SimulationState &s) { s.enemies.push_back(s.enemies.front()); },
            u"its id is out of order"_s);
        row(
            "orphaned enemy id",
            [](SimulationState &s) { s.nextEntityId = s.enemies.front().id; },
            u"its id is out of order"_s);
        row(
            "enemy type",
            [](SimulationState &s) { s.enemies.front().type = 5; },
            u"unknown type"_s);
        row(
            "enemy before the path",
            [](SimulationState &s) { s.enemies.front().travelled = Fixed::fromInt(-1); },
            u"not on the path"_s);
        row(
            "enemy behind the path",
            [](SimulationState &s) { s.enemies.front().travelled = Fixed::fromInt(10); },
            u"not on the path"_s);
        row(
            "dead enemy",
            [](SimulationState &s) { s.enemies.front().health = Fixed(); },
            u"dead but still in the game"_s);
        row(
            "tower id order",
            [](SimulationState &s) { s.towers.push_back(s.towers.front()); },
            u"its id is out of order"_s);
        row(
            "orphaned tower id",
            [](SimulationState &s) { s.nextEntityId = s.towers.front().id; },
            u"its id is out of order"_s);
        row(
            "tower with the id of an enemy",
            [](SimulationState &s) { s.towers.front().id = s.enemies.front().id; },
            u"an enemy has its id"_s);
        row("tower type", [](SimulationState &s) { s.towers.front().type = 5; }, u"unknown type"_s);
        row("tower slot", [](SimulationState &s) { s.towers.front().slot = 3; }, u"unknown slot"_s);
        row(
            "shared slot",
            [](SimulationState &s) {
                s.towers.push_back({s.nextEntityId++, 0, s.towers.front().slot, 0});
            },
            u"its slot is taken"_s);
        row(
            "cooldown",
            [](SimulationState &s) { s.towers.front().cooldownTicks = -1; },
            u"negative cooldown"_s);
    }

    void damageIsFound()
    {
        QFETCH(const Damage, damage);
        QFETCH(const QString, expected);
        const Content content = contentOf(scenarioJson());
        SimulationState state = healthyState(content);
        QCOMPARE(checkInvariants(content, state), QString());
        damage(state);
        const QString problem = checkInvariants(content, state);
        QVERIFY2(problem.contains(expected), qPrintable(problem));
    }
};

QTEST_APPLESS_MAIN(InvariantsTest)
#include "tst_invariants.moc"
