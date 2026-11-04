#include "automation/headless_runner.h"

#include "core/content_loader.h"
#include "core/replay.h"
#include "core/simulation.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLatin1StringView>
#include <QSaveFile>

#include <algorithm>
#include <limits>
#include <type_traits>
#include <utility>
#include <vector>

using namespace Qt::StringLiterals;

namespace qtd::automation {

namespace {

constexpr int kExitOk = 0;
constexpr int kExitFailed = 1;
constexpr qint64 kNanosecondsPerSecond = 1'000'000'000;

QByteArray compact(const QJsonObject &object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

HeadlessOutcome failure(const QString &message)
{
    return {kExitFailed, compact({{"error"_L1, message}})};
}

QString statusName(core::GameStatus status)
{
    switch (status) {
    case core::GameStatus::Won:
        return u"won"_s;
    case core::GameStatus::Lost:
        return u"lost"_s;
    case core::GameStatus::Running:
        break;
    }
    return u"running"_s;
}

QJsonObject stateReport(const core::Simulation &game)
{
    const core::SimulationState &state = game.state();
    return {{"status"_L1, statusName(state.status)},
            {"tick"_L1, state.tick},
            {"gold"_L1, state.gold},
            {"lives"_L1, state.lives},
            {"hash"_L1, core::hashToText(game.hash())}};
}

/// Plays the commands of the script at their ticks, then goes on until the game ends. Commands for
/// ticks behind the end of the game run at its last tick and get GameOver. Commands behind maxTicks
/// are left out: the game stops before their time comes.
void playGame(core::ReplayRecorder *recorder,
              const std::vector<core::RecordedCommand> &commands,
              qint64 maxTicks)
{
    for (const core::RecordedCommand &recorded : commands) {
        if (recorded.tick > maxTicks) {
            break;
        }
        recorder->advance(recorded.tick - recorder->simulation().state().tick);
        recorder->execute(recorded.command);
    }
    recorder->advance(maxTicks - recorder->simulation().state().tick);
}

bool writeFile(const QString &fileName, const QByteArray &data, QString *error)
{
    QSaveFile file(fileName);
    if (file.open(QIODevice::WriteOnly) && file.write(data) == data.size() && file.commit()) {
        return true;
    }
    *error = u"%1: %2"_s.arg(fileName, file.errorString());
    return false;
}

QJsonObject benchmarkReport(qint64 ticks, qint64 nanoseconds)
{
    const qint64 time = std::max<qint64>(nanoseconds, 1);
    const qint64 ticksPerSecond = ticks * kNanosecondsPerSecond / time;
    return {{"ticks"_L1, ticks},
            {"nanoseconds"_L1, nanoseconds},
            {"nanosecondsPerTick"_L1, ticks > 0 ? nanoseconds / ticks : 0},
            {"ticksPerSecond"_L1, ticksPerSecond},
            {"realtimeFactor"_L1, ticksPerSecond / core::kTicksPerSecond}};
}

HeadlessOutcome verify(const HeadlessOptions &options, core::Content content)
{
    core::ReplayLoadResult loaded = core::loadReplayFile(options.replay);
    if (!loaded.replay) {
        return failure(loaded.error);
    }
    core::ReplayPlayer player(std::move(content), *std::move(loaded.replay));
    player.step(std::numeric_limits<qint64>::max());
    QJsonObject report = stateReport(player.simulation());
    report.insert("matches"_L1, player.error().isEmpty());
    if (!player.error().isEmpty()) {
        report.insert("difference"_L1, player.error());
    }
    return {player.error().isEmpty() ? kExitOk : kExitFailed, compact(report)};
}

HeadlessOutcome playGames(const HeadlessOptions &options, const core::Content &content)
{
    std::vector<core::RecordedCommand> commands;
    quint64 firstSeed = options.seed.value_or(kDefaultSeed);
    if (!options.script.isEmpty()) {
        core::ReplayLoadResult script = core::loadScriptFile(options.script);
        if (!script.replay) {
            return failure(script.error);
        }
        commands = std::move(script.replay->commands);
        firstSeed = options.seed.value_or(script.replay->seed);
    }
    const QString contentName = QFileInfo(options.map).fileName();
    QJsonArray games;
    qint64 ticks = 0;
    qint64 nanoseconds = 0;
    for (qint64 run = 0; run < options.runs; ++run) {
        const quint64 seed = firstSeed + static_cast<quint64>(run);
        core::ReplayRecorder recorder(content, contentName, seed);
        QElapsedTimer timer;
        timer.start();
        playGame(&recorder, commands, options.maxTicks);
        nanoseconds += timer.nsecsElapsed();
        ticks += recorder.simulation().state().tick;

        const core::Replay replay = recorder.replay();
        QJsonObject report = stateReport(recorder.simulation());
        report.insert("seed"_L1, QString::number(seed));
        report.insert("commands"_L1, static_cast<qint64>(replay.commands.size()));
        report.insert(
            "rejected"_L1,
            static_cast<qint64>(std::ranges::count_if(replay.commands, [](const auto &recorded) {
                return recorded.result != core::CommandResult::Accepted;
            })));
        games.append(report);

        QString error;
        if (!options.record.isEmpty() && !writeFile(options.record, core::toJson(replay), &error)) {
            return failure(error);
        }
    }
    QJsonObject root{{"map"_L1, contentName}, {"games"_L1, games}};
    if (options.benchmark) {
        root.insert("benchmark"_L1, benchmarkReport(ticks, nanoseconds));
    }
    return {kExitOk, compact(root)};
}

HeadlessOutcome run(const HeadlessOptions &options)
{
    core::ContentLoadResult loaded = core::loadContentFile(options.map);
    if (!loaded.content) {
        return failure(loaded.error);
    }
    return options.replay.isEmpty() ? playGames(options, *loaded.content)
                                    : verify(options, *std::move(loaded.content));
}

/// The first rule that the given options break, or nothing.
QString conflict(const HeadlessOptions &options, bool maxTicksGiven)
{
    if (options.map.isEmpty()) {
        return u"--map is required"_s;
    }
    if (options.runs < 1) {
        return u"--runs must be at least 1"_s;
    }
    if (options.maxTicks < 0) {
        return u"--max-ticks must not be negative"_s;
    }
    if (options.runs > 1 && !options.record.isEmpty()) {
        return u"--record takes a single run"_s;
    }
    if (!options.replay.isEmpty() && (options.seed || options.runs > 1 ||
                                      !options.script.isEmpty() || !options.record.isEmpty())) {
        return u"--replay brings its own seed and commands"_s;
    }
    if (!options.replay.isEmpty() && (maxTicksGiven || options.benchmark)) {
        return u"--replay runs to its end and measures nothing"_s;
    }
    return {};
}

template <typename Number>
bool readNumber(const QCommandLineParser &parser,
                const QCommandLineOption &option,
                Number *number,
                QString *error)
{
    if (!parser.isSet(option)) {
        return true;
    }
    bool ok = false;
    const QString text = parser.value(option);
    if constexpr (std::is_same_v<Number, quint64>) {
        *number = text.toULongLong(&ok);
    } else {
        *number = text.toLongLong(&ok);
    }
    if (!ok) {
        *error = u"--%1 needs a number, not \"%2\""_s.arg(option.names().constFirst(), text);
    }
    return ok;
}

std::optional<HeadlessBot> botFromName(const QString &name)
{
    if (name == "idle"_L1) {
        return HeadlessBot::Idle;
    }
    return std::nullopt;
}

/// The options of the command line.
struct CommandLine
{
    QCommandLineOption help{u"help"_s, u"Show this text."_s};
    QCommandLineOption buildInfo{u"build-info"_s, u"Print program and version as JSON."_s};
    QCommandLineOption map{u"map"_s, u"The content file to play."_s, u"file"_s};
    QCommandLineOption seed{
        u"seed"_s, u"Seed of the first game, default: of the script, or 1."_s, u"number"_s};
    QCommandLineOption runs{
        u"runs"_s, u"Number of games, seeds count up, default 1."_s, u"number"_s};
    QCommandLineOption bot{u"bot"_s, u"Who plays: idle, the default."_s, u"name"_s};
    QCommandLineOption script{u"script"_s, u"Play the commands of a file."_s, u"file"_s};
    QCommandLineOption replay{u"replay"_s, u"Check a replay against --map."_s, u"file"_s};
    QCommandLineOption record{u"record"_s, u"Write the game as a replay."_s, u"file"_s};
    QCommandLineOption output{u"output"_s, u"Write the report to a file."_s, u"file"_s};
    QCommandLineOption maxTicks{
        u"max-ticks"_s, u"Stop a game at this tick, default 1000000."_s, u"number"_s};
    QCommandLineOption benchmark{u"benchmark"_s, u"Add measured tick durations."_s};
};

void readOptions(const QCommandLineParser &parser,
                 const CommandLine &line,
                 HeadlessArguments *result)
{
    HeadlessOptions options;
    options.map = parser.value(line.map);
    options.script = parser.value(line.script);
    options.replay = parser.value(line.replay);
    options.record = parser.value(line.record);
    options.output = parser.value(line.output);
    options.benchmark = parser.isSet(line.benchmark);
    if (parser.isSet(line.bot)) {
        const QString name = parser.value(line.bot);
        const std::optional<HeadlessBot> bot = botFromName(name);
        if (!bot) {
            result->error = u"unknown bot \"%1\", available: idle"_s.arg(name);
            return;
        }
        options.bot = *bot;
    }
    quint64 seed = kDefaultSeed;
    if (readNumber(parser, line.seed, &seed, &result->error) &&
        readNumber(parser, line.runs, &options.runs, &result->error) &&
        readNumber(parser, line.maxTicks, &options.maxTicks, &result->error)) {
        options.seed = parser.isSet(line.seed) ? std::optional(seed) : std::nullopt;
        result->error = conflict(options, parser.isSet(line.maxTicks));
    }
    if (result->error.isEmpty()) {
        result->options = std::move(options);
    }
}

} // namespace

HeadlessArguments parseHeadlessArguments(const QStringList &arguments)
{
    const CommandLine line;
    QCommandLineParser parser;
    parser.addOptions({line.help,
                       line.buildInfo,
                       line.map,
                       line.seed,
                       line.runs,
                       line.bot,
                       line.script,
                       line.replay,
                       line.record,
                       line.output,
                       line.maxTicks,
                       line.benchmark});

    HeadlessArguments result;
    result.helpText = parser.helpText();
    if (!parser.parse(arguments)) {
        result.error = parser.errorText();
        return result;
    }
    if (!parser.positionalArguments().isEmpty()) {
        result.error = u"unexpected argument \"%1\""_s.arg(parser.positionalArguments().first());
        return result;
    }
    result.help = parser.isSet(line.help);
    result.buildInfo = parser.isSet(line.buildInfo);
    if (!result.help && !result.buildInfo) {
        readOptions(parser, line, &result);
    }
    return result;
}

HeadlessOutcome runHeadless(const HeadlessOptions &options)
{
    HeadlessOutcome outcome = run(options);
    QString error;
    if (!options.output.isEmpty() && !writeFile(options.output, outcome.json + '\n', &error)) {
        return failure(error);
    }
    return outcome;
}

} // namespace qtd::automation
