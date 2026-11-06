#include "../core/scenario.h"
#include "automation/bot.h"
#include "automation/invariants.h"

#include <QTest>

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <variant>

using qtd::automation::checkInvariants;
using qtd::core::BuildTowerCommand;
using qtd::core::CommandResult;
using qtd::core::Content;
using qtd::core::Fixed;
using qtd::core::Simulation;
using qtd::core::SimulationState;
using qtd::tests::contentOf;
using qtd::tests::editedJson;
using qtd::tests::scenarioJson;

using namespace Qt::StringLiterals;

using Damage = std::function<void(SimulationState &)>;
Q_DECLARE_METATYPE(Damage)

namespace {

/// A game with one enemy on the path and one tower at (5, 1), after a few ticks.
SimulationState healthyState(const Content &content)
{
    Simulation game(content, 7);
    game.execute(BuildTowerCommand{{5, 1}, 0});
    game.advance(3);
    return game.state();
}

void addTowerRows()
{
    const auto row = [](const char *name, const Damage &damage, const QString &expected) {
        QTest::newRow(name) << damage << expected;
    };
    row(
        "tower with the id of an enemy",
        [](SimulationState &s) { s.towers.front().id = s.enemies.front().id; },
        u"an enemy has its id"_s);
    row("tower type", [](SimulationState &s) { s.towers.front().type = 5; }, u"unknown type"_s);
    row(
        "tower outside",
        [](SimulationState &s) { s.towers.front().cell = {11, 1}; },
        u"outside the map"_s);
    row(
        "tower on the path",
        [](SimulationState &s) { s.towers.front().cell = {3, 0}; },
        u"on a blocked cell"_s);
    row(
        "tower on a rock",
        [](SimulationState &s) { s.towers.front().cell = {0, 2}; },
        u"on a blocked cell"_s);
    row(
        "shared cell",
        [](SimulationState &s) {
            s.towers.push_back(
                {.id = s.nextEntityId++, .cell = s.towers.front().cell, .spent = 40});
        },
        u"its cell is taken"_s);
    row(
        "cooldown",
        [](SimulationState &s) { s.towers.front().cooldownTicks = -1; },
        u"negative cooldown"_s);
    row(
        "enemy above its health",
        [](SimulationState &s) { s.enemies.front().health = Fixed::fromInt(5); },
        u"healed beyond its health"_s);
    row("level", [](SimulationState &s) { s.towers.front().level = 1; }, u"unknown level"_s);
    row(
        "spell that is not there",
        [](SimulationState &s) { s.spellCooldowns.push_back(0); },
        u"the spells do not match the content"_s);
    row(
        "gold of a tower",
        [](SimulationState &s) { ++s.towers.front().spent; },
        u"its gold does not add up"_s);
}

} // namespace

namespace {

/// What a fuzz bot has tried.
struct FuzzRecord
{
    std::set<qtd::core::Rotation> rotations;
    std::set<qtd::core::TargetPriority> priorities;
    std::set<CommandResult> priorityResults;
    std::set<CommandResult> upgradeResults;
    std::set<CommandResult> sellResults;
    std::set<CommandResult> spellResults;
    std::set<CommandResult> waveResults;

