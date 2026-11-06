#include "core/replay.h"
#include "scenario.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <utility>
#include <variant>
#include <vector>

using qtd::core::BuildTowerCommand;
using qtd::core::Checkpoint;
using qtd::core::CommandResult;
using qtd::core::Content;
using qtd::core::GameStatus;
using qtd::core::hashFromText;
using qtd::core::hashToText;
using qtd::core::loadReplayFile;
using qtd::core::loadScriptFile;
using qtd::core::parseReplay;
using qtd::core::parseScript;
using qtd::core::Replay;
using qtd::core::ReplayLoadResult;
using qtd::core::ReplayPlayer;
using qtd::core::ReplayRecorder;
using qtd::core::verifyReplay;
using qtd::tests::contentOf;
using qtd::tests::editedJson;
using qtd::tests::required;
using qtd::tests::scenarioJson;

using namespace Qt::StringLiterals;

namespace {

Content scenario()
{
    return contentOf(scenarioJson());
}

/// The scenario with enemies that need 200 ticks for the path, so that it lasts some seconds.
Content slowScenario()
{
    return contentOf(editedJson(R"("speedPerTick": "0.5")", R"("speedPerTick": "0.05")"));
}

/// A whole game of the scenario: one tower at tick 0, a rejected command at tick 2 and a second
/// tower at tick 3. The tower at (5, 1) wins it.
Replay recordedGame()
{
    ReplayRecorder recorder(scenario(), u"scenario.json"_s, 7);
    recorder.execute(BuildTowerCommand{{5, 1}, 0});
    recorder.advance(2);
    recorder.execute(BuildTowerCommand{{5, 1}, 0});
    recorder.advance(1);
    recorder.execute(BuildTowerCommand{{2, 1}, 0});
    recorder.advance(1000);
    return recorder.replay();
}

/// The JSON of a small replay with one piece of its text replaced.
QByteArray editedReplay(const QByteArray &from, const QByteArray &to)
{
    const QByteArray json = R"json({
    "formatVersion": 1,
    "content": "scenario.json",
    "seed": "7",
    "contentHash": "00000000000000ff",
    "commands": [
        { "tick": 3, "type": "buildTower", "x": 5, "y": 1, "towerType": 0, "result": "accepted" },
        { "tick": 5, "type": "buildTower", "x": -1, "y": 0, "towerType": 0, "result": "outsideMap" }
    ],
    "checkpoints": [{ "tick": 0, "hash": "0000000000000001" }, { "tick": 30, "hash": "abcdef0123456789" }]
}
)json";
    return QByteArray(json).replace(from, to);
}

// The expected texts hold quotes on purpose, raw strings would only make the tables wider.
// NOLINTBEGIN(modernize-raw-string-literal)
void addBrokenRow(const char *name, const char *from, const char *to, const QString &error)
{
    QTest::newRow(name) << QByteArray(from) << QByteArray(to) << error;
}

void addBrokenHeaderRows()
{
    addBrokenRow("version",
                 "\"formatVersion\": 1",
                 "\"formatVersion\": 2",
                 u"formatVersion: must be an integer from 1 to 1"_s);
    addBrokenRow("no content", "\"content\": \"scenario.json\",", "", u"content: is missing"_s);
    addBrokenRow("letters in seed",
                 "\"seed\": \"7\"",
                 "\"seed\": \"7a\"",
                 u"seed: must be a text of decimal digits that fits 64 bits"_s);
    addBrokenRow("sign in seed",
                 "\"seed\": \"7\"",
                 "\"seed\": \"+7\"",
                 u"seed: must be a text of decimal digits that fits 64 bits"_s);
    addBrokenRow("huge seed",
                 "\"seed\": \"7\"",
                 "\"seed\": \"18446744073709551616\"",
                 u"seed: must be a text of decimal digits that fits 64 bits"_s);
    addBrokenRow("short hash",
                 "\"00000000000000ff\"",
                 "\"ff\"",
                 u"contentHash: must be 16 lowercase hex digits"_s);
}

