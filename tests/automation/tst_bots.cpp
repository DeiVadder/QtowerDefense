#include "../core/scenario.h"
#include "automation/bot.h"
#include "automation/headless_runner.h"
#include "core/world.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

#include <cstddef>
#include <memory>
#include <optional>
#include <variant>

using qtd::automation::Bot;
using qtd::automation::BotKind;
using qtd::core::BuildTowerCommand;
using qtd::core::CastSpellCommand;
using qtd::core::Cell;
using qtd::core::Command;
using qtd::core::Simulation;
using qtd::core::UpgradeTowerCommand;

namespace {

/// No gold, so the bot can only cast. A slow grunt enters first, a runner overtakes it.
Simulation spellGame()
{
    QByteArray json = qtd::tests::scenarioJson();
    json.replace(R"("startGold": 100)", R"("startGold": 0)");
    json.replace(R"("speedPerTick": "0.5")", R"("speedPerTick": "0.1")");
    json.replace(R"("enemies": [)", R"("enemies": [{"id": "runner", "health": "9",
        "speedPerTick": "0.5", "bounty": 1, "lifeCost": 1}, )");
    json.replace(
        R"("wave": [{ "enemy": "grunt", "count": 2, "delayTicks": 0, "intervalTicks": 4 }])",
        R"("spells": [{"id": "mark", "cooldownTicks": 14, "radius": "1", "damageMin": "0",
                    "damageMax": "0", "damageType": "ice"}],
                    "wave": [{"enemy": "grunt", "count": 1, "delayTicks": 0, "intervalTicks": 1},
                             {"enemy": "runner", "count": 1, "delayTicks": 2, "intervalTicks": 1}])");
    return {qtd::tests::contentOf(json), 1};
}

/// The first command of the good bot in a fresh game of this content.
Command firstGoodCommand(const QByteArray &json)
{
    const Simulation game(qtd::tests::contentOf(json), 1);
    const std::unique_ptr<Bot> bot = qtd::automation::makeBot(BotKind::Good, 1);
    return qtd::tests::required(bot->nextCommand(game));
}

} // namespace

class BotsTest : public QObject
{
    Q_OBJECT

private slots:
    static void everyMapOfTheWorldCanBeWonButNotWithoutTowers_data()
    {
        QTest::addColumn<QString>("map");
        const qtd::core::World world = qtd::tests::required(
            qtd::core::loadWorldFile(QStringLiteral(QTD_CONTENT_DIR "/world.json")).world);
        for (const qtd::core::WorldSection &section : world.sections) {
            QTest::newRow(qPrintable(section.id)) << section.map;
        }
    }

    // The bar is low on purpose: the balance of the maps comes with stage 12.
    static void everyMapOfTheWorldCanBeWonButNotWithoutTowers()
    {
        QFETCH(const QString, map);
        QVERIFY(!map.isEmpty()); // every section has its map now
        const auto statusWith = [&map](BotKind bot) {
            qtd::automation::HeadlessOptions options;
            options.map = QStringLiteral(QTD_CONTENT_DIR "/") + map;
            options.bot = bot;
            const QByteArray json = qtd::automation::runHeadless(options).json;
            const QJsonObject report = QJsonDocument::fromJson(json).object();
            if (report.contains(QLatin1StringView("error"))) {
                return QString::fromUtf8(json); // the problem itself, in place of a status
            }
            return report[QLatin1StringView("games")]
                .toArray()
                .at(0)
                .toObject()[QLatin1StringView("status")]
                .toString();
        };
        QCOMPARE(statusWith(BotKind::Naive), QStringLiteral("won"));
        QCOMPARE(statusWith(BotKind::Good), QStringLiteral("won"));
        QCOMPARE(statusWith(BotKind::Idle), QStringLiteral("lost"));
    }

    static void theNaiveBotWaitsForAnEnemy()
    {
        const Simulation game = spellGame();
        const std::unique_ptr<Bot> bot = qtd::automation::makeBot(BotKind::Naive, 1);
        QVERIFY(!bot->nextCommand(game).has_value());
    }

    static void theNaiveBotCastsOnceTheSpellIsReady()
    {
        Simulation game = spellGame();
        const std::unique_ptr<Bot> bot = qtd::automation::makeBot(BotKind::Naive, 1);
        game.advance(1);
        const Command command = qtd::tests::required(bot->nextCommand(game));
        QVERIFY(std::holds_alternative<CastSpellCommand>(command));
        game.execute(command);
        QVERIFY(!bot->nextCommand(game).has_value()); // cooling down
    }

    // The list of enemies goes by age, the oldest is not the one that has come furthest.
    static void theNaiveBotAimsAtTheEnemyThatHasComeFurthest()
    {
        Simulation game = spellGame();
        const std::unique_ptr<Bot> bot = qtd::automation::makeBot(BotKind::Naive, 1);
        game.advance(1);
        game.execute(qtd::tests::required(bot->nextCommand(game)));
        game.advance(14);
        QCOMPARE(game.state().enemies.size(), std::size_t{2});
        const Cell grunt = qtd::core::cellAt(game.position(game.state().enemies.at(0)));
        const Cell runner = qtd::core::cellAt(game.position(game.state().enemies.at(1)));
        QVERIFY(runner.x > grunt.x);
        const Command command = qtd::tests::required(bot->nextCommand(game));
        QCOMPARE(std::get<CastSpellCommand>(command).cell.x, runner.x);
    }

