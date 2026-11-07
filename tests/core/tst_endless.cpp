#include "core/replay.h"
#include "core/simulation.h"
#include "scenario.h"

#include <QByteArray>
#include <QTest>

using qtd::core::CallWaveCommand;
using qtd::core::CommandResult;
using qtd::core::Content;
using qtd::core::Fixed;
using qtd::core::GameMode;
using qtd::core::GameStatus;
using qtd::core::Replay;
using qtd::core::ReplayRecorder;
using qtd::core::Simulation;
using qtd::tests::contentOf;
using qtd::tests::editedJson;
using qtd::tests::required;

namespace {

// The scenario sends two grunts of health 4 that cross the map in 20 ticks. Without towers both get
// through, each costs one life.
Content livesOf(int lives)
{
    return contentOf(
        editedJson("\"startLives\": 3", "\"startLives\": " + QByteArray::number(lives)));
}

// One grunt after 30 ticks, a wave can be called early for a gold per 7 ticks.
Content oneLateGrunt()
{
    return contentOf(
        editedJson(R"("startGold": 100)", R"("startGold": 100, "earlyCallTicksPerGold": 7)")
            .replace("\"startLives\": 3", "\"startLives\": 10")
            .replace(R"("count": 2, "delayTicks": 0)", R"("count": 1, "delayTicks": 30)"));
}

/// Runs until the game ends or the round begins, at most the given ticks.
void runToRound(Simulation *game, qint64 round, qint64 ticks = 1000)
{
    for (qint64 done = 0;
         done < ticks && game->state().status == GameStatus::Running && game->state().round < round;
         ++done) {
        game->tick();
    }
}

} // namespace

class TestEndless : public QObject
{
    Q_OBJECT

private slots:
    static void aCampaignIsWonAfterItsWave()
    {
        Simulation game(livesOf(10), 1);
        game.advance(1000);
        QVERIFY(game.state().status == GameStatus::Won);
        QCOMPARE(game.state().round, qint64{0});
    }

    // The next round starts when the field is clear, its enemies come with more health.
    static void theWaveComesAgainStronger()
    {
        Simulation game(livesOf(10), 1, GameMode::Endless);
        runToRound(&game, 1);
        QVERIFY(game.state().status == GameStatus::Running);
        QCOMPARE(game.state().round, qint64{1});
        QCOMPARE(game.state().lives, qint64{8});
        QVERIFY(game.state().enemies.empty());
        game.tick();
        QCOMPARE(game.state().enemies.size(), std::size_t{1});
        QCOMPARE(game.state().enemies.front().health.raw(), Fixed::fromInt(5).raw());
        QCOMPARE(game.state().enemies.front().round, qint64{1});
    }

    static void healthGrowsEachRoundUpToACeiling_data()
    {
        QTest::addColumn<qint64>("round");
        QTest::addColumn<qint64>("health");
        QTest::newRow("first") << qint64{0} << qint64{4};
        QTest::newRow("second") << qint64{1} << qint64{5};
        QTest::newRow("fifth") << qint64{4} << qint64{8};
        QTest::newRow("far away") << qint64{1'000'000} << qint64{400};
    }

