#include "core/content_loader.h"
#include "core/replay.h"
#include "core/simulation.h"
#include "scenario.h"

#include <QByteArray>
#include <QTest>

#include <algorithm>

using namespace Qt::StringLiterals;
using qtd::core::BuildTowerCommand;
using qtd::core::BuyCardCommand;
using qtd::core::CollectCoinCommand;
using qtd::core::CommandResult;
using qtd::core::DiscardCardCommand;
using qtd::core::parseContent;
using qtd::core::RollCardCommand;
using qtd::core::rollCost;
using qtd::core::SellTowerCommand;
using qtd::core::Simulation;
using qtd::tests::contentOf;
using qtd::tests::editedJson;
using qtd::tests::required;
using qtd::tests::scenarioJson;

namespace {

constexpr qint64 kPrice = 40; // of the tower in the scenario
constexpr qint64 kBounty = 7;

/// The scenario with this much gold. The second enemy comes long after the first is dead.
Simulation richGame(qint64 gold = 1000)
{
    QByteArray json = scenarioJson();
    json.replace(R"("startGold": 100)", "\"startGold\": " + QByteArray::number(gold));
    json.replace(R"("intervalTicks": 4)", R"("intervalTicks": 300)");
    return Simulation(contentOf(json), 7);
}

/// The error of the scenario with this key added to its rules.
QString errorOf(const QByteArray &key)
{
    return parseContent(editedJson(R"("startGold": 100)", R"("startGold": 100, )" + key)).error;
}

/// Runs until the first enemy is dead, with a tower that shoots it.
void killAnEnemy(Simulation *game)
{
    QVERIFY(game->execute(BuildTowerCommand{{1, 1}, 0}) == CommandResult::Accepted);
    for (int tick = 0; tick < 200 && game->state().coins.empty(); ++tick) {
        game->tick();
    }
    QCOMPARE(game->state().coins.size(), std::size_t{1});
}

} // namespace

class CardsTest : public QObject
{
    Q_OBJECT

private slots:
    static void aRollIsCheaperThanAnyTower()
    {
        Simulation game = richGame();
        QCOMPARE(rollCost(game.content()), kPrice * 80 / 100);
        QVERIFY(rollCost(game.content()) < kPrice);
        QVERIFY(game.execute(RollCardCommand{}) == CommandResult::Accepted);
        QCOMPARE(game.state().gold, 1000 - rollCost(game.content()));
        QCOMPARE(game.state().hand.size(), std::size_t{1});
        QCOMPARE(game.state().hand.front().paid, rollCost(game.content()));
    }

    static void aRollNeedsTheGold()
    {
        Simulation game = richGame(kPrice * 80 / 100 - 1);
        QVERIFY(game.execute(RollCardCommand{}) == CommandResult::NotEnoughGold);
        QVERIFY(game.state().hand.empty());
        QCOMPARE(game.state().gold, kPrice * 80 / 100 - 1);
    }

    /// The draw belongs to the game: the same seed gives the same cards after a reload.
    static void aRollMovesTheRandomNumbers()
    {
        Simulation first = richGame();
        Simulation second = richGame();
        QVERIFY(first.execute(RollCardCommand{}) == CommandResult::Accepted);
        QVERIFY(first.hash() != second.hash());
        QVERIFY(second.execute(RollCardCommand{}) == CommandResult::Accepted);
        QCOMPARE(first.hash(), second.hash());
    }

    static void aBoughtCardCostsTheFullPrice()
    {
        Simulation game = richGame();
        QVERIFY(game.execute(BuyCardCommand{0}) == CommandResult::Accepted);
        QCOMPARE(game.state().gold, 1000 - kPrice);
        QCOMPARE(game.state().hand.front().type, std::size_t{0});
        QCOMPARE(game.state().hand.front().paid, kPrice);
    }

    static void aCardNeedsATowerAndTheGold()
    {
        Simulation game = richGame(39);
        QVERIFY(game.execute(BuyCardCommand{1}) == CommandResult::UnknownTowerType);
        QVERIFY(game.execute(BuyCardCommand{-1}) == CommandResult::UnknownTowerType);
        QVERIFY(game.execute(BuyCardCommand{0}) == CommandResult::NotEnoughGold);
    }