void addBrokenListRows()
{
    addBrokenRow("command type",
                 "\"type\": \"buildTower\", \"x\": 5",
                 "\"type\": \"swapTower\", \"x\": 5",
                 u"commands[0].type: unknown command \"swapTower\""_s);
    addBrokenRow("target priority",
                 "\"type\": \"buildTower\", \"x\": 5, \"y\": 1, \"towerType\": 0",
                 "\"type\": \"setTargetPriority\", \"tower\": 1, \"priority\": \"weakest\"",
                 u"commands[0].priority: unknown target priority \"weakest\""_s);
    addBrokenRow("rotation",
                 "\"type\": \"buildTower\", \"x\": 5",
                 "\"type\": \"buildTower\", \"rotation\": 4, \"x\": 5",
                 u"commands[0].rotation: must be an integer from 0 to 3"_s);
    addBrokenRow("result",
                 "\"result\": \"accepted\"",
                 "\"result\": \"maybe\"",
                 u"commands[0].result: unknown result \"maybe\""_s);
    addBrokenRow(
        "no result", ", \"result\": \"accepted\"", "", u"commands[0].result: is missing"_s);
    addBrokenRow("negative tick",
                 "\"tick\": 3",
                 "\"tick\": -3",
                 u"commands[0].tick: must be an integer from 0 to 999999999999999999"_s);
    addBrokenRow("order of commands",
                 "\"tick\": 5",
                 "\"tick\": 2",
                 u"commands[1].tick: must not be below the tick of the command before it"_s);
    addBrokenRow("command after the end",
                 "\"tick\": 5",
                 "\"tick\": 31",
                 u"commands: must not go on after the last checkpoint"_s);
    addBrokenRow("order of checkpoints",
                 "\"tick\": 30",
                 "\"tick\": 0",
                 u"checkpoints[1].tick: must be above the tick of the checkpoint before it"_s);
    addBrokenRow("bad checkpoint hash",
                 "\"abcdef0123456789\"",
                 "\"ABCDEF0123456789\"",
                 u"checkpoints[1].hash: must be 16 lowercase hex digits"_s);
    addBrokenRow(
        "unknown key", "\"seed\": \"7\"", "\"seed\": \"7\", \"speed\": 2", u"speed: unknown key"_s);
    addBrokenRow("commands no array",
                 "\"commands\": [",
                 "\"commands\": 1, \"x\": [",
                 u"commands: must be an array"_s);
    addBrokenRow(
        "fraction",
        "\"tick\": 5",
        "\"tick\": 5.5",
        u"line 8: number 5.5 is no integer, write decimal values as strings like \"1.5\""_s);
}
// NOLINTEND(modernize-raw-string-literal)

} // namespace

class ReplayTest : public QObject
{
    Q_OBJECT

private slots:
    void hashTextHasSixteenLowercaseDigits()
    {
        QCOMPARE(hashToText(0xABCULL), u"0000000000000abc"_s);
        QCOMPARE(hashFromText(u"ffffffffffffffff"), 0xFFFFFFFFFFFFFFFFULL);
        QCOMPARE(hashFromText(u"0000000000000abc"), 0xABCULL);
        QVERIFY(!hashFromText(u"abc"));
        QVERIFY(!hashFromText(u"0000000000000ABC"));
        QVERIFY(!hashFromText(u"000000000000000g"));
    }

    void theRecorderWritesEveryCommandAtItsTick()
    {
        const Replay replay = recordedGame();
        QCOMPARE(replay.content, u"scenario.json"_s);
        QCOMPARE(replay.seed, 7ULL);
        QCOMPARE(replay.commands.size(), std::size_t(3));
        QCOMPARE(replay.commands[0].tick, 0);
        QVERIFY(replay.commands[0].result == CommandResult::Accepted);
        QCOMPARE(replay.commands[1].tick, 2);
        QVERIFY(replay.commands[1].result == CommandResult::CellOccupied);
        QCOMPARE(replay.commands[2].tick, 3);
    }

    void checkpointsComeOncePerSecondAndAtTheEnd()
    {
        ReplayRecorder recorder(slowScenario(), u"s"_s, 1);
        recorder.advance(65);
        const std::vector<Checkpoint> checkpoints = recorder.replay().checkpoints;
        QCOMPARE(checkpoints.size(), std::size_t(4));
        QCOMPARE(checkpoints[0].tick, 0);
        QCOMPARE(checkpoints[1].tick, 30);
        QCOMPARE(checkpoints[2].tick, 60);
        QCOMPARE(checkpoints[3].tick, recorder.simulation().state().tick);
        QCOMPARE(checkpoints[3].hash, recorder.simulation().hash());
    }

    void aCheckpointIncludesTheCommandsOfItsTick()
    {
        ReplayRecorder recorder(slowScenario(), u"s"_s, 1);
        recorder.advance(30);
        recorder.execute(BuildTowerCommand{{5, 1}, 0});
        const quint64 afterCommand = recorder.simulation().hash();
        recorder.advance(1);
        const Replay replay = recorder.replay();
        QCOMPARE(replay.checkpoints[1].tick, 30);
        QCOMPARE(replay.checkpoints[1].hash, afterCommand);
    }