    void note(const qtd::core::Command &command, CommandResult result)
    {
        if (const auto *build = std::get_if<BuildTowerCommand>(&command)) {
            rotations.insert(build->rotation);
        } else if (const auto *aim = std::get_if<qtd::core::SetTargetPriorityCommand>(&command)) {
            priorities.insert(aim->priority);
            priorityResults.insert(result);
        } else if (std::holds_alternative<qtd::core::UpgradeTowerCommand>(command)) {
            upgradeResults.insert(result);
        } else if (std::holds_alternative<qtd::core::CastSpellCommand>(command)) {
            spellResults.insert(result);
        } else if (std::holds_alternative<qtd::core::CallWaveCommand>(command)) {
            waveResults.insert(result);
        } else {
            sellResults.insert(result);
        }
    }
};

void playFuzzGame(quint64 seed, FuzzRecord *record)
{
    const QByteArray spell = R"("spells": [{"id": "s", "cooldownTicks": 100, "radius": "1",
        "damageMin": "1", "damageMax": "1", "damageType": "ice"}], "wave")";
    QByteArray json = editedJson(R"("wave")", spell);
    json.replace(R"("count": 2)", R"("count": 30)"); // long enough to meet a spell that cools down
    json.replace(R"("delayTicks": 0)", R"("delayTicks": 3)"); // a wave to call
    Simulation game(contentOf(json), seed);
    const std::unique_ptr<qtd::automation::Bot> bot =
        qtd::automation::makeBot(qtd::automation::BotKind::Fuzz, seed);
    while (game.state().status == qtd::core::GameStatus::Running) {
        while (const std::optional<qtd::core::Command> command = bot->nextCommand(game)) {
            record->note(*command, game.execute(*command));
        }
        game.tick();
    }
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
        game.execute(BuildTowerCommand{{5, 1}, 0});
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
        row(
            "status without strength",
            [](SimulationState &s) { s.enemies.front().poison.ticks = 3; },
            u"impossible status effect"_s);
        row(
            "strength without status",
            [](SimulationState &s) { s.enemies.front().poison.strength = Fixed::fromInt(1); },
            u"impossible status effect"_s);
        row(
            "negative ticks of a status",
            [](SimulationState &s) { s.enemies.front().slow.ticks = -1; },
            u"impossible status effect"_s);
        row(
            "slow beyond the limit",
            [](SimulationState &s) { s.enemies.front().slow = {3, Fixed::fromInt(1)}; },
            u"impossible status effect"_s);
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
        addTowerRows();
    }

    // With this shape, a tower also takes the cell to its left.
    void everyCellOfAShapeIsChecked_data()
    {
        QTest::addColumn<qint64>("x");
        QTest::addColumn<qint64>("y");
        QTest::addColumn<QString>("expected");
        QTest::newRow("free") << qint64{2} << qint64{1} << QString();
        QTest::newRow("over the edge") << qint64{0} << qint64{1} << u"outside the map"_s;
        QTest::newRow("onto the rock") << qint64{1} << qint64{2} << u"on a blocked cell"_s;
        QTest::newRow("into a tower") << qint64{6} << qint64{1} << u"its cell is taken"_s;
    }

    void everyCellOfAShapeIsChecked()
    {
        QFETCH(const qint64, x);
        QFETCH(const qint64, y);
        QFETCH(const QString, expected);
        const Content content = contentOf(editedJson(R"("footprint": [{"x": 0, "y": 0}])",
                                                     R"("footprint": [{"x": 0, "y": 0},
                                                                      {"x": -1, "y": 0}])"));
        SimulationState state = healthyState(content);
        state.towers.push_back({.id = state.nextEntityId++, .cell = {x, y}, .spent = 40});
        const QString problem = qtd::automation::checkInvariants(content, state);
        QVERIFY2(expected.isEmpty() ? problem.isEmpty() : problem.contains(expected),
                 qPrintable(problem));
    }

    // On (0, 5), the second cell lies off the map unless the tower is turned half round.
    static void theCellsOfATurnedTowerAreChecked()
    {
        const Content content = contentOf(editedJson(R"("footprint": [{"x": 0, "y": 0}])",
                                                     R"("footprint": [{"x": 0, "y": 0},
                                                                      {"x": -1, "y": 0}])"));
        SimulationState state = healthyState(content);
        state.towers.push_back({.id = state.nextEntityId++,
                                .cell = {0, 5},
                                .rotation = qtd::core::Rotation::Half,
                                .spent = 40});
        QCOMPARE(checkInvariants(content, state), QString());
        state.towers.back().rotation = qtd::core::Rotation::None;
        QVERIFY(checkInvariants(content, state).contains(u"outside the map"_s));
    }

    static void spellCooldownsStayInTheirRange_data()
    {
        QTest::addColumn<qint64>("cooldown");
        QTest::addColumn<bool>("sound");
        QTest::newRow("ready") << qint64{0} << true;
        QTest::newRow("just cast") << qint64{5} << true;
        QTest::newRow("negative") << qint64{-1} << false;
        QTest::newRow("too long") << qint64{6} << false;
    }

    static void spellCooldownsStayInTheirRange()
    {
        QFETCH(const qint64, cooldown);
        QFETCH(const bool, sound);
        Content content = contentOf(scenarioJson());
        qtd::core::SpellType spell;
        spell.cooldownTicks = 5;
        content.spells.push_back(spell);
        Simulation game(content, 1);
        SimulationState state = game.state();
        state.spellCooldowns.at(0) = cooldown;
        QCOMPARE(checkInvariants(content, state).isEmpty(), sound);
    }

    // Over a few games, as a game draws how often its commands are well-formed.
    static void theFuzzBotTriesEverything()
    {
        FuzzRecord record;
        for (quint64 seed = 1; seed <= 40; ++seed) {
            playFuzzGame(seed, &record);
        }
        QCOMPARE(record.rotations.size(), std::size_t{qtd::core::kRotations});
        QCOMPARE(record.priorities.size(), std::size_t{qtd::core::kTargetPriorities});
        QVERIFY(record.priorityResults.contains(CommandResult::Accepted));
        QVERIFY(record.priorityResults.contains(CommandResult::UnknownTower));
        QVERIFY(record.upgradeResults.contains(CommandResult::HighestLevel));
        QVERIFY(record.upgradeResults.contains(CommandResult::UnknownTower));
        QVERIFY(record.sellResults.contains(CommandResult::Accepted));
        QVERIFY(record.sellResults.contains(CommandResult::UnknownTower));
        QVERIFY(record.spellResults.contains(CommandResult::Accepted));
        QVERIFY(record.spellResults.contains(CommandResult::SpellCooling));
        QVERIFY(record.spellResults.contains(CommandResult::UnknownSpell));
        QVERIFY(record.waveResults.contains(CommandResult::Accepted));
        QVERIFY(record.waveResults.contains(CommandResult::NoWaveWaiting));
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
