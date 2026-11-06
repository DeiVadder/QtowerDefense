#include "core/simulation.h"
#include "scenario.h"

#include <QByteArray>
#include <QTest>

using qtd::core::CallWaveCommand;
using qtd::core::CommandResult;
using qtd::core::GameStatus;
using qtd::core::Simulation;
using qtd::tests::contentOf;
using qtd::tests::scenarioJson;

namespace {

/// Two waves of one grunt. The first waits 30 ticks and the second 20, 7 ticks of waiting pay a
/// gold.
Simulation twoWaves()
{
    QByteArray json = scenarioJson();
    json.replace(R"("startGold": 100)", R"("startGold": 100, "earlyCallTicksPerGold": 7)");
    json.replace(
        R"("wave": [{ "enemy": "grunt", "count": 2, "delayTicks": 0, "intervalTicks": 4 }])",
        R"("wave": [{"enemy": "grunt", "count": 1, "delayTicks": 30, "intervalTicks": 1},
                             {"enemy": "grunt", "count": 1, "delayTicks": 20, "intervalTicks": 1}])");
    return {contentOf(json), 1};
}

} // namespace

class WavesTest : public QObject
{
    Q_OBJECT

private slots:
    static void aCalledWaveComesAtTheNextTick()
    {
        Simulation game = twoWaves();
        game.advance(2);
        QVERIFY(game.state().enemies.empty());
        QVERIFY(game.execute(CallWaveCommand{}) == CommandResult::Accepted);
        QVERIFY(game.state().enemies.empty());
        game.tick();
        QCOMPARE(game.state().enemies.size(), std::size_t{1});
    }

    // 28 ticks are left at tick 2, four times 7.
    static void theBonusPaysForTheTicksSaved()
    {
        Simulation game = twoWaves();
        game.advance(2);
        QCOMPARE(game.waveCallBonus(), 4);
        game.execute(CallWaveCommand{});
        QCOMPARE(game.state().gold, 104);
        QCOMPARE(game.state().spawnCountdownTicks, 0);
    }

    static void theBonusIsRoundedDown()
    {
        Simulation game = twoWaves();
        game.advance(3); // 27 ticks left
        QCOMPARE(game.waveCallBonus(), 3);
    }

    static void aSecondCallHasNothingToCall()
    {
        Simulation game = twoWaves();
        game.execute(CallWaveCommand{});
        const quint64 hash = game.hash();
        QVERIFY(game.check(CallWaveCommand{}) == CommandResult::NoWaveWaiting);
        QVERIFY(game.execute(CallWaveCommand{}) == CommandResult::NoWaveWaiting);
        QCOMPARE(game.hash(), hash);
        QCOMPARE(game.waveCallBonus(), 0);
    }

    static void theNextWaveCanBeCalledToo()
    {
        Simulation game = twoWaves();
        game.execute(CallWaveCommand{});
        game.tick(); // the first wave is out, the second waits
        QCOMPARE(game.state().spawnGroup, std::size_t{1});
        QVERIFY(game.execute(CallWaveCommand{}) == CommandResult::Accepted);
        game.tick();
        QCOMPARE(game.state().enemies.size(), std::size_t{2});
    }

    static void noWaveIsCalledInTheMiddleOfAGroup()
    {
        Simulation game(contentOf(scenarioJson()), 1); // two grunts, 4 ticks apart
        game.advance(2);
        QCOMPARE(game.state().spawnedInGroup, 1);
        QVERIFY(game.state().spawnCountdownTicks > 0);
        QVERIFY(game.check(CallWaveCommand{}) == CommandResult::NoWaveWaiting);
    }

    static void afterTheLastWaveThereIsNothingToCall()
    {
        Simulation game = twoWaves();
        game.advance(52);
        QCOMPARE(game.state().spawnGroup, std::size_t{2});
        QVERIFY(game.check(CallWaveCommand{}) == CommandResult::NoWaveWaiting);
    }

    static void aMapMayPayNothing()
    {
        QByteArray json = scenarioJson();
        json.replace(R"("delayTicks": 0)", R"("delayTicks": 30)");
        Simulation game(contentOf(json), 1);
        QCOMPARE(game.waveCallBonus(), 0);
        QVERIFY(game.execute(CallWaveCommand{}) == CommandResult::Accepted);
        QCOMPARE(game.state().gold, 100);
    }

    static void noWaveIsCalledAfterTheEnd()
    {
        QByteArray json = scenarioJson();
        json.replace(R"("lifeCost": 1)", R"("lifeCost": 3)");
        Simulation game(contentOf(json), 1);
        game.advance(100);
        QVERIFY(game.state().status == GameStatus::Lost);
        QVERIFY(game.check(CallWaveCommand{}) == CommandResult::GameOver);
        QCOMPARE(game.waveCallBonus(), 0);
    }

    static void theBonusMustBeSound()
    {
        QByteArray json = scenarioJson();
        json.replace(R"("startGold": 100)", R"("startGold": 100, "earlyCallTicksPerGold": 0)");
        const QString error = qtd::core::parseContent(json).error;
        QVERIFY2(error.startsWith(u"earlyCallTicksPerGold: must be"), qPrintable(error));
    }
};

QTEST_APPLESS_MAIN(WavesTest)
#include "tst_waves.moc"
