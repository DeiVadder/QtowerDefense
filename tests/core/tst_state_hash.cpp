#include "core/state_hash.h"
#include "scenario.h"

#include <QStringList>
#include <QTest>

#include <functional>
#include <utility>
#include <vector>

using qtd::core::BuildTowerCommand;
using qtd::core::contentHash;
using qtd::core::Enemy;
using qtd::core::Fixed;
using qtd::core::Fnv1a64;
using qtd::core::GameStatus;
using qtd::core::Simulation;
using qtd::core::SimulationState;
using qtd::core::stateHash;
using qtd::core::Tower;
using qtd::tests::contentOf;
using qtd::tests::editedJson;
using qtd::tests::scenarioJson;

using namespace Qt::StringLiterals;

namespace {

/// A game after a few ticks, with one enemy on the path and one tower.
Simulation runningGame()
{
    Simulation game(contentOf(scenarioJson()), 7);
    game.execute(BuildTowerCommand{{5, 1}, 0});
    game.advance(3);
    return game;
}

using Change = std::function<void(SimulationState &)>;
using NamedChanges = std::vector<std::pair<QString, Change>>;

/// Every field has a value of its own, gold is negative. Mirrored in state_hash_reference.py.
SimulationState handmadeState()
{
    SimulationState state;
    state.contentHash = 0x0123456789ABCDEFULL;
    state.tick = 42;
    state.status = GameStatus::Lost;
    state.gold = -5;
    state.lives = 3;
    state.random = qtd::core::Random(7, 54);
    state.nextEntityId = 9;
    state.spawnGroup = 1;
    state.spawnedInGroup = 2;
    state.spawnCountdownTicks = 4;
    state.enemies.push_back({3, 0, Fixed::fromRaw(15000), Fixed::fromRaw(25000)});
    state.towers.push_back({5, 1, {2, 3}, 6});
    return state;
}

/// The names of the changes that leave the hash as it is.
QStringList unnoticed(const SimulationState &base, const NamedChanges &changes)
{
    QStringList names;
    for (const auto &[name, change] : changes) {
        SimulationState changed = base;
        change(changed);
        if (stateHash(changed) == stateHash(base)) {
            names.append(name);
        }
    }
    return names;
}

NamedChanges fieldChanges()
{
    return {
        {"content", [](auto &s) { ++s.contentHash; }},
        {"tick", [](auto &s) { ++s.tick; }},
        {"status", [](auto &s) { s.status = GameStatus::Won; }},
        {"gold", [](auto &s) { ++s.gold; }},
        {"lives", [](auto &s) { ++s.lives; }},
        {"random state", [](auto &s) { s.random.next(); }},
        {"random stream", [](auto &s) { s.random = qtd::core::Random(7, 55); }},
        {"next id", [](auto &s) { ++s.nextEntityId; }},
        {"spawn group", [](auto &s) { ++s.spawnGroup; }},
        {"spawned", [](auto &s) { ++s.spawnedInGroup; }},
        {"spawn countdown", [](auto &s) { ++s.spawnCountdownTicks; }},
    };
}

NamedChanges entityChanges()
{
    return {
        {"enemy added", [](auto &s) { s.enemies.push_back(Enemy{}); }},
        {"enemy removed", [](auto &s) { s.enemies.clear(); }},
        {"enemy id", [](auto &s) { ++s.enemies[0].id; }},
        {"enemy type", [](auto &s) { ++s.enemies[0].type; }},
        {"enemy health", [](auto &s) { s.enemies[0].health = Fixed::fromRaw(1); }},
        {"enemy place", [](auto &s) { s.enemies[0].travelled = Fixed::fromRaw(1); }},
        {"tower added", [](auto &s) { s.towers.push_back(Tower{}); }},
        {"tower removed", [](auto &s) { s.towers.clear(); }},
        {"tower id", [](auto &s) { ++s.towers[0].id; }},
        {"tower type", [](auto &s) { ++s.towers[0].type; }},
        {"tower x", [](auto &s) { ++s.towers[0].cell.x; }},
        {"tower y", [](auto &s) { ++s.towers[0].cell.y; }},
        {"tower cooldown", [](auto &s) { ++s.towers[0].cooldownTicks; }},
    };
}

} // namespace

class TestStateHash : public QObject
{
    Q_OBJECT

private slots:
    /// The expected values come from tests/core/state_hash_reference.py, which shares no code with
    /// the project. They hold on every compiler and in every process.
    void theStateHashIsPinned()
    {
        QCOMPARE(stateHash(handmadeState()), 0x1109AD7A5261B20EULL);
    }

    void theContentHashIsPinned()
    {
        QCOMPARE(contentHash(contentOf(scenarioJson())), 0x44CDE2B42100B02EULL);
    }