    void askingForAReplayDoesNotChangeTheRecording()
    {
        ReplayRecorder recorder(scenario(), u"s"_s, 1);
        recorder.advance(5);
        QCOMPARE(recorder.replay().checkpoints.size(), std::size_t(2));
        recorder.advance(5);
        const std::vector<Checkpoint> checkpoints = recorder.replay().checkpoints;
        QCOMPARE(checkpoints.size(), std::size_t(2));
        QCOMPARE(checkpoints[1].tick, 10);
    }

    void aReplayMatchesItsOwnGame()
    {
        const Replay replay = recordedGame();
        ReplayPlayer player(scenario(), replay);
        player.step(1'000'000);
        QVERIFY(player.finished());
        QCOMPARE(player.error(), QString());
        QCOMPARE(player.simulation().hash(), replay.checkpoints.back().hash);
        QVERIFY(player.simulation().state().status == GameStatus::Won);
    }

    void commandsAfterTheEndAreRecordedAndRepeated()
    {
        ReplayRecorder recorder(scenario(), u"s"_s, 7);
        recorder.execute(BuildTowerCommand{{5, 1}, 0});
        recorder.advance(1000);
        QVERIFY(recorder.execute(BuildTowerCommand{{2, 1}, 0}) == CommandResult::GameOver);
        QCOMPARE(verifyReplay(scenario(), recorder.replay()), QString());
    }

    void aReplayOfAFreshGameHasOneCheckpoint()
    {
        const ReplayRecorder recorder(scenario(), u"s"_s, 1);
        const Replay replay = recorder.replay();
        QCOMPARE(replay.checkpoints.size(), std::size_t(1));
        const ReplayPlayer player(scenario(), replay);
        QVERIFY(player.finished());
        QCOMPARE(player.error(), QString());
    }

    void stepsOfZeroTicksDoNothing()
    {
        ReplayPlayer player(scenario(), recordedGame());
        player.step(0);
        QCOMPARE(player.simulation().state().tick, 0);
        QVERIFY(!player.finished());
    }

    void aWrongHashIsFoundAtItsTick()
    {
        Replay replay = recordedGame();
        replay.checkpoints[1].hash ^= 1U;
        const qint64 tick = replay.checkpoints[1].tick;
        ReplayPlayer player(scenario(), std::move(replay));
        player.step(1'000'000);
        QVERIFY(player.finished());
        QVERIFY(player.error().startsWith(u"tick %1: the state hash is "_s.arg(tick)));
        QCOMPARE(player.simulation().state().tick, tick);
    }

    void anotherSeedIsFound()
    {
        Replay replay = recordedGame();
        replay.seed = 8;
        const QString difference = verifyReplay(scenario(), std::move(replay));
        QVERIFY(difference.startsWith(u"tick 0: the state hash"_s));
    }

    void aWrongResultIsFound()
    {
        Replay replay = recordedGame();
        replay.commands[1].result = CommandResult::Accepted;
        QCOMPARE(verifyReplay(scenario(), std::move(replay)),
                 u"tick 2: a command gave cellOccupied, the replay expects accepted"_s);
    }

    void otherContentIsFoundBeforeTheGameStarts()
    {
        const Content other = contentOf(editedJson("\"startGold\": 100", "\"startGold\": 101"));
        const ReplayPlayer player(other, recordedGame());
        QVERIFY(player.finished());
        QVERIFY(player.error().startsWith(u"the content differs"_s));
        QCOMPARE(player.simulation().state().tick, 0);
    }

    void aReplayThatGoesOnAfterTheEndIsFound()
    {
        Replay replay = recordedGame();
        const qint64 end = replay.checkpoints.back().tick;
        replay.checkpoints.push_back(Checkpoint{end + 10, 0});
        QCOMPARE(verifyReplay(scenario(), std::move(replay)),
                 u"the game ended, the replay expects tick %1"_s.arg(end + 10));
    }

