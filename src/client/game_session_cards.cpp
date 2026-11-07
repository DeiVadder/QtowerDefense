#include "client/game_session.h"

#include "core/simulation.h"

#include <QVariantList>

#include <variant>

// The hand, the dice and the coins: what a player does beside the board.

namespace qtd::client {

QVariantList GameSession::hand() const
{
    QVariantList cards;
    if (game_) {
        for (const core::Card &card : game_->simulation().state().hand) {
            cards.append(static_cast<int>(card.type));
        }
    }
    return cards;
}

int GameSession::handLimit()
{
    return static_cast<int>(core::kHandLimit);
}

int GameSession::rollCost() const
{
    return game_ ? static_cast<int>(core::rollCost(game_->simulation().content())) : 0;
}

int GameSession::chosenCard() const
{
    return chosenCard_;
}

void GameSession::chooseCard(int slot)
{
    const QVariantList cards = hand();
    if (slot < 0 || slot >= cards.size()) {
        forgetCard();
        return;
    }
    // A build takes the first card of its type, so that is the one that counts as chosen.
    chosenCard_ = static_cast<int>(cards.indexOf(cards.at(slot)));
    setSelectedTower(cards.at(slot).toInt());
    emit selectionChanged();
}

void GameSession::forgetCard()
{
    if (chosenCard_ >= 0) {
        chosenCard_ = -1;
        emit selectionChanged();
    }
}

bool GameSession::executeOnHand(const core::Command &command)
{
    if (!game_) {
        return false;
    }
    const core::CommandResult result = game_->execute(command);
    if (result == core::CommandResult::HandFull || result == core::CommandResult::NotEnoughGold) {
        emit handRefused(result == core::CommandResult::HandFull);
    }
    if (result != core::CommandResult::Accepted) {
        return false;
    }
    // First the dice, they keep the new card from showing too early.
    if (std::holds_alternative<core::RollCardCommand>(command)) {
        emit cardRolled(static_cast<int>(game_->simulation().state().hand.back().type));
    }
    emit handChanged();
    emit stateChanged();
    return true;
}

bool GameSession::rollCard()
{
    if (!executeOnHand(core::RollCardCommand{})) {
        return false;
    }
    emit cue(Cue::Roll);
    return true;
}

bool GameSession::buyCard(int towerType)
{
    if (!executeOnHand(core::BuyCardCommand{towerType})) {
        return false;
    }
    emit cue(Cue::Roll);
    return true;
}

bool GameSession::discardCard()
{
    if (chosenCard_ < 0) {
        return false;
    }
    const int slot = chosenCard_;
    forgetCard();
    return executeOnHand(core::DiscardCardCommand{slot});
}

bool GameSession::collectCoin(int coin)
{
    if (!game_ || game_->execute(core::CollectCoinCommand{coin}) != core::CommandResult::Accepted) {
        return false;
    }
    reportCoins();
    emit stateChanged();
    emit cue(Cue::Coin);
    return true;
}

} // namespace qtd::client
