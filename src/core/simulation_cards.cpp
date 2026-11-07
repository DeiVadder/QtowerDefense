#include "core/simulation.h"

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

// The rules of the hand and of the coins, apart from those of the board.

namespace qtd::core {

CommandResult Simulation::check(const RollCardCommand & /*command*/) const
{
    if (state_.status != GameStatus::Running) {
        return CommandResult::GameOver;
    }
    if (state_.hand.size() >= kHandLimit) {
        return CommandResult::HandFull;
    }
    return state_.gold < rollCost(content_) ? CommandResult::NotEnoughGold
                                            : CommandResult::Accepted;
}

CommandResult Simulation::check(const BuyCardCommand &command) const
{
    if (state_.status != GameStatus::Running) {
        return CommandResult::GameOver;
    }
    if (command.towerType < 0 ||
        std::cmp_greater_equal(command.towerType, content_.towers.size())) {
        return CommandResult::UnknownTowerType;
    }
    if (state_.hand.size() >= kHandLimit) {
        return CommandResult::HandFull;
    }
    const TowerType &type = content_.towers[static_cast<std::size_t>(command.towerType)];
    return state_.gold < type.levels.front().cost ? CommandResult::NotEnoughGold
                                                  : CommandResult::Accepted;
}

CommandResult Simulation::check(const DiscardCardCommand &command) const
{
    if (state_.status != GameStatus::Running) {
        return CommandResult::GameOver;
    }
    return command.slot < 0 || std::cmp_greater_equal(command.slot, state_.hand.size())
               ? CommandResult::UnknownCard
               : CommandResult::Accepted;
}

CommandResult Simulation::check(const CollectCoinCommand &command) const
{
    if (state_.status != GameStatus::Running) {
        return CommandResult::GameOver;
    }
    return std::ranges::find(state_.coins, command.coin, &BonusCoin::id) == state_.coins.end()
               ? CommandResult::UnknownCoin
               : CommandResult::Accepted;
}

CommandResult Simulation::run(const RollCardCommand &command)
{
    const CommandResult result = check(command);
    if (result == CommandResult::Accepted) {
        const qint64 cost = rollCost(content_);
        state_.gold -= cost;
        const auto towers = static_cast<quint32>(content_.towers.size());
        state_.hand.push_back({state_.random.nextBelow(towers), cost});
    }
    return result;
}

CommandResult Simulation::run(const BuyCardCommand &command)
{
    const CommandResult result = check(command);
    if (result == CommandResult::Accepted) {
        const auto type = static_cast<std::size_t>(command.towerType);
        const qint64 cost = content_.towers[type].levels.front().cost;
        state_.gold -= cost;
        state_.hand.push_back({type, cost});
    }
    return result;
}

CommandResult Simulation::run(const DiscardCardCommand &command)
{
    const CommandResult result = check(command);
    if (result == CommandResult::Accepted) {
        state_.hand.erase(state_.hand.begin() + static_cast<std::ptrdiff_t>(command.slot));
    }
    return result;
}

CommandResult Simulation::run(const CollectCoinCommand &command)
{
    const CommandResult result = check(command);
    if (result == CommandResult::Accepted) {
        const std::vector<BonusCoin>::const_iterator coin =
            std::ranges::find(state_.coins, command.coin, &BonusCoin::id);
        state_.gold += coin->value;
        state_.coins.erase(coin);
    }
    return result;
}

std::vector<Card>::const_iterator Simulation::cardOfType(std::size_t type) const
{
    return std::ranges::find(state_.hand, type, &Card::type);
}

void Simulation::fadeCoins()
{
    for (BonusCoin &coin : state_.coins) {
        --coin.ticksLeft;
    }
    std::erase_if(state_.coins, [](const BonusCoin &coin) { return coin.ticksLeft <= 0; });
}

void Simulation::dropCoin(const Enemy &enemy)
{
    const qint64 value = content_.enemies[enemy.type].bounty * content_.coinPercent / 100;
    if (value > 0) {
        state_.coins.push_back({state_.nextCoinId++, position(enemy), value, content_.coinTicks});
    }
}

} // namespace qtd::core