    static void theHandHoldsFiveCards()
    {
        Simulation game = richGame();
        for (std::size_t card = 0; card < qtd::core::kHandLimit; ++card) {
            QVERIFY(game.execute(RollCardCommand{}) == CommandResult::Accepted);
        }
        const qint64 gold = game.state().gold;
        QVERIFY(game.execute(RollCardCommand{}) == CommandResult::HandFull);
        QVERIFY(game.execute(BuyCardCommand{0}) == CommandResult::HandFull);
        QCOMPARE(game.state().gold, gold);
    }

    static void aCardBuildsForFree()
    {
        Simulation game = richGame();
        QVERIFY(game.execute(BuyCardCommand{0}) == CommandResult::Accepted);
        const qint64 gold = game.state().gold;
        QVERIFY(game.execute(BuildTowerCommand{{1, 1}, 0}) == CommandResult::Accepted);
        QCOMPARE(game.state().gold, gold);
        QVERIFY(game.state().hand.empty());
    }

    static void aCardBuildsWithoutAnyGold()
    {
        Simulation game = richGame(kPrice);
        QVERIFY(game.execute(BuyCardCommand{0}) == CommandResult::Accepted);
        QCOMPARE(game.state().gold, 0);
        QVERIFY(game.check(BuildTowerCommand{{1, 1}, 0}) == CommandResult::Accepted);
    }

    static void aRejectedBuildKeepsTheCard()
    {
        Simulation game = richGame();
        QVERIFY(game.execute(BuyCardCommand{0}) == CommandResult::Accepted);
        QVERIFY(game.execute(BuildTowerCommand{{0, 0}, 0}) == CommandResult::CellBlocked);
        QCOMPARE(game.state().hand.size(), std::size_t{1});
    }

    static void withoutACardABuildPaysThePrice()
    {
        Simulation game = richGame();
        QVERIFY(game.execute(BuildTowerCommand{{1, 1}, 0}) == CommandResult::Accepted);
        QCOMPARE(game.state().gold, 1000 - kPrice);
    }

    /// Otherwise rolling and selling would print gold.
    static void aTowerSellsForWhatItsCardCost()
    {
        Simulation game = richGame();
        QVERIFY(game.execute(RollCardCommand{}) == CommandResult::Accepted);
        QVERIFY(game.execute(BuildTowerCommand{{1, 1}, 0}) == CommandResult::Accepted);
        const qtd::core::Tower &tower = game.state().towers.front();
        QCOMPARE(tower.spent, rollCost(game.content()));
        const qint64 before = game.state().gold;
        QVERIFY(game.execute(SellTowerCommand{tower.id}) == CommandResult::Accepted);
        QVERIFY(game.state().gold - before < rollCost(game.content()));
    }

    static void aDiscardedCardIsGone()
    {
        Simulation game = richGame();
        QVERIFY(game.execute(BuyCardCommand{0}) == CommandResult::Accepted);
        QVERIFY(game.execute(RollCardCommand{}) == CommandResult::Accepted);
        const qint64 gold = game.state().gold;
        QVERIFY(game.execute(DiscardCardCommand{0}) == CommandResult::Accepted);
        QCOMPARE(game.state().gold, gold);
        QCOMPARE(game.state().hand.size(), std::size_t{1});
        QCOMPARE(game.state().hand.front().paid, rollCost(game.content()));
    }

    static void onlyACardCanBeDiscarded()
    {
        Simulation game = richGame();
        QVERIFY(game.execute(DiscardCardCommand{0}) == CommandResult::UnknownCard);
        QVERIFY(game.execute(DiscardCardCommand{-1}) == CommandResult::UnknownCard);
    }

    static void aDefeatedEnemyDropsACoin()
    {
        Simulation game = richGame();
        killAnEnemy(&game);
        const qtd::core::BonusCoin coin = game.state().coins.front();
        QCOMPARE(coin.value, kBounty * 25 / 100);
        QVERIFY(coin.place.x > qtd::core::Fixed());
    }

    static void aCoinPaysOnce()
    {
        Simulation game = richGame();
        killAnEnemy(&game);
        const qtd::core::BonusCoin coin = game.state().coins.front();
        const qint64 gold = game.state().gold;
        QVERIFY(game.execute(CollectCoinCommand{coin.id}) == CommandResult::Accepted);
        QCOMPARE(game.state().gold, gold + coin.value);
        QVERIFY(game.execute(CollectCoinCommand{coin.id}) == CommandResult::UnknownCoin);
        QCOMPARE(game.state().gold, gold + coin.value);
    }

