#include "../core/scenario.h"
#include "automation/headless_runner.h"
#include "core/replay.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

#include <cstddef>
#include <set>

using qtd::automation::HeadlessArguments;
using qtd::automation::HeadlessOptions;
using qtd::automation::HeadlessOutcome;
using qtd::automation::parseHeadlessArguments;
using qtd::automation::runHeadless;
using qtd::tests::required;

using namespace Qt::StringLiterals;

namespace {

QString mapFile()
{
    return QDir(QStringLiteral(QTD_CONTENT_DIR)).filePath(u"training_ground.json"_s);
}

QString goldenFile(const QString &name)
{
    return QDir(QStringLiteral(QTD_GOLDEN_DIR)).filePath(name);
}

HeadlessOptions options()
{
    HeadlessOptions result;
    result.map = mapFile();
    return result;
}

/// A fuzz game whose state check calls tick 5 broken.
HeadlessOptions brokenAtTick5()
{
    HeadlessOptions game = options();
    game.bot = qtd::automation::BotKind::Fuzz;
    game.seed = 11;
    game.check = [](const qtd::core::Content &, const qtd::core::SimulationState &state) {
        return state.tick == 5 ? u"broken"_s : QString();
    };
    return game;
}

QJsonObject parsed(const QByteArray &json)
{
    return QJsonDocument::fromJson(json).object();
}

QByteArray fileText(const QString &name)
{
    QFile file(name);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

QStringList commandLine(const QStringList &arguments)
{
    return QStringList{u"qtd_headless"_s} + arguments;
}

} // namespace

class HeadlessRunnerTest : public QObject
{
    Q_OBJECT

private slots:
    void allOptionsAreRead()
    {
        const HeadlessArguments result =
            parseHeadlessArguments(commandLine({u"--map"_s,
                                                u"m"_s,
                                                u"--seed"_s,
                                                u"18446744073709551615"_s,
                                                u"--runs"_s,
                                                u"3"_s,
                                                u"--script"_s,
                                                u"s"_s,
                                                u"--output"_s,
                                                u"o"_s,
                                                u"--max-ticks"_s,
                                                u"9"_s,
                                                u"--bot"_s,
                                                u"idle"_s,
                                                u"--benchmark"_s}));
        QCOMPARE(result.error, QString());
        const HeadlessOptions read = required(result.options);
        QCOMPARE(read.map, u"m"_s);
        QCOMPARE(read.seed.value_or(0), 18446744073709551615ULL);
        QCOMPARE(read.runs, 3);
        QCOMPARE(read.script, u"s"_s);
        QCOMPARE(read.output, u"o"_s);
        QCOMPARE(read.maxTicks, 9);
        QVERIFY(read.benchmark);
    }

    void helpAndBuildInfoNeedNoMap()
    {
        QVERIFY(parseHeadlessArguments(commandLine({u"--help"_s})).help);
        QVERIFY(parseHeadlessArguments(commandLine({u"--help"_s})).helpText.contains(u"--map"_s));
        QVERIFY(parseHeadlessArguments(commandLine({u"--build-info"_s})).buildInfo);
    }