    // Content allows a health of 1e9, a hundred times of it must not overflow.
    static void theHighestHealthGrowsWithoutOverflow()
    {
        const Content content =
            contentOf(editedJson(R"("health": "4")", R"("health": "1000000000")"));
        QCOMPARE(qtd::core::fullHealth(content, 0, 1'000'000).raw(),
                 Fixed::fromInt(100'000'000'000).raw());
    }

    // While the last enemies of a round walk there is nothing to call, then the next round can be.
    static void theNextRoundCanBeCalledEarly()
    {
        Simulation game(oneLateGrunt(), 1, GameMode::Endless);
        game.advance(32);
        QCOMPARE(game.state().enemies.size(), std::size_t{1});
        QVERIFY(game.check(CallWaveCommand{}) == CommandResult::NoWaveWaiting);
        runToRound(&game, 1);
        QCOMPARE(game.state().round, qint64{1});
        QCOMPARE(game.waveCallBonus(), qint64{4});
        const qint64 gold = game.state().gold;
        QVERIFY(game.execute(CallWaveCommand{}) == CommandResult::Accepted);
        QCOMPARE(game.state().gold, gold + 4);
        game.tick();
        QCOMPARE(game.state().enemies.size(), std::size_t{1});
        QCOMPARE(game.state().enemies.front().round, qint64{1});
    }

    static void healthGrowsEachRoundUpToACeiling()
    {
        QFETCH(const qint64, round);
        QFETCH(const qint64, health);
        QCOMPARE(qtd::core::fullHealth(livesOf(3), 0, round).raw(), Fixed::fromInt(health).raw());
    }

    static void anEndlessGameEndsOnlyWithTheLives()
    {
        Simulation game(livesOf(3), 1, GameMode::Endless);
        game.advance(10'000);
        QVERIFY(game.state().status == GameStatus::Lost);
        QCOMPARE(game.state().lives, qint64{0});
        QCOMPARE(game.state().round, qint64{1});
    }

    // Regeneration stops at the health the enemy came with, not at the health of its type.
    static void regenerationKeepsTheHealthOfTheRound()
    {
        const Content content =
            contentOf(editedJson(R"("lifeCost": 1)", R"("lifeCost": 1, "regeneration": "1")")
                          .replace("\"startLives\": 3", "\"startLives\": 10"));
        Simulation game(content, 1, GameMode::Endless);
        runToRound(&game, 1);
        game.advance(3);
        QVERIFY(!game.state().enemies.empty());
        QCOMPARE(game.state().enemies.front().health.raw(), Fixed::fromInt(5).raw());
    }

    static void theHashKnowsTheMode()
    {
        const Simulation campaign(livesOf(3), 1);
        const Simulation endless(livesOf(3), 1, GameMode::Endless);
        QVERIFY(campaign.hash() != endless.hash());
    }

    // A campaign writes no mode, its files stay as they were.
    static void aReplayKeepsTheMode_data()
    {
        QTest::addColumn<GameMode>("mode");
        QTest::addColumn<bool>("written");
        QTest::newRow("campaign") << GameMode::Campaign << false;
        QTest::newRow("endless") << GameMode::Endless << true;
    }

    static void aReplayKeepsTheMode()
    {
        QFETCH(const GameMode, mode);
        QFETCH(const bool, written);
        ReplayRecorder recorder(livesOf(10), QStringLiteral("scenario.json"), 1, mode);
        recorder.advance(200);
        const QByteArray json = qtd::core::toJson(recorder.replay());
        QCOMPARE(json.contains("\"mode\""), written);
        const Replay replay = required(qtd::core::parseReplay(json).replay);
        QVERIFY(replay.mode == mode);
        QCOMPARE(qtd::core::verifyReplay(livesOf(10), replay), QString());
    }

    static void aSavedEndlessGameGoesOnEndless()
    {
        ReplayRecorder recorder(livesOf(10), QStringLiteral("scenario.json"), 1, GameMode::Endless);
        recorder.advance(60);
        const qtd::core::ResumeResult resumed =
            qtd::core::resumeGame(livesOf(10), recorder.replay());
        QVERIFY2(resumed.game != nullptr, qPrintable(resumed.error));
        QVERIFY(resumed.game->simulation().state().mode == GameMode::Endless);
        QCOMPARE(resumed.game->simulation().hash(), recorder.simulation().hash());
    }

    static void anUnknownModeIsRejected()
    {
        ReplayRecorder recorder(livesOf(10), QStringLiteral("scenario.json"), 1, GameMode::Endless);
        const QByteArray json =
            qtd::core::toJson(recorder.replay()).replace("\"endless\"", "\"forever\"");
        const qtd::core::ReplayLoadResult result = qtd::core::parseReplay(json);
        QVERIFY(!result.replay);
        QVERIFY2(result.error.contains(QStringLiteral("mode")), qPrintable(result.error));
    }
};

QTEST_APPLESS_MAIN(TestEndless)
#include "tst_endless.moc"