    // The path runs along y = 0. A tower of range 3 on (3, 1) reaches the points of the path from
    // x = 0.17 to 5.83, eleven of the points every half field. On (2, 1) the start cuts it to ten.
    static void theGoodBotBuildsWhereItReachesTheMostPath()
    {
        const Command command = firstGoodCommand(qtd::tests::scenarioJson());
        const BuildTowerCommand build = std::get<BuildTowerCommand>(command);
        QCOMPARE(build.cell.x, 3);
        QCOMPARE(build.cell.y, 1);
        QCOMPARE(build.towerType, 0);
    }

    // A cheaper tower is no bargain when the wave does not burn.
    static void theGoodBotBuildsWhatTheWaveFears()
    {
        const QByteArray json = qtd::tests::editedJson(R"("towers": [{)", R"("towers": [{
            "id": "torch", "cost": 20, "range": "3", "cooldownTicks": 4, "damageMin": "2",
            "damageMax": "2", "damageType": "fire", "footprint": [{"x": 0, "y": 0}]}, {)")
                                    .replace(R"("lifeCost": 1)", R"("lifeCost": 1,
            "resistances": [{"damageType": "fire", "share": "1"}])");
        QCOMPARE(std::get<BuildTowerCommand>(firstGoodCommand(json)).towerType, 1);
    }

    // 1.5 damage on a radius of one field counts as 3, more than the 2 of the arrow.
    static void theGoodBotCountsTheSplash()
    {
        const QByteArray json = qtd::tests::editedJson(R"("towers": [{)", R"("towers": [{
            "id": "boulder", "cost": 40, "range": "3", "cooldownTicks": 4, "damageMin": "1.5",
            "damageMax": "1.5", "splashRadius": "1", "damageType": "physical",
            "footprint": [{"x": 0, "y": 0}]}, {)");
        const BuildTowerCommand build = std::get<BuildTowerCommand>(firstGoodCommand(json));
        QCOMPARE(build.towerType, 0);
        QCOMPARE(build.cell.x, 3);
    }

    // The best cell stays the best when the tower must turn to fit: the rock below it blocks the
    // second field of the tower as it is drawn.
    static void theGoodBotTurnsATowerThatDoesNotFit()
    {
        const QByteArray json =
            qtd::tests::editedJson(R"("footprint": [{"x": 0, "y": 0}])",
                                   R"("footprint": [{"x": 0, "y": 0}, {"x": 0, "y": 1}])")
                .replace(R"("obstacles": [{ "terrain": "rock", "x": 0, "y": 2 }])",
                         R"("obstacles": [{ "terrain": "rock", "x": 3, "y": 2 }])");
        const BuildTowerCommand build = std::get<BuildTowerCommand>(firstGoodCommand(json));
        QCOMPARE(build.cell.x, 3);
        QCOMPARE(build.cell.y, 1);
        QVERIFY(build.rotation != qtd::core::Rotation::None);
    }

    // Twice the damage for a quarter of the price of a tower beats a second tower.
    static void theGoodBotUpgradesWhenItPaysMore()
    {
        Simulation game(
            qtd::tests::contentOf(qtd::tests::editedJson(R"("footprint": [{"x": 0, "y": 0}])",
                                                         R"("footprint": [{"x": 0, "y": 0}],
            "upgrades": [{"cost": 10, "damageMin": "4", "damageMax": "4"}])")),
            1);
        const std::unique_ptr<Bot> bot = qtd::automation::makeBot(BotKind::Good, 1);
        game.execute(qtd::tests::required(bot->nextCommand(game)));
        QCOMPARE(game.state().towers.size(), std::size_t{1});
        const Command next = qtd::tests::required(bot->nextCommand(game));
        QCOMPARE(std::get<UpgradeTowerCommand>(next).tower, game.state().towers.front().id);
    }

    // Two enemies make no crowd. The spell waits until the runner reaches the last quarter of the
    // path, at 7.5 of 10.
    static void theGoodBotKeepsItsSpellForACrowdOrAnEscape()
    {
        Simulation game = spellGame();
        const std::unique_ptr<Bot> bot = qtd::automation::makeBot(BotKind::Good, 1);
        std::optional<Command> command;
        while (!command && game.state().tick < 30) {
            game.advance(1);
            command = bot->nextCommand(game);
        }
        const CastSpellCommand cast = std::get<CastSpellCommand>(qtd::tests::required(command));
        const qtd::core::Enemy &runner = game.state().enemies.at(1);
        QVERIFY(runner.travelled >= qtd::core::Fixed::fromRaw(75'000));
        QCOMPARE(cast.cell.x, qtd::core::cellAt(game.position(runner)).x);
    }
};

QTEST_APPLESS_MAIN(BotsTest)
#include "tst_bots.moc"
