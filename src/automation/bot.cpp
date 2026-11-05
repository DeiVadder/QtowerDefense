#include "automation/bot.h"

#include "core/json_reader.h"
#include "core/random.h"

#include <algorithm>
#include <array>
#include <cstddef>

namespace qtd::automation {

namespace {

constexpr quint64 kFuzzStream = 99; // apart from the stream of the game
constexpr quint32 kMaxFuzzCommandsPerTick = 3;
constexpr quint32 kCommandValues = 3; // x, y and the tower type

/// A game draws how often a command is well-formed, one in so many. 0 means never: such a game
/// builds nothing and gets lost. A well-formed command can still meet a blocked cell.
constexpr std::array<quint32, 3> kFuzzValidOneIn{0, 8, 2};

class IdleBot : public Bot
{
public:
    std::optional<core::Command> nextCommand(const core::Simulation & /*game*/) override
    {
        return std::nullopt;
    }
};

class NaiveBot : public Bot
{
public:
    std::optional<core::Command> nextCommand(const core::Simulation &game) override
    {
        const core::SimulationState &state = game.state();
        const core::Content &content = game.content();
        if (state.status != core::GameStatus::Running || state.gold < content.towers[0].cost) {
            return std::nullopt;
        }
        for (qint64 y = 0; y < content.grid.height(); ++y) {
            for (qint64 x = 0; x < content.grid.width(); ++x) {
                const core::BuildTowerCommand command{{x, y}, 0};
                if (nextToPath(content.grid, command.cell) &&
                    game.check(command) == core::CommandResult::Accepted) {
                    return command;
                }
            }
        }
        return std::nullopt;
    }

private:
    static bool nextToPath(const core::Grid &grid, core::Cell cell)
    {
        const std::array<core::Cell, 4> neighbours{core::Cell{cell.x - 1, cell.y},
                                                   core::Cell{cell.x + 1, cell.y},
                                                   core::Cell{cell.x, cell.y - 1},
                                                   core::Cell{cell.x, cell.y + 1}};
        return std::ranges::any_of(neighbours, [&grid](core::Cell neighbour) {
            return grid.contains(neighbour) && grid.terrainAt(neighbour) == core::Terrain::Path;
        });
    }
};

class FuzzBot : public Bot
{
public:
    explicit FuzzBot(quint64 seed)
        : random_(seed, kFuzzStream),
          validOneIn_(
              kFuzzValidOneIn[random_.nextBelow(static_cast<quint32>(kFuzzValidOneIn.size()))])
    {
    }

    std::optional<core::Command> nextCommand(const core::Simulation &game) override
    {
        if (game.state().tick != tick_ || !started_) {
            started_ = true;
            tick_ = game.state().tick;
            commandsLeft_ = random_.nextBelow(kMaxFuzzCommandsPerTick + 1);
        }
        if (commandsLeft_ == 0) {
            return std::nullopt;
        }
        --commandsLeft_;
        const core::Content &content = game.content();
        const bool wellFormed = validOneIn_ != 0 && random_.nextBelow(validOneIn_) == 0;
        // A broken command breaks one value, so that each of them reaches its check.
        const quint32 broken = wellFormed ? kCommandValues : random_.nextBelow(kCommandValues);
        const qint64 x = pick(content.grid.width(), broken != 0);
        const qint64 y = pick(content.grid.height(), broken != 1);
        return core::BuildTowerCommand{
            {x, y}, pick(static_cast<qint64>(content.towers.size()), broken != 2)};
    }

private:
    /// A valid index, or a value next to or far off the range.
    qint64 pick(qint64 size, bool valid)
    {
        const std::array<qint64, 5> invalid{
            -1, size, size + 1, -core::kMaxExactInteger, core::kMaxExactInteger};
        if (valid) {
            return random_.nextBelow(static_cast<quint32>(size));
        }
        return invalid[random_.nextBelow(static_cast<quint32>(invalid.size()))];
    }

    core::Random random_;
    quint32 validOneIn_;
    qint64 tick_ = 0;
    quint32 commandsLeft_ = 0;
    bool started_ = false;
};

} // namespace

std::unique_ptr<Bot> makeBot(BotKind kind, quint64 seed)
{
    switch (kind) {
    case BotKind::Naive:
        return std::make_unique<NaiveBot>();
    case BotKind::Fuzz:
        return std::make_unique<FuzzBot>(seed);
    case BotKind::Idle:
        break;
    }
    return std::make_unique<IdleBot>();
}

} // namespace qtd::automation