    void wrongCommandLinesGiveAnError_data()
    {
        QTest::addColumn<QStringList>("arguments");
        QTest::addColumn<QString>("error");
        const QStringList map{u"--map"_s, u"m"_s};
        QTest::newRow("no map") << QStringList() << u"--map is required"_s;
        QTest::newRow("unknown option")
            << QStringList{u"--colour"_s} << u"Unknown option 'colour'."_s;
        QTest::newRow("stray argument")
            << QStringList{u"more"_s} << u"unexpected argument \"more\""_s;
        QTest::newRow("seed") << map + QStringList{u"--seed"_s, u"-1"_s}
                              << u"--seed needs a number, not \"-1\""_s;
        QTest::newRow("runs") << map + QStringList{u"--runs"_s, u"x"_s}
                              << u"--runs needs a number, not \"x\""_s;
        QTest::newRow("no runs") << map + QStringList{u"--runs"_s, u"0"_s}
                                 << u"--runs must be at least 1"_s;
        QTest::newRow("max ticks") << map + QStringList{u"--max-ticks"_s, u"-1"_s}
                                   << u"--max-ticks must not be negative"_s;
        QTest::newRow("bot") << map + QStringList{u"--bot"_s, u"clever"_s}
                             << u"unknown bot \"clever\", available: idle, naive, fuzz"_s;
        QTest::newRow("record many")
            << map + QStringList{u"--runs"_s, u"2"_s, u"--record"_s, u"r"_s}
            << u"--record takes a single run"_s;
        QTest::newRow("replay and seed")
            << map + QStringList{u"--replay"_s, u"r"_s, u"--seed"_s, u"1"_s}
            << u"--replay brings its own seed and commands"_s;
        QTest::newRow("replay and max ticks")
            << map + QStringList{u"--replay"_s, u"r"_s, u"--max-ticks"_s, u"9"_s}
            << u"--replay runs to its end and measures nothing"_s;
        QTest::newRow("replay and benchmark")
            << map + QStringList{u"--replay"_s, u"r"_s, u"--benchmark"_s}
            << u"--replay runs to its end and measures nothing"_s;
        QTest::newRow("bot and script")
            << map + QStringList{u"--bot"_s, u"naive"_s, u"--script"_s, u"s"_s}
            << u"--bot and --script both want to play"_s;
        QTest::newRow("control and bot") << QStringList{u"--control"_s, u"--bot"_s, u"fuzz"_s}
                                         << u"--control takes its games from the protocol"_s;
        QTest::newRow("control and output") << QStringList{u"--control"_s, u"--output"_s, u"o"_s}
                                            << u"--control takes its games from the protocol"_s;
        QTest::newRow("replay and bot")
            << map + QStringList{u"--replay"_s, u"r"_s, u"--bot"_s, u"fuzz"_s}
            << u"--replay brings its own seed and commands"_s;
        QTest::newRow("control and map") << map + QStringList{u"--control"_s}
                                         << u"--control takes its games from the protocol"_s;
        QTest::newRow("replay and script")
            << map + QStringList{u"--replay"_s, u"r"_s, u"--script"_s, u"s"_s}
            << u"--replay brings its own seed and commands"_s;
    }

    void wrongCommandLinesGiveAnError()
    {
        QFETCH(const QStringList, arguments);
        QFETCH(const QString, error);
        const HeadlessArguments result = parseHeadlessArguments(commandLine(arguments));
        QVERIFY(!result.options);
        QCOMPARE(result.error, error);
    }

    void anIdleGameIsLost()
    {
        const HeadlessOutcome outcome = runHeadless(options());
        QCOMPARE(outcome.exitCode, 0);
        const QJsonObject report = parsed(outcome.json);
        QCOMPARE(report["map"_L1].toString(), u"training_ground.json"_s);
        const QJsonObject game = report["games"_L1].toArray().first().toObject();
        QCOMPARE(game["status"_L1].toString(), u"lost"_s);
        QCOMPARE(game["lives"_L1].toInteger(), 0);
        QCOMPARE(game["gold"_L1].toInteger(), 120);
        QCOMPARE(game["seed"_L1].toString(), u"1"_s);
        QCOMPARE(game["commands"_L1].toInteger(), 0);
        QVERIFY(!report.contains("benchmark"_L1));
    }

    void theReportIsTheSameEveryTime()
    {
        HeadlessOptions game = options();
        game.script = goldenFile(u"training_ground_rejected.replay.json"_s);
        QCOMPARE(runHeadless(game).json, runHeadless(game).json);
    }

    void runsCountTheSeedsUp()
    {
        HeadlessOptions game = options();
        game.seed = 5;
        game.runs = 3;
        const QJsonArray games = parsed(runHeadless(game).json)["games"_L1].toArray();
        QCOMPARE(games.size(), 3);
        QCOMPARE(games[2].toObject()["seed"_L1].toString(), u"7"_s);
    }