    static void aCoinFades()
    {
        Simulation game = richGame();
        killAnEnemy(&game);
        const qint64 id = game.state().coins.front().id;
        const qint64 ticks = game.state().coins.front().ticksLeft;
        QCOMPARE(ticks, game.content().coinTicks);
        game.advance(ticks - 1);
        QVERIFY(std::ranges::any_of(
            game.state().coins, [id](const qtd::core::BonusCoin &coin) { return coin.id == id; }));
        game.tick();
        QVERIFY(std::ranges::none_of(
            game.state().coins, [id](const qtd::core::BonusCoin &coin) { return coin.id == id; }));
    }

    /// Replays name towers by their ids, coins must not move them.
    static void coinsLeaveTheIdsOfTowersAlone()
    {
        Simulation game = richGame();
        killAnEnemy(&game);
        while (game.state().enemies.empty()) {
            game.tick();
        }
        const qint64 next = game.state().nextEntityId;
        const qint64 coin = game.state().nextCoinId;
        while (game.state().nextCoinId == coin) {
            game.tick();
        }
        QCOMPARE(game.state().nextEntityId, next);
    }

    static void aCommandWaitsForNoFinishedGame()
    {
        Simulation game = richGame();
        game.advance(100000);
        QVERIFY(game.state().status != qtd::core::GameStatus::Running);
        QVERIFY(game.execute(RollCardCommand{}) == CommandResult::GameOver);
        QVERIFY(game.execute(BuyCardCommand{0}) == CommandResult::GameOver);
        QVERIFY(game.execute(DiscardCardCommand{0}) == CommandResult::GameOver);
        QVERIFY(game.execute(CollectCoinCommand{1}) == CommandResult::GameOver);
    }

    static void theCommandsSurviveAReplayFile()
    {
        qtd::core::Replay replay;
        replay.content = u"map.json"_s;
        replay.checkpoints.push_back({10, 2});
        replay.commands.push_back({1, RollCardCommand{}, CommandResult::Accepted});
        replay.commands.push_back({2, BuyCardCommand{3}, CommandResult::HandFull});
        replay.commands.push_back({3, DiscardCardCommand{4}, CommandResult::UnknownCard});
        replay.commands.push_back({4, CollectCoinCommand{5}, CommandResult::UnknownCoin});
        const qtd::core::Replay read =
            required(qtd::core::parseReplay(qtd::core::toJson(replay)).replay);
        QCOMPARE(read.commands.size(), std::size_t{4});
        QVERIFY(std::holds_alternative<RollCardCommand>(read.commands.at(0).command));
        QCOMPARE(std::get<BuyCardCommand>(read.commands.at(1).command).towerType, 3);
        QCOMPARE(std::get<DiscardCardCommand>(read.commands.at(2).command).slot, 4);
        QCOMPARE(std::get<CollectCoinCommand>(read.commands.at(3).command).coin, 5);
        QVERIFY(read.commands.at(3).result == CommandResult::UnknownCoin);
    }

    static void theRulesHaveTheirLimits_data()
    {
        QTest::addColumn<QByteArray>("key");
        QTest::addColumn<QString>("error");
        QTest::newRow("a roll for free") << QByteArray(R"("rollPercent": 0)") << u"rollPercent"_s;
        QTest::newRow("a roll at full price")
            << QByteArray(R"("rollPercent": 100)") << u"rollPercent"_s;
        QTest::newRow("a coin above its bounty")
            << QByteArray(R"("coinPercent": 101)") << u"coinPercent"_s;
        QTest::newRow("a coin gone at once") << QByteArray(R"("coinTicks": 0)") << u"coinTicks"_s;
    }

    static void theRulesHaveTheirLimits()
    {
        QFETCH(const QByteArray, key);
        QFETCH(const QString, error);
        QVERIFY2(errorOf(key).startsWith(error), qPrintable(errorOf(key)));
    }
};

QTEST_GUILESS_MAIN(CardsTest)
#include "tst_cards.moc"
