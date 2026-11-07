#pragma once

#include "core/simulation.h"

#include <QByteArray>
#include <QByteArrayView>
#include <QString>
#include <QStringView>
#include <QtTypes>

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

namespace qtd::core {

class ObjectReader;

/// Ticks between two checkpoints of a recording, one second of game time.
constexpr qint64 kCheckpointIntervalTicks = kTicksPerSecond;

/// A command as it was given. It ran after all ticks up to `tick`, before the next one.
struct RecordedCommand
{
    qint64 tick = 0;
    Command command;
    CommandResult result = CommandResult::Accepted;
};

/// The state hash at the end of a tick number, after the commands of that tick.
struct Checkpoint
{
    qint64 tick = 0;
    quint64 hash = 0;
};

/// A game as data: same content, same seed and same commands must give the same checkpoints (E-20).
/// The last checkpoint is the end of the recording.
struct Replay
{
    QString content; // the name of the content file, for people and tools
    quint64 seed = 0;
    quint64 contentHash = 0;
    GameMode mode = GameMode::Campaign; // in the file only when it is endless
    std::vector<RecordedCommand> commands;
    std::vector<Checkpoint> checkpoints;
};

/// Holds the replay, or an error text that names the place of the problem.
struct ReplayLoadResult
{
    std::optional<Replay> replay;
    QString error;
};

/// Sixteen lowercase hex digits, the form of all hashes in files and reports.
QString hashToText(quint64 hash);
std::optional<quint64> hashFromText(QStringView text);

/// The text of a command result in files and reports.
QString toString(CommandResult result);
QString toString(GameStatus status);
QString toString(GameMode mode);
/// "campaign" or "endless", nothing for any other text.
std::optional<GameMode> gameModeFromText(QStringView text);

/// A seed from its decimal text, the only form it has outside. Signs and spaces do not count.
std::optional<quint64> seedFromText(const QString &text);

/// The optional key "rotation" of a build command: quarter turns from 0 to 3, none if it is
/// missing.
Rotation readRotation(ObjectReader &item);

/// The file format, stable for equal replays. See docs/Architektur.md.
QByteArray toJson(const Replay &replay);

/// Reads and validates a replay.
ReplayLoadResult parseReplay(QByteArrayView json);

/// Reads a script: the format of a replay, in which contentHash, the results and the checkpoints
/// are optional and have no meaning. Any replay is a valid script.
ReplayLoadResult parseScript(QByteArrayView json);

/// Reads a file and calls parseReplay or parseScript. The error text starts with the file name.
ReplayLoadResult loadReplayFile(const QString &fileName);
ReplayLoadResult loadScriptFile(const QString &fileName);

/// Plays a game and writes down all that is needed to repeat it, rejected commands too.
class ReplayRecorder
{
public:
    ReplayRecorder(Content content,
                   QString contentName,
                   quint64 seed,
                   GameMode mode = GameMode::Campaign);

    const Simulation &simulation() const
    {
        return game_;
    }

    CommandResult execute(const Command &command);

    /// Runs up to the given number of ticks, fewer if the game ends.
    void advance(qint64 ticks);

    /// The recording up to now. It ends with a checkpoint of the current tick.
    Replay replay() const;

private:
    Simulation game_;
    Replay replay_;
};

/// A game that goes on, or the reason why it cannot.
struct ResumeResult
{
    std::unique_ptr<ReplayRecorder> game;
    QString error;
};

/// A saved game is a replay of a running game (E-31): this repeats it to its last tick and records
/// on from there. It fails if the commands or the last hash come out differently.
ResumeResult resumeGame(Content content, const Replay &saved);
/// The same from two files. The error text starts with the name of the file.
ResumeResult resumeGame(const QString &mapFile, const QString &saveFile);

/// Repeats a replay in steps of any size and compares results and checkpoints on the way. The first
/// difference ends the run.
class ReplayPlayer
{
public:
    ReplayPlayer(Content content, Replay replay);

    const Simulation &simulation() const
    {
        return game_;
    }

    /// True after the last checkpoint or after the first difference.
    bool finished() const
    {
        return finished_;
    }

    /// Describes the first difference, empty if there is none so far.
    const QString &error() const
    {
        return error_;
    }

    /// Runs up to the given number of ticks of the replay.
    void step(qint64 ticks);

private:
    void settleTick();
    void fail(const QString &message);

    Simulation game_;
    Replay replay_;
    std::size_t nextCommand_ = 0;
    std::size_t nextCheckpoint_ = 0;
    bool finished_ = false;
    QString error_;
};

/// Plays a replay to its end. Returns the first difference, empty if there is none.
QString verifyReplay(Content content, Replay replay);

} // namespace qtd::core