    void differentSeedsGiveDifferentGames()
    {
        HeadlessOptions game = options();
        game.script = goldenFile(u"training_ground_defence.replay.json"_s);
        game.runs = 2;
        const QJsonArray games = parsed(runHeadless(game).json)["games"_L1].toArray();
        QVERIFY(games[0].toObject()["hash"_L1] != games[1].toObject()["hash"_L1]);
    }

    void aScriptIsPlayedAndCounted()
    {
        HeadlessOptions game = options();
        game.seed = 18446744073709551615ULL;
        game.script = goldenFile(u"training_ground_rejected.replay.json"_s);
        const QJsonObject report =
            parsed(runHeadless(game).json)["games"_L1].toArray().first().toObject();
        QCOMPARE(report["commands"_L1].toInteger(), 9);
        QCOMPARE(report["rejected"_L1].toInteger(), 7);
        QCOMPARE(report["status"_L1].toString(), u"won"_s);
    }

    void maxTicksStopsAGame()
    {
        HeadlessOptions game = options();
        game.maxTicks = 10;
        const QJsonObject report =
            parsed(runHeadless(game).json)["games"_L1].toArray().first().toObject();
        QCOMPARE(report["status"_L1].toString(), u"running"_s);
        QCOMPARE(report["tick"_L1].toInteger(), 10);
    }

    /// The golden game has its result from seed 20261104, the seed of the file.
    void aScriptBringsItsSeed()
    {
        HeadlessOptions game = options();
        game.script = goldenFile(u"training_ground_defence.replay.json"_s);
        const QJsonObject report =
            parsed(runHeadless(game).json)["games"_L1].toArray().first().toObject();
        QCOMPARE(report["seed"_L1].toString(), u"20261104"_s);
        QCOMPARE(report["hash"_L1].toString(), u"353ffc8e53a01e80"_s);
    }

    void theSeedOptionBeatsTheScript()
    {
        HeadlessOptions game = options();
        game.seed = 3;
        game.script = goldenFile(u"training_ground_defence.replay.json"_s);
        const QJsonObject report =
            parsed(runHeadless(game).json)["games"_L1].toArray().first().toObject();
        QCOMPARE(report["seed"_L1].toString(), u"3"_s);
    }

