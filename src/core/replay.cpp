#include "core/replay.h"

#include "core/json_reader.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLatin1StringView>

#include <algorithm>
#include <array>
#include <limits>
#include <utility>
#include <variant>

using namespace Qt::StringLiterals;

namespace qtd::core {

namespace {

constexpr qint64 kFormatVersion = 1;
constexpr qsizetype kHashDigits = 16;

using ResultNames = std::array<QLatin1StringView, 7>;
constexpr ResultNames kResultNames{"accepted"_L1,
                                   "gameOver"_L1,
                                   "unknownTowerType"_L1,
                                   "outsideMap"_L1,
                                   "cellBlocked"_L1,
                                   "cellOccupied"_L1,
                                   "notEnoughGold"_L1};
static_assert(kResultNames.size() == static_cast<std::size_t>(CommandResult::NotEnoughGold) + 1);

enum class Kind : quint8 {
    Replay,
    Script,
};

bool isHashDigit(QChar c)
{
    return (c >= u'0' && c <= u'9') || (c >= u'a' && c <= u'f');
}

bool isDecimalDigit(QChar c)
{
    return c >= u'0' && c <= u'9';
}

quint64 readSeed(ObjectReader &root)
{
    const std::optional<quint64> seed = seedFromText(root.text("seed"_L1));
    if (!seed) {
        root.fail(u"seed"_s, u"must be a text of decimal digits that fits 64 bits"_s);
    }
    return seed.value_or(0);
}

quint64 readHash(ObjectReader &reader, QLatin1StringView key)
{
    const std::optional<quint64> hash = hashFromText(reader.text(key));
    if (!hash) {
        reader.fail(key, u"must be %1 lowercase hex digits"_s.arg(kHashDigits));
    }
    return hash.value_or(0);
}

CommandResult readResult(ObjectReader &item)
{
    const QString name = item.text("result"_L1);
    const ResultNames::const_iterator found = std::ranges::find(kResultNames, name);
    if (found == kResultNames.end()) {
        item.fail(u"result"_s, u"unknown result \"%1\""_s.arg(name));
        return CommandResult::Accepted;
    }
    return static_cast<CommandResult>(found - kResultNames.begin());
}

Command readCommand(ObjectReader &item)
{
    const QString type = item.text("type"_L1);
    if (type != "buildTower"_L1) {
        item.fail(u"type"_s, u"unknown command \"%1\""_s.arg(type));
        return BuildTowerCommand{};
    }
    // Any number is allowed, a rejected command is part of a recording too.
    const qint64 x = item.integer("x"_L1, -kMaxExactInteger, kMaxExactInteger);
    const qint64 y = item.integer("y"_L1, -kMaxExactInteger, kMaxExactInteger);
    return BuildTowerCommand{{x, y},
                             item.integer("towerType"_L1, -kMaxExactInteger, kMaxExactInteger)};
}

void readCommands(ObjectReader &root, Kind kind, Replay *replay)
{
    const auto read = [kind, replay](ObjectReader &item) {
        RecordedCommand recorded;
        recorded.tick = item.integer("tick"_L1, 0, kMaxExactInteger);
        recorded.command = readCommand(item);
        if (kind == Kind::Replay || item.has("result"_L1)) {
            recorded.result = readResult(item);
        }
        if (!replay->commands.empty() && recorded.tick < replay->commands.back().tick) {
            item.fail(u"tick"_s, u"must not be below the tick of the command before it"_s);
        }
        replay->commands.push_back(recorded);
    };
    root.forEach("commands"_L1, read, ObjectReader::Items::MayBeEmpty);
}

void readCheckpoints(ObjectReader &root, Replay *replay)
{
    root.forEach("checkpoints"_L1, [replay](ObjectReader &item) {
        Checkpoint checkpoint;
        checkpoint.tick = item.integer("tick"_L1, 0, kMaxExactInteger);
        checkpoint.hash = readHash(item, "hash"_L1);
        if (!replay->checkpoints.empty() && checkpoint.tick <= replay->checkpoints.back().tick) {
            item.fail(u"tick"_s, u"must be above the tick of the checkpoint before it"_s);
        }
        replay->checkpoints.push_back(checkpoint);
    });
    if (!replay->commands.empty() && !replay->checkpoints.empty() &&
        replay->commands.back().tick > replay->checkpoints.back().tick) {
        root.fail(u"commands"_s, u"must not go on after the last checkpoint"_s);
    }
}

ReplayLoadResult failure(QString error)
{
    return {std::nullopt, std::move(error)};
}

ReplayLoadResult parse(QByteArrayView json, Kind kind)
{
    QString error;
    const std::optional<QJsonObject> object = parseExactJsonObject(json, &error);
    if (!object) {
        return failure(std::move(error));
    }
    ObjectReader root(*object, QString(), &error);
    root.integer("formatVersion"_L1, kFormatVersion, kFormatVersion);
    Replay replay;
    replay.content = root.text("content"_L1);
    replay.seed = readSeed(root);
    if (kind == Kind::Replay || root.has("contentHash"_L1)) {
        replay.contentHash = readHash(root, "contentHash"_L1);
    }
    readCommands(root, kind, &replay);
    if (kind == Kind::Replay || root.has("checkpoints"_L1)) {
        readCheckpoints(root, &replay);
    }
    root.rejectUnknownKeys();
    if (!error.isEmpty()) {
        return failure(std::move(error));
    }
    return {std::move(replay), QString()};
}

ReplayLoadResult loadFile(const QString &fileName, Kind kind)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        return failure(u"%1: %2"_s.arg(fileName, file.errorString()));
    }
    ReplayLoadResult result = parse(file.readAll(), kind);
    if (!result.replay) {
        result.error.prepend(fileName + u": "_s);
    }
    return result;
}

