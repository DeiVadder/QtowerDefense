#pragma once

#include "core/content.h"
#include "core/random.h"

#include <QtTypes>

#include <cstddef>
#include <variant>
#include <vector>

namespace qtd::core {

/// The simulation only knows ticks. Whoever drives it decides how fast they run (E-17).
constexpr int kTicksPerSecond = 30;

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
};

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

/// What a player, human or bot, can ask for. New commands join this list.
using Command = std::variant<BuildTowerCommand, SetTargetPriorityCommand>;

enum class CommandResult : quint8 {
    Accepted,
    GameOver,
    UnknownTowerType,
    OutsideMap,
    CellBlocked,
    CellOccupied,
    NotEnoughGold,
    UnknownTower,
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

    /// Applies a command between two ticks. Anything but Accepted leaves the state untouched.
    CommandResult execute(const Command &command);

    /// Runs one tick: enemies move, new ones spawn, poison works, towers fire, then the game may
    /// end. Does nothing once the game is over.
    void tick();

    /// Runs up to the given number of ticks, fewer if the game ends.
    void advance(qint64 ticks);

    /// The position of an enemy on the map.
    Point position(const Enemy &enemy) const;

    /// The hash of the current state, see stateHash().
    quint64 hash() const;

private:
    CommandResult run(const BuildTowerCommand &command);
    CommandResult run(const SetTargetPriorityCommand &command);
    bool hasTowerAt(Cell cell) const;
    void moveEnemies();
    void spawnEnemies();
    void poisonEnemies();
    void fireTowers();
    void updateStatus();
    void hit(Enemy *enemy, const TowerType &tower);
    void hurt(Enemy *enemy, Fixed damage);
    void removeDeadEnemies();
    Enemy *findTarget(const Tower &tower, const std::vector<Point> &positions);
    Fixed rollDamage(const TowerType &type);

    Content content_;
    SimulationState state_;
};

} // namespace qtd::core