    void jsonKeepsAllOfAReplay()
    {
        Replay replay = recordedGame();
        replay.seed = 0xFFFFFFFFFFFFFFFFULL; // does not survive a detour through floating point
        const Replay read = required(parseReplay(toJson(replay)).replay);
        QCOMPARE(read.content, replay.content);
        QCOMPARE(read.seed, replay.seed);
        QCOMPARE(read.contentHash, replay.contentHash);
        QCOMPARE(read.commands.size(), replay.commands.size());
        QCOMPARE(read.commands[1].tick, 2);
        QVERIFY(read.commands[1].result == CommandResult::CellOccupied);
        QCOMPARE(std::get<BuildTowerCommand>(read.commands[2].command).cell.x, 2);
        QCOMPARE(read.checkpoints.size(), replay.checkpoints.size());
        QCOMPARE(read.checkpoints.back().hash, replay.checkpoints.back().hash);
        QCOMPARE(toJson(read), toJson(replay));
    }

    void theFileFormatIsFixed()
    {
        Replay replay;
        replay.content = u"map.json"_s;
        replay.seed = 7;
        replay.contentHash = 255;
        replay.commands.push_back({3,
                                   BuildTowerCommand{{2, 1}, 0, qtd::core::Rotation::Half},
                                   CommandResult::NotEnoughGold});
        replay.checkpoints.push_back({3, 16});
        const QByteArray compact = QByteArray(toJson(replay)).replace(' ', "").replace('\n', "");
        QCOMPARE(
            compact,
            QByteArray(R"({"checkpoints":[{"hash":"0000000000000010","tick":3}],)"
                       R"("commands":[{"result":"notEnoughGold","rotation":2,"tick":3,)"
                       R"("towerType":0,"type":"buildTower","x":2,"y":1}],"content":"map.json",)"
                       R"("contentHash":"00000000000000ff","formatVersion":1,"seed":"7"})"));
    }

    void aRotationComesBackFromAFile()
    {
        Replay replay;
        replay.content = u"map.json"_s;
        replay.seed = 7;
        replay.contentHash = 255;
        replay.commands.push_back({3,
                                   BuildTowerCommand{{2, 1}, 0, qtd::core::Rotation::ThreeQuarters},
                                   CommandResult::Accepted});
        replay.checkpoints.push_back({3, 16});
        const ReplayLoadResult result = parseReplay(toJson(replay));
        QCOMPARE(result.error, QString());
        const Replay read = required(result.replay);
        const auto &build = std::get<BuildTowerCommand>(read.commands.front().command);
        QVERIFY(build.rotation == qtd::core::Rotation::ThreeQuarters);
    }

    void aTargetPriorityComesBackFromAFile()
    {
        Replay replay;
        replay.content = u"map.json"_s;
        replay.seed = 7;
        replay.contentHash = 255;
        replay.commands.push_back(
            {3,
             qtd::core::SetTargetPriorityCommand{9, qtd::core::TargetPriority::Strongest},
             CommandResult::UnknownTower});
        replay.checkpoints.push_back({3, 16});
        const ReplayLoadResult result = parseReplay(toJson(replay));
        QCOMPARE(result.error, QString());
        const Replay read = required(result.replay);
        const auto &command =
            std::get<qtd::core::SetTargetPriorityCommand>(read.commands.front().command);
        QCOMPARE(command.tower, 9);
        QVERIFY(command.priority == qtd::core::TargetPriority::Strongest);
        QVERIFY(read.commands.front().result == CommandResult::UnknownTower);
    }

    void commandsOnTowersSurviveARoundTrip()
    {
        Replay replay;
        replay.content = u"map.json"_s;
        replay.seed = 7;
        replay.contentHash = 255;
        replay.commands.push_back(
            {3, qtd::core::UpgradeTowerCommand{9}, CommandResult::HighestLevel});
        replay.commands.push_back({4, qtd::core::SellTowerCommand{8}, CommandResult::Accepted});
        replay.checkpoints.push_back({4, 16});
        const ReplayLoadResult result = parseReplay(toJson(replay));
        QCOMPARE(result.error, QString());
        const Replay read = required(result.replay);
        QCOMPARE(std::get<qtd::core::UpgradeTowerCommand>(read.commands.at(0).command).tower, 9);
        QVERIFY(read.commands.at(0).result == CommandResult::HighestLevel);
        QCOMPARE(std::get<qtd::core::SellTowerCommand>(read.commands.at(1).command).tower, 8);
    }