QJsonObject commandToJson(const RecordedCommand &recorded)
{
    const auto &build = std::get<BuildTowerCommand>(recorded.command);
    return {{"tick"_L1, recorded.tick},
            {"type"_L1, "buildTower"_L1},
            {"x"_L1, build.cell.x},
            {"y"_L1, build.cell.y},
            {"towerType"_L1, build.towerType},
            {"result"_L1, toString(recorded.result)}};
}

} // namespace

QString hashToText(quint64 hash)
{
    return u"%1"_s.arg(hash, kHashDigits, 16, QChar(u'0'));
}

std::optional<quint64> hashFromText(QStringView text)
{
    if (text.size() != kHashDigits || !std::ranges::all_of(text, isHashDigit)) {
        return std::nullopt;
    }
    return text.toULongLong(nullptr, 16);
}

std::optional<quint64> seedFromText(const QString &text)
{
    bool ok = !text.isEmpty() && std::ranges::all_of(text, isDecimalDigit);
    const quint64 seed = ok ? text.toULongLong(&ok) : 0;
    return ok ? std::optional<quint64>(seed) : std::nullopt;
}

QString toString(GameStatus status)
{
    switch (status) {
    case GameStatus::Won:
        return u"won"_s;
    case GameStatus::Lost:
        return u"lost"_s;
    case GameStatus::Running:
        break;
    }
    return u"running"_s;
}

QString toString(CommandResult result)
{
    return kResultNames[static_cast<std::size_t>(result)];
}

