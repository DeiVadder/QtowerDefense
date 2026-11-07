#pragma once

#include "core/content.h"
#include "core/random.h"

#include <QtTypes>

#include <cstddef>
#include <optional>
#include <variant>
#include <vector>

namespace qtd::core {

/// The simulation only knows ticks. Whoever drives it decides how fast they run (E-17).
constexpr int kTicksPerSecond = 30;

/// The cards a player can hold at once.
constexpr std::size_t kHandLimit = 5;

enum class GameStatus : quint8 {
    Running,
    Won,
    Lost,
};

/// A status effect on an enemy, for the ticks that are left. Without ticks its strength is 0.
struct ActiveStatus
{
    qint64 ticks = 0;
    Fixed strength;
};

struct Enemy
{
    qint64 id = 0;
    std::size_t type = 0;
    Fixed health;
    Fixed travelled; // distance along the path
    ActiveStatus slow;
    ActiveStatus poison;
};

struct Tower
{
    qint64 id = 0;
    std::size_t type = 0;
    Cell cell;                // the cell it stands on, see TowerType::footprint
    qint64 cooldownTicks = 0; // ticks until the tower can fire
    Rotation rotation = Rotation::None;
    TargetPriority priority = TargetPriority::First;
    std::size_t level = 0; // an index into TowerType::levels
    qint64 spent = 0;      // all the gold it cost so far
};

/// A tower that is already paid for, whatever the card cost is what selling it counts.
struct Card
{
    std::size_t type = 0;
    qint64 paid = 0;
};

/// What a defeated enemy leaves behind, for whoever collects it in time.
struct BonusCoin
{
    qint64 id = 0;
    Point place;
    qint64 value = 0;
    qint64 ticksLeft = 0;
};

/// All that changes during a game. stateHash() must cover every field. Enemies and towers stay in
/// the order of their ids, and every rule walks them in that order.
struct SimulationState
{
    quint64 contentHash = 0;
    qint64 tick = 0;
    GameStatus status = GameStatus::Running;
    qint64 gold = 0;
    qint64 lives = 0;
    Random random{0, 0};
    qint64 nextEntityId = 1;
    std::size_t spawnGroup = 0; // the group that spawns next
    qint64 spawnedInGroup = 0;  // enemies of that group that are already out
    qint64 spawnCountdownTicks = 0;
    std::vector<Enemy> enemies;
    std::vector<Tower> towers;
    std::vector<qint64> spellCooldowns; // ticks until each spell of the content can be cast
    std::vector<Card> hand;             // at most kHandLimit, in the order they came
    qint64 nextCoinId = 1;              // apart from the entities: coins must not move their ids
    std::vector<BonusCoin> coins;
};

/// Takes a card of this type from the hand if there is one, else pays the full price.
struct BuildTowerCommand
{
    Cell cell;
    qint64 towerType = 0;
    Rotation rotation = Rotation::None;
};

struct SetTargetPriorityCommand
{
    qint64 tower = 0; // the id of a tower in the game
    TargetPriority priority = TargetPriority::First;
};

struct UpgradeTowerCommand
{
    qint64 tower = 0; // the id of a tower in the game
};

struct SellTowerCommand
{
    qint64 tower = 0; // the id of a tower in the game
};

struct CastSpellCommand
{
    qint64 spell = 0; // an index into Content::spells
    Cell cell;        // the centre of the spell
};

/// Lets the next wave start at the next tick, for gold.
struct CallWaveCommand
{
};

/// A random card for less than any tower costs.
struct RollCardCommand
{
};

/// The card of this type, at its full price.
struct BuyCardCommand
{
    qint64 towerType = 0;
};

/// Throws a card away, its gold is lost.
struct DiscardCardCommand
{
    qint64 slot = 0; // an index into the hand
};

struct CollectCoinCommand
{
    qint64 coin = 0; // the id of a coin in the game
};

/// What a player, human or bot, can ask for. New commands join this list.
using Command = std::variant<BuildTowerCommand,
                             SetTargetPriorityCommand,
                             UpgradeTowerCommand,
                             SellTowerCommand,
                             CastSpellCommand,
                             CallWaveCommand,
                             RollCardCommand,
                             BuyCardCommand,
                             DiscardCardCommand,
                             CollectCoinCommand>;

enum class CommandResult : quint8 {
    Accepted,
    GameOver,
    UnknownTowerType,
    OutsideMap,
    CellBlocked,
    CellOccupied,
    NotEnoughGold,
    UnknownTower,
    HighestLevel,
    UnknownSpell,
    SpellCooling,
    NoWaveWaiting,
    HandFull,
    UnknownCard,
    UnknownCoin,
};

/// One game. Same content, seed and commands at the same ticks give the same states, bit by bit.
class Simulation
{
public:
    /// The content must come from parseContent, which guarantees its limits.
    Simulation(Content content, quint64 seed);