    void aSpellSurvivesARoundTrip()
    {
        Replay replay;
        replay.content = u"map.json"_s;
        replay.seed = 7;
        replay.contentHash = 255;
        replay.commands.push_back(
            {3, qtd::core::CastSpellCommand{1, {4, 5}}, CommandResult::SpellCooling});
        replay.commands.push_back(
            {3, qtd::core::CastSpellCommand{9, {4, 5}}, CommandResult::UnknownSpell});
        replay.checkpoints.push_back({3, 16});
        const ReplayLoadResult result = parseReplay(toJson(replay));
        QCOMPARE(result.error, QString());
        const Replay read = required(result.replay);
        const auto &command = std::get<qtd::core::CastSpellCommand>(read.commands.at(0).command);
        QCOMPARE(command.spell, 1);
        QCOMPARE(command.cell.x, 4);
        QCOMPARE(command.cell.y, 5);
        QVERIFY(read.commands.at(0).result == CommandResult::SpellCooling);
        QVERIFY(read.commands.at(1).result == CommandResult::UnknownSpell);
    }

    void aCommandWithoutRotationStandsUnturned()
    {
        const ReplayLoadResult result = parseReplay(editedReplay("", ""));
        QCOMPARE(result.error, QString());
        const Replay read = required(result.replay);
        const auto &build = std::get<BuildTowerCommand>(read.commands.front().command);
        QVERIFY(build.rotation == qtd::core::Rotation::None);
    }

    void aValidFileIsRead()
    {
        const ReplayLoadResult result = parseReplay(editedReplay("", ""));
        QCOMPARE(result.error, QString());
        const Replay replay = required(result.replay);
        QVERIFY(replay.commands[1].result == CommandResult::OutsideMap);
        QCOMPARE(std::get<BuildTowerCommand>(replay.commands[1].command).cell.x, -1);
        QCOMPARE(replay.checkpoints[1].hash, 0xABCDEF0123456789ULL);
    }

    void brokenFilesAreRejected_data()
    {
        QTest::addColumn<QByteArray>("from");
        QTest::addColumn<QByteArray>("to");
        QTest::addColumn<QString>("error");
        addBrokenHeaderRows();
        addBrokenListRows();
    }

    void brokenFilesAreRejected()
    {
        QFETCH(const QByteArray, from);
        QFETCH(const QByteArray, to);
        QFETCH(const QString, error);
        QVERIFY(editedReplay("", "").contains(from));
        const ReplayLoadResult result = parseReplay(editedReplay(from, to));
        QVERIFY(!result.replay);
        QCOMPARE(result.error, error);
    }

    void noCheckpointsAreRejected()
    {
        const ReplayLoadResult result =
            parseReplay(R"({"formatVersion": 1, "content": "c", "seed": "1",
            "contentHash": "0000000000000000", "commands": [], "checkpoints": []})");
        QCOMPARE(result.error, u"checkpoints: must be an array that is not empty"_s);
    }

    void aScriptNeedsCommandsOnly()
    {
        const ReplayLoadResult result =
            parseScript(R"({"formatVersion": 1, "content": "c", "seed": "9",
            "commands": [{"tick": 4, "type": "buildTower", "x": 10, "y": 10, "towerType": 0}]})");
        QCOMPARE(result.error, QString());
        const Replay script = required(result.replay);
        QCOMPARE(script.seed, 9ULL);
        QCOMPARE(script.commands[0].tick, 4);
        QVERIFY(script.checkpoints.empty());
    }

    void aReplayIsAScript()
    {
        const ReplayLoadResult result = parseScript(editedReplay("", ""));
        QCOMPARE(result.error, QString());
        QCOMPARE(required(result.replay).commands.size(), std::size_t(2));
    }

    void aScriptIsNoReplay()
    {
        const ReplayLoadResult result =
            parseReplay(R"({"formatVersion": 1, "content": "c", "seed": "9",
            "commands": []})");
        QCOMPARE(result.error, u"contentHash: is missing"_s);
    }

    void filesAreLoaded()
    {
        const QTemporaryDir directory;
        const QString name = directory.filePath(u"game.replay.json"_s);
        QFile file(name);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(toJson(recordedGame()));
        file.close();
        QCOMPARE(verifyReplay(scenario(), required(loadReplayFile(name).replay)), QString());
        QCOMPARE(required(loadScriptFile(name).replay).commands.size(), std::size_t(3));
    }

    void problemsWithFilesNameTheFile()
    {
        const QTemporaryDir directory;
        const QString missing = directory.filePath(u"missing.json"_s);
        QVERIFY(loadReplayFile(missing).error.startsWith(missing + u": "_s));
        const QString broken = directory.filePath(u"broken.json"_s);
        QFile file(broken);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("[]");
        file.close();
        QCOMPARE(loadReplayFile(broken).error, broken + u": the top level must be an object"_s);
    }
};

QTEST_APPLESS_MAIN(ReplayTest)
#include "tst_replay.moc"