QByteArray toJson(const Replay &replay)
{
    QJsonArray commands;
    for (const RecordedCommand &recorded : replay.commands) {
        commands.append(commandToJson(recorded));
    }
    QJsonArray checkpoints;
    for (const Checkpoint &checkpoint : replay.checkpoints) {
        checkpoints.append(
            QJsonObject{{"tick"_L1, checkpoint.tick}, {"hash"_L1, hashToText(checkpoint.hash)}});
    }
    const QJsonObject root{{"formatVersion"_L1, kFormatVersion},
                           {"content"_L1, replay.content},
                           {"seed"_L1, QString::number(replay.seed)},
                           {"contentHash"_L1, hashToText(replay.contentHash)},
                           {"commands"_L1, commands},
                           {"checkpoints"_L1, checkpoints}};
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

ReplayLoadResult parseReplay(QByteArrayView json)
{
    return parse(json, Kind::Replay);
}

ReplayLoadResult parseScript(QByteArrayView json)
{
    return parse(json, Kind::Script);
}

ReplayLoadResult loadReplayFile(const QString &fileName)
{
    return loadFile(fileName, Kind::Replay);
}

ReplayLoadResult loadScriptFile(const QString &fileName)
{
    return loadFile(fileName, Kind::Script);
}

ReplayRecorder::ReplayRecorder(Content content, QString contentName, quint64 seed)
    : game_(std::move(content), seed)
{
    replay_.content = std::move(contentName);
    replay_.seed = seed;
    replay_.contentHash = game_.state().contentHash;
}

CommandResult ReplayRecorder::execute(const Command &command)
{
    const CommandResult result = game_.execute(command);
    replay_.commands.push_back({game_.state().tick, command, result});
    return result;
}

void ReplayRecorder::advance(qint64 ticks)
{
    for (qint64 done = 0; done < ticks && game_.state().status == GameStatus::Running; ++done) {
        // Written just before the tick runs, so that the commands of this tick are part of it.
        if (game_.state().tick % kCheckpointIntervalTicks == 0) {
            replay_.checkpoints.push_back({game_.state().tick, game_.hash()});
        }
        game_.tick();
    }
}

Replay ReplayRecorder::replay() const
{
    // advance() writes a checkpoint before a tick runs, so none can stand at the current tick yet.
    Replay replay = replay_;
    replay.checkpoints.push_back({game_.state().tick, game_.hash()});
    return replay;
}

ReplayPlayer::ReplayPlayer(Content content, Replay replay)
    : game_(std::move(content), replay.seed), replay_(std::move(replay))
{
    if (game_.state().contentHash != replay_.contentHash) {
        fail(u"the content differs: its hash is %1, the replay expects %2"_s.arg(
            hashToText(game_.state().contentHash), hashToText(replay_.contentHash)));
        return;
    }
    settleTick();
}

void ReplayPlayer::step(qint64 ticks)
{
    for (qint64 done = 0; done < ticks && !finished_; ++done) {
        if (game_.state().status != GameStatus::Running) {
            fail(u"the game ended, the replay expects tick %1"_s.arg(
                replay_.checkpoints[nextCheckpoint_].tick));
            return;
        }
        game_.tick();
        settleTick();
    }
}

void ReplayPlayer::settleTick()
{
    const qint64 tick = game_.state().tick;
    while (nextCommand_ < replay_.commands.size() && replay_.commands[nextCommand_].tick == tick) {
        const RecordedCommand &recorded = replay_.commands[nextCommand_++];
        const CommandResult result = game_.execute(recorded.command);
        if (result != recorded.result) {
            fail(u"tick %1: a command gave %2, the replay expects %3"_s.arg(tick).arg(
                toString(result), toString(recorded.result)));
            return;
        }
    }
    if (nextCheckpoint_ < replay_.checkpoints.size() &&
        replay_.checkpoints[nextCheckpoint_].tick == tick) {
        const quint64 expected = replay_.checkpoints[nextCheckpoint_++].hash;
        if (game_.hash() != expected) {
            fail(u"tick %1: the state hash is %2, the replay expects %3"_s.arg(tick).arg(
                hashToText(game_.hash()), hashToText(expected)));
            return;
        }
    }
    finished_ = nextCheckpoint_ >= replay_.checkpoints.size();
}

void ReplayPlayer::fail(const QString &message)
{
    error_ = message;
    finished_ = true;
}

QString verifyReplay(Content content, Replay replay)
{
    ReplayPlayer player(std::move(content), std::move(replay));
    player.step(std::numeric_limits<qint64>::max());
    return player.error();
}

} // namespace qtd::core
