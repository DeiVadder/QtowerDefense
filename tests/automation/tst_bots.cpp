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
#include <variant>

using qtd::automation::Bot;
using qtd::automation::BotKind;
using qtd::core::CastSpellCommand;
using qtd::core::Cell;
using qtd::core::Command;
using qtd::core::Simulation;

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
};

QTEST_APPLESS_MAIN(BotsTest)
#include "tst_bots.moc"
