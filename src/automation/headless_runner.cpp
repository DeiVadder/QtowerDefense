#include "automation/headless_runner.h"

#include "automation/report.h"
#include "core/content_loader.h"
#include "core/replay.h"
#include "core/simulation.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QLatin1StringView>
#include <QTextStream>

#include <algorithm>
#include <limits>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

using namespace Qt::StringLiterals;

namespace qtd::automation {

namespace {

constexpr int kExitOk = 0;
constexpr int kExitFailed = 1;
constexpr qint64 kNanosecondsPerSecond = 1'000'000'000;

HeadlessOutcome failure(const QString &message)
{
    return {kExitFailed, compact({{"error"_L1, message}})};
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

/// The state check of a game as a text with its tick, or nothing.
QString problemOf(const core::Simulation &game, const StateCheck &check)
{
    const QString problem = check(game.content(), game.state());
    return problem.isEmpty() ? problem : u"tick %1: %2"_s.arg(game.state().tick).arg(problem);
}

/// All commands of a bot for the current tick. Rejected commands must not change the game.
QString playCommands(core::ReplayRecorder *recorder, Bot *bot, const StateCheck &check)
{
    const core::Simulation &game = recorder->simulation();
    while (const std::optional<core::Command> command = bot->nextCommand(game)) {
        const quint64 before = game.hash();
        const bool over = game.state().status != core::GameStatus::Running;
        const core::CommandResult result = recorder->execute(*command);
        if (over && result != core::CommandResult::GameOver) {
            return u"tick %1: a command ran after the end"_s.arg(game.state().tick);
        }
        if (result != core::CommandResult::Accepted && recorder->simulation().hash() != before) {
            return u"tick %1: a rejected command changed the state"_s.arg(game.state().tick);
        }
        const QString problem = problemOf(game, check);
        if (!problem.isEmpty()) {
            return problem;
        }
    }
    return {};
}

/// Lets a bot play, also after the end of the game, where commands must be rejected. Checks the
/// state from the start on. Gives a text for the first broken rule, or nothing.
QString
playWithBot(core::ReplayRecorder *recorder, Bot *bot, const StateCheck &check, qint64 maxTicks)
{
    const core::Simulation &game = recorder->simulation();
    QString problem = problemOf(game, check);
    while (problem.isEmpty()) {
        problem = playCommands(recorder, bot, check);
        if (game.state().status != core::GameStatus::Running || game.state().tick >= maxTicks) {
            break;
        }
        if (problem.isEmpty()) {
            recorder->advance(1);
            problem = problemOf(game, check);
        }
    }
    return problem;
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
    QString problem = problemOf(player.simulation(), options.check);
    while (problem.isEmpty() && !player.finished()) {
        player.step(1);
        problem = problemOf(player.simulation(), options.check);
    }
    QJsonObject report = stateReport(player.simulation());
    report.insert("matches"_L1, player.error().isEmpty());
    if (!player.error().isEmpty()) {
        report.insert("difference"_L1, player.error());
    }
    if (!problem.isEmpty()) {
        report.insert("invariant"_L1, problem);
    }
    const bool good = player.error().isEmpty() && problem.isEmpty();
    return {good ? kExitOk : kExitFailed, compact(report)};
}

QJsonObject gameReport(const core::Simulation &game, const core::Replay &replay, quint64 seed)
{
    QJsonObject report = stateReport(game);
    report.insert("seed"_L1, QString::number(seed));
    report.insert("commands"_L1, static_cast<qint64>(replay.commands.size()));
    report.insert(
        "rejected"_L1,
        static_cast<qint64>(std::ranges::count_if(replay.commands, [](const auto &recorded) {
            return recorded.result != core::CommandResult::Accepted;
        })));
    return report;
}

/// Keeps a broken game as a replay, so that the problem can be repeated.
HeadlessOutcome keepBrokenGame(const QString &problem, const core::Replay &replay, QString fileName)
{
    if (fileName.isEmpty()) {
        fileName = u"failure-%1.replay.json"_s.arg(replay.seed);
    }
    QString error;
    if (!writeFile(fileName, core::toJson(replay), &error)) {
        return failure(u"%1, and no replay: %2"_s.arg(problem, error));
    }
    return {kExitFailed,
            compact({{"error"_L1, problem},
                     {"seed"_L1, QString::number(replay.seed)},
                     {"replay"_L1, fileName}})};
}

/// Plays one game. A crash leaves no replay, so a fuzz run names each seed before it starts.
QString playOneGame(core::ReplayRecorder *recorder,
                    const HeadlessOptions &options,
                    const std::vector<core::RecordedCommand> &commands,
                    quint64 seed)
{
    if (options.bot == BotKind::Idle) {
        playGame(recorder, commands, options.maxTicks);
        return {};
    }
    if (options.bot == BotKind::Fuzz) {
        QTextStream(stderr) << "fuzz game with seed " << seed << '\n';
    }
    const std::unique_ptr<Bot> bot = makeBot(options.bot, seed);
    return playWithBot(recorder, bot.get(), options.check, options.maxTicks);
}

HeadlessOutcome playGames(const HeadlessOptions &options, const core::Content &content)
{
    std::vector<core::RecordedCommand> commands;
    quint64 firstSeed = options.seed.value_or(kDefaultSeed);
    core::GameMode mode = options.mode.value_or(core::GameMode::Campaign);
    if (!options.script.isEmpty()) {
        core::ReplayLoadResult script = core::loadScriptFile(options.script);
        if (!script.replay) {
            return failure(script.error);
        }
        commands = std::move(script.replay->commands);
        firstSeed = options.seed.value_or(script.replay->seed);
        mode = options.mode.value_or(script.replay->mode);
    }
    const QString contentName = QFileInfo(options.map).fileName();
    QJsonArray games;
    qint64 ticks = 0;
    qint64 nanoseconds = 0;
    for (qint64 run = 0; run < options.runs; ++run) {
        const quint64 seed = firstSeed + static_cast<quint64>(run);
        core::ReplayRecorder recorder(content, contentName, seed, mode);
        QElapsedTimer timer;
        timer.start();
        const QString problem = playOneGame(&recorder, options, commands, seed);
        nanoseconds += timer.nsecsElapsed();
        ticks += recorder.simulation().state().tick;

        const core::Replay replay = recorder.replay();
        if (!problem.isEmpty()) {
            return keepBrokenGame(problem, replay, options.record);
        }
        games.append(gameReport(recorder.simulation(), replay, seed));

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
    if (options.control) {
        const bool alone = options.map.isEmpty() && options.replay.isEmpty() &&
                           options.script.isEmpty() && options.record.isEmpty() &&
                           options.output.isEmpty() && !options.seed && !options.mode &&
                           options.runs == 1 && options.bot == BotKind::Idle && !maxTicksGiven &&
                           !options.benchmark;
        return alone ? QString() : u"--control takes its games from the protocol"_s;
    }
    if (options.map.isEmpty()) {
        return u"--map is required"_s;
    }
    if (options.bot != BotKind::Idle && !options.script.isEmpty()) {
        return u"--bot and --script both want to play"_s;
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
    if (!options.replay.isEmpty() &&
        (options.seed || options.mode || options.runs > 1 || options.bot != BotKind::Idle ||
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

std::optional<BotKind> botFromName(const QString &name)
{
    if (name == "idle"_L1) {
        return BotKind::Idle;
    }
    if (name == "naive"_L1) {
        return BotKind::Naive;
    }
    if (name == "fuzz"_L1) {
        return BotKind::Fuzz;
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
    QCommandLineOption bot{u"bot"_s, u"Who plays: idle, the default, naive or fuzz."_s, u"name"_s};
    QCommandLineOption mode{u"mode"_s,
                            u"campaign, the default, or endless: the wave comes again, stronger."_s,
                            u"name"_s};
    QCommandLineOption script{u"script"_s, u"Play the commands of a file."_s, u"file"_s};
    QCommandLineOption replay{u"replay"_s, u"Check a replay against --map."_s, u"file"_s};
    QCommandLineOption record{u"record"_s, u"Write the game as a replay."_s, u"file"_s};
    QCommandLineOption output{u"output"_s, u"Write the report to a file."_s, u"file"_s};
    QCommandLineOption maxTicks{
        u"max-ticks"_s, u"Stop a game at this tick, default 1000000."_s, u"number"_s};
    QCommandLineOption benchmark{u"benchmark"_s, u"Add measured tick durations."_s};
    QCommandLineOption control{u"control"_s, u"Speak the JSON protocol on stdin and stdout."_s};
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
    options.control = parser.isSet(line.control);
    if (parser.isSet(line.bot)) {
        const QString name = parser.value(line.bot);
        const std::optional<BotKind> bot = botFromName(name);
        if (!bot) {
            result->error = u"unknown bot \"%1\", available: idle, naive, fuzz"_s.arg(name);
            return;
        }
        options.bot = *bot;
    }
    if (parser.isSet(line.mode)) {
        const QString name = parser.value(line.mode);
        options.mode = core::gameModeFromText(name);
        if (!options.mode) {
            result->error = u"unknown mode \"%1\", available: campaign, endless"_s.arg(name);
            return;
        }
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
                       line.mode,
                       line.script,
                       line.replay,
                       line.record,
                       line.output,
                       line.maxTicks,
                       line.benchmark,
                       line.control});

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