    /// A build at tick 500 lies behind a stop at tick 10, so the gold must stay untouched.
    void commandsBehindMaxTicksAreLeftOut()
    {
        const QTemporaryDir directory;
        HeadlessOptions game = options();
        game.script = directory.filePath(u"late.json"_s);
        QFile file(game.script);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(R"({"formatVersion": 1, "content": "c", "seed": "1", "commands": [
            {"tick": 10, "type": "buildTower", "x": 4, "y": 4, "towerType": 0},
            {"tick": 500, "type": "buildTower", "x": 6, "y": 0, "towerType": 0}]})");
        file.close();
        game.maxTicks = 10;
        const QJsonObject report =
            parsed(runHeadless(game).json)["games"_L1].toArray().first().toObject();
        QCOMPARE(report["commands"_L1].toInteger(), 1);
        QCOMPARE(report["gold"_L1].toInteger(), 70);
        QCOMPARE(report["tick"_L1].toInteger(), 10);
    }

    void theNaiveBotDefendsTheTrainingGround()
    {
        HeadlessOptions game = options();
        game.bot = qtd::automation::BotKind::Naive;
        game.seed = 7;
        const QJsonObject report =
            parsed(runHeadless(game).json)["games"_L1].toArray().first().toObject();
        QCOMPARE(report["status"_L1].toString(), u"won"_s);
        QCOMPARE(report["rejected"_L1].toInteger(), 0);
        QVERIFY(report["commands"_L1].toInteger() >= 2); // 120 gold pay for two towers at once
    }

    void aFuzzRunIsRepeatable()
    {
        HeadlessOptions game = options();
        game.bot = qtd::automation::BotKind::Fuzz;
        game.seed = 3;
        game.runs = 10;
        const HeadlessOutcome first = runHeadless(game);
        QCOMPARE(first.exitCode, 0);
        QCOMPARE(runHeadless(game).json, first.json);
        qint64 commands = 0;
        qint64 rejected = 0;
        const QJsonArray games = parsed(first.json)["games"_L1].toArray();
        for (const QJsonValueConstRef report : games) {
            commands += report.toObject()["commands"_L1].toInteger();
            rejected += report.toObject()["rejected"_L1].toInteger();
        }
        QVERIFY(rejected > 0);
        QVERIFY(commands > rejected);
    }

    void aBrokenGameStaysRepeatable()
    {
        const QTemporaryDir directory;
        HeadlessOptions game = brokenAtTick5();
        game.record = directory.filePath(u"broken.replay.json"_s);
        const HeadlessOutcome outcome = runHeadless(game);
        QCOMPARE(outcome.exitCode, 1);
        const QJsonObject report = parsed(outcome.json);
        QCOMPARE(report["error"_L1].toString(), u"tick 5: broken"_s);
        QCOMPARE(report["seed"_L1].toString(), u"11"_s);
        QCOMPARE(report["replay"_L1].toString(), game.record);

        HeadlessOptions healthy = options();
        healthy.replay = game.record;
        QCOMPARE(runHeadless(healthy).exitCode, 0);

        HeadlessOptions check = healthy;
        check.check = game.check;
        const HeadlessOutcome repeated = runHeadless(check);
        QCOMPARE(repeated.exitCode, 1);
        QCOMPARE(parsed(repeated.json)["invariant"_L1].toString(), u"tick 5: broken"_s);
        QVERIFY(parsed(repeated.json)["matches"_L1].toBool());
    }

    void aBrokenGameFindsAPlaceOfItsOwn()
    {
        const QTemporaryDir directory;
        const QString before = QDir::currentPath();
        QVERIFY(QDir::setCurrent(directory.path()));
        const HeadlessOutcome outcome = runHeadless(brokenAtTick5());
        const bool written = QFile::exists(u"failure-11.replay.json"_s);
        QVERIFY(QDir::setCurrent(before));
        QCOMPARE(outcome.exitCode, 1);
        QVERIFY(written);
    }

    void aBrokenGameWithoutAPlaceForItsReplaySaysSo()
    {
        HeadlessOptions game = brokenAtTick5();
        game.record = u"no/such/folder/x"_s;
        const HeadlessOutcome outcome = runHeadless(game);
        QCOMPARE(outcome.exitCode, 1);
        QVERIFY(parsed(outcome.json)["error"_L1].toString().startsWith(
            u"tick 5: broken, and no replay"_s));
    }

    void aBrokenStartIsFound()
    {
        HeadlessOptions game = brokenAtTick5();
        game.check = [](const qtd::core::Content &, const qtd::core::SimulationState &) {
            return u"broken"_s;
        };
        const QTemporaryDir directory;
        game.record = directory.filePath(u"broken.replay.json"_s);
        QCOMPARE(parsed(runHeadless(game).json)["error"_L1].toString(), u"tick 0: broken"_s);
    }

    void fuzzGamesEndBothWays()
    {
        HeadlessOptions game = options();
        game.bot = qtd::automation::BotKind::Fuzz;
        game.seed = 1;
        game.runs = 40;
        const HeadlessOutcome outcome = runHeadless(game);
        QCOMPARE(outcome.exitCode, 0);
        std::set<QString> ends;
        const QJsonArray games = parsed(outcome.json)["games"_L1].toArray();
        for (const QJsonValueConstRef report : games) {
            ends.insert(report.toObject()["status"_L1].toString());
        }
        QCOMPARE(ends.size(), std::size_t{2});
    }

    void aRecordedFuzzGamePassesItsCheck()
    {
        const QTemporaryDir directory;
        HeadlessOptions game = options();
        game.bot = qtd::automation::BotKind::Fuzz;
        game.seed = 5;
        game.record = directory.filePath(u"fuzz.replay.json"_s);
        QCOMPARE(runHeadless(game).exitCode, 0);

        HeadlessOptions check = options();
        check.replay = game.record;
        QCOMPARE(runHeadless(check).exitCode, 0);
    }

    void aRecordedGamePassesItsCheck()
    {
        const QTemporaryDir directory;
        HeadlessOptions game = options();
        game.seed = 12;
        game.script = goldenFile(u"training_ground_defence.replay.json"_s);
        game.record = directory.filePath(u"game.replay.json"_s);
        QCOMPARE(runHeadless(game).exitCode, 0);

        HeadlessOptions check = options();
        check.replay = game.record;
        const HeadlessOutcome outcome = runHeadless(check);
        QCOMPARE(outcome.exitCode, 0);
        QVERIFY(parsed(outcome.json)["matches"_L1].toBool());
    }

    void aGoldenReplayPassesItsCheck()
    {
        HeadlessOptions check = options();
        check.replay = goldenFile(u"training_ground_defence.replay.json"_s);
        const HeadlessOutcome outcome = runHeadless(check);
        QCOMPARE(outcome.exitCode, 0);
        const QJsonObject report = parsed(outcome.json);
        QVERIFY(report["matches"_L1].toBool());
        QCOMPARE(report["status"_L1].toString(), u"won"_s);
    }

    void aDifferenceFailsTheCheck()
    {
        const QTemporaryDir directory;
        QByteArray json = fileText(goldenFile(u"training_ground_idle.replay.json"_s));
        const qtd::core::Replay replay = required(qtd::core::parseReplay(json).replay);
        const QByteArray last = qtd::core::hashToText(replay.checkpoints.back().hash).toLatin1();
        json.replace(last, "0123456789abcdef");
        HeadlessOptions check = options();
        check.replay = directory.filePath(u"changed.replay.json"_s);
        QFile file(check.replay);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(json);
        file.close();

        const HeadlessOutcome outcome = runHeadless(check);
        QCOMPARE(outcome.exitCode, 1);
        const QJsonObject report = parsed(outcome.json);
        QVERIFY(!report["matches"_L1].toBool());
        QVERIFY(report["difference"_L1].toString().contains(u"0123456789abcdef"_s));
    }

    void theBenchmarkReportsTicks()
    {
        HeadlessOptions game = options();
        game.benchmark = true;
        const QJsonObject report = parsed(runHeadless(game).json)["benchmark"_L1].toObject();
        QCOMPARE(report["ticks"_L1].toInteger(), 681); // the idle game of training_ground
        QVERIFY(report["nanoseconds"_L1].toInteger() > 0);
        QVERIFY(report["ticksPerSecond"_L1].toInteger() > 0);
    }

    void theReportGoesToAFile()
    {
        const QTemporaryDir directory;
        HeadlessOptions game = options();
        game.output = directory.filePath(u"report.json"_s);
        const HeadlessOutcome outcome = runHeadless(game);
        QCOMPARE(fileText(game.output), outcome.json + '\n');
    }

    void aProblemWithTheMapReplacesAnOldReport()
    {
        const QTemporaryDir directory;
        HeadlessOptions game = options();
        game.output = directory.filePath(u"report.json"_s);
        QCOMPARE(runHeadless(game).exitCode, 0);
        game.map = directory.filePath(u"missing.json"_s);
        const HeadlessOutcome outcome = runHeadless(game);
        QCOMPARE(outcome.exitCode, 1);
        QCOMPARE(fileText(game.output), outcome.json + '\n');
    }

    void problemsEndUpInTheReport()
    {
        for (QString HeadlessOptions::*const file : {&HeadlessOptions::map,
                                                     &HeadlessOptions::script,
                                                     &HeadlessOptions::replay,
                                                     &HeadlessOptions::record,
                                                     &HeadlessOptions::output}) {
            HeadlessOptions game = options();
            game.maxTicks = 1;
            game.*file = u"missing/dir/file.json"_s;
            const HeadlessOutcome outcome = runHeadless(game);
            QCOMPARE(outcome.exitCode, 1);
            QVERIFY(parsed(outcome.json)["error"_L1].toString().contains(u"missing"_s));
        }
    }
};

QTEST_APPLESS_MAIN(HeadlessRunnerTest)
#include "tst_headless_runner.moc"