    const Content &content() const
    {
        return content_;
    }
    const SimulationState &state() const
    {
        return state_;
    }

    /// What execute() would answer for this command right now.
    CommandResult check(const BuildTowerCommand &command) const;
    CommandResult check(const SetTargetPriorityCommand &command) const;
    CommandResult check(const UpgradeTowerCommand &command) const;
    CommandResult check(const SellTowerCommand &command) const;
    CommandResult check(const CastSpellCommand &command) const;
    CommandResult check(const CallWaveCommand &command) const;
    CommandResult check(const RollCardCommand &command) const;
    CommandResult check(const BuyCardCommand &command) const;
    CommandResult check(const DiscardCardCommand &command) const;
    CommandResult check(const CollectCoinCommand &command) const;

    /// The gold for calling the next wave right now: the ticks it would still wait, in gold.
    qint64 waveCallBonus() const;

    /// Applies a command between two ticks. Anything but Accepted leaves the state untouched.
    CommandResult execute(const Command &command);

    /// Runs one tick: spells cool down, coins fade, enemies move, new ones spawn, poison and
    /// regeneration work, towers fire, then the game may end. Does nothing once the game is over.
    void tick();

    /// Runs up to the given number of ticks, fewer if the game ends.
    void advance(qint64 ticks);

    /// The position of an enemy on the map.
    Point position(const Enemy &enemy) const;

    /// The tower with this id, nothing if there is none.
    const Tower *towerWithId(qint64 id) const;

    /// What a tower does right now.
    const TowerLevel &levelOf(const Tower &tower) const;

    /// What an upgrade would make of a tower, nothing at the highest level.
    const TowerLevel *nextLevelOf(const Tower &tower) const;

    /// The gold that selling a tower brings.
    qint64 sellValue(const Tower &tower) const;

    /// The hash of the current state, see stateHash().
    quint64 hash() const;

private:
    CommandResult run(const BuildTowerCommand &command);
    CommandResult run(const SetTargetPriorityCommand &command);
    CommandResult run(const UpgradeTowerCommand &command);
    CommandResult run(const SellTowerCommand &command);
    CommandResult run(const CastSpellCommand &command);
    CommandResult run(const CallWaveCommand &command);
    CommandResult run(const RollCardCommand &command);
    CommandResult run(const BuyCardCommand &command);
    CommandResult run(const DiscardCardCommand &command);
    CommandResult run(const CollectCoinCommand &command);
    /// GameOver, UnknownTower or Accepted, what all commands on a tower check first.
    CommandResult checkTowerCommand(qint64 id) const;
    bool hasTowerAt(Cell cell) const;
    /// The first card of this tower type, the end of the hand if there is none.
    std::vector<Card>::const_iterator cardOfType(std::size_t type) const;
    void fadeCoins();
    void dropCoin(const Enemy &enemy);
    void moveEnemies();
    void spawnEnemies();
    void updateHealth();
    void fireTowers();
    void updateStatus();
    void strike(const Tower &tower, const std::vector<Point> &positions, std::size_t target);
    void hit(Enemy *enemy,
             DamageType damageType,
             const std::vector<StatusEffect> &effects,
             Fixed damage);
    void hurt(Enemy *enemy, Fixed damage);
    void removeDeadEnemies();
    /// The index of the enemy that a tower fires at.
    std::optional<std::size_t> findTarget(const Tower &tower,
                                          const std::vector<Point> &positions) const;
    Fixed rollDamage(Fixed lowest, Fixed highest);

    Content content_;
    SimulationState state_;
};

} // namespace qtd::core
