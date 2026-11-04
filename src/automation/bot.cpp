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

/// A game draws how often a value is valid, one in so many. 0 means never: such a game builds
/// nothing and gets lost, the others mostly win.
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
        for (std::size_t slot = 0; slot < content.buildSlots.size(); ++slot) {
            if (std::ranges::find(state.towers, slot, &core::Tower::slot) == state.towers.end()) {
                return core::BuildTowerCommand{static_cast<qint64>(slot), 0};
            }
        }
        return std::nullopt;
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
        const qint64 slot = pick(static_cast<qint64>(game.content().buildSlots.size()));
        return core::BuildTowerCommand{slot,
                                       pick(static_cast<qint64>(game.content().towers.size()))};
    }

private:
    /// A valid index as often as this game wants, otherwise a value next to or far off the range.
    qint64 pick(qint64 size)
    {
        const std::array<qint64, 5> invalid{
            -1, size, size + 1, -core::kMaxExactInteger, core::kMaxExactInteger};
        if (validOneIn_ != 0 && random_.nextBelow(validOneIn_) == 0) {
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