    // The test vectors of the FNV authors.
    static void fnvMatchesPublishedValues()
    {
        QCOMPARE(Fnv1a64().value(), 0xcbf29ce484222325ULL);
        Fnv1a64 letter;
        letter.addBytes("a");
        QCOMPARE(letter.value(), 0xaf63dc4c8601ec8cULL);
        Fnv1a64 word;
        word.addBytes("foobar");
        QCOMPARE(word.value(), 0x85944171f73967e8ULL);
    }

    static void numbersGoInLowestByteFirst()
    {
        Fnv1a64 number;
        number.addNumber(0x0102030405060708ULL);
        Fnv1a64 bytes;
        bytes.addBytes("\x08\x07\x06\x05\x04\x03\x02\x01");
        QCOMPARE(number.value(), bytes.value());
        QCOMPARE(number.value(), 0x0c6d4496e17859d5ULL); // computed with a separate Python FNV-1a
    }

    static void everyFieldOfTheStateCounts()
    {
        const SimulationState base = runningGame().state();
        QVERIFY(base.enemies.size() == 1 && base.towers.size() == 1);
        QCOMPARE(unnoticed(base, fieldChanges()), QStringList());
        QCOMPARE(unnoticed(base, entityChanges()), QStringList());
    }

    static void sameFieldsInDifferentRolesDiffer()
    {
        SimulationState left = runningGame().state();
        SimulationState right = left;
        left.gold = 1;
        left.lives = 2;
        right.gold = 2;
        right.lives = 1;
        QVERIFY(stateHash(left) != stateHash(right));
    }

    static void memoryLayoutDoesNotCount()
    {
        const Simulation game = runningGame();
        const auto *copy = new Simulation(game); // another place in memory
        SimulationState roomy = game.state();
        roomy.enemies.reserve(1'000);
        roomy.towers.reserve(1'000);
        QCOMPARE(copy->hash(), game.hash());
        QCOMPARE(stateHash(roomy), game.hash());
        delete copy;
    }

    static void hashIgnoresNothingButIds_data()
    {
        QTest::addColumn<QByteArray>("from");
        QTest::addColumn<QByteArray>("to");
        const auto row = [](const char *from, const char *to) {
            QTest::newRow(from) << QByteArray(from) << QByteArray(to);
        };
        row(R"("startGold": 100)", R"("startGold": 101)");
        row(R"("startLives": 3)", R"("startLives": 4)");
        row(R"({ "x": 10, "y": 0 })", R"({ "x": 9, "y": 0 })");
        row(R"("gridWidth": 11)", R"("gridWidth": 12)");
        row(R"("gridHeight": 11)", R"("gridHeight": 12)");
        row(R"("terrain": "rock")", R"("terrain": "tree")");
        row(R"("x": 0, "y": 2)", R"("x": 1, "y": 2)");
        row(R"("footprint": [{"x": 0, "y": 0}])",
            R"("footprint": [{"x": 0, "y": 0}, {"x": 1, "y": 0}])");
        row(R"("cost": 40)", R"("cost": 41)");
        row(R"("range": "3")", R"("range": "3.0001")");
        row(R"("cooldownTicks": 4)", R"("cooldownTicks": 5)");
        row(R"("damageMin": "2")", R"("damageMin": "1")");
        row(R"("damageMax": "2")", R"("damageMax": "3")");
        row(R"("health": "4")", R"("health": "5")");
        row(R"("speedPerTick": "0.5")", R"("speedPerTick": "0.6")");
        row(R"("bounty": 7)", R"("bounty": 8)");
        row(R"("lifeCost": 1)", R"("lifeCost": 2)");
        row(R"("count": 2)", R"("count": 3)");
        row(R"("delayTicks": 0)", R"("delayTicks": 1)");
        row(R"("intervalTicks": 4)", R"("intervalTicks": 5)");
    }

    static void hashIgnoresNothingButIds()
    {
        QFETCH(const QByteArray, from);
        QFETCH(const QByteArray, to);
        const QByteArray json = editedJson(from, to);
        QVERIFY(!json.isEmpty());
        QVERIFY(contentHash(contentOf(json)) != contentHash(contentOf(scenarioJson())));
    }

    static void contentGoesIntoTheStateHash()
    {
        const Simulation base(contentOf(scenarioJson()), 7);
        const Simulation other(contentOf(editedJson(R"("cost": 40)", R"("cost": 41)")), 7);
        QVERIFY(base.hash() != other.hash());
    }

    static void idsOfContentDoNotCount()
    {
        const qtd::core::Content renamed =
            contentOf(editedJson(R"("id": "arrow")", R"("id": "bolt")"));
        QCOMPARE(contentHash(renamed), contentHash(contentOf(scenarioJson())));
    }
};

QTEST_APPLESS_MAIN(TestStateHash)
#include "tst_state_hash.moc"
