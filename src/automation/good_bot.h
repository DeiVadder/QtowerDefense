#pragma once

#include "automation/bot.h"

#include <memory>

namespace qtd::automation {

/// A player that thinks (E-38): it builds and upgrades where its gold does the most damage to the
/// wave of the map, and casts its spells into crowds. It reads no hidden state and does not
/// cheat, it sees what a human sees.
std::unique_ptr<Bot> makeGoodBot();

} // namespace qtd::automation
