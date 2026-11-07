#pragma once

#include "core/simulation.h"

#include <QtTypes>

#include <memory>
#include <optional>

namespace qtd::automation {

/// The players that need no script. Bots give the same commands as a human.
enum class BotKind : quint8 {
    Idle,  // does nothing
    Naive, // builds the first tower type beside the path, whenever the gold is there
    Fuzz,  // random commands, many of them invalid
    Good,  // builds and upgrades where the gold does the most damage, casts into crowds (E-38)
};

/// Decides what to do. It only reads the game and answers with commands.
class Bot
{
public:
    virtual ~Bot() = default;

    /// The next command of the current tick, or nothing to let the tick run.
    virtual std::optional<core::Command> nextCommand(const core::Simulation &game) = 0;
};

/// The seed is the one of the game, so that a run stays repeatable.
std::unique_ptr<Bot> makeBot(BotKind kind, quint64 seed);

} // namespace qtd::automation
