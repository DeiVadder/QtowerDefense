#pragma once

#include "core/fixed.h"

#include <QString>
#include <QStringView>

#include <array>
#include <cstddef>
#include <optional>
#include <vector>

namespace qtd::core {

/// A field of the chessboard. Its centre is the point (x, y) of the map, see centreOf().
struct Cell
{
    qint64 x = 0;
    qint64 y = 0;

    constexpr auto operator<=>(const Cell &) const = default;
};

Point centreOf(Cell cell);

/// The cell that a point on the map lies on. The point must not lie left of or above the map.
Cell cellAt(Point point);

/// What lies on a cell. Towers only stand on open cells.
enum class Terrain : quint8 {
    Open,
    Path,
    Rock,
    Tree,
    Water,
};

/// The chessboard of a map, row by row.
class Grid
{
public:
    Grid() = default;
    Grid(qint64 width, qint64 height);

    qint64 width() const
    {
        return width_;
    }
    qint64 height() const
    {
        return height_;
    }
    const std::vector<Terrain> &cells() const
    {
        return cells_;
    }
    bool contains(Cell cell) const;

    /// The cell must be on the grid.
    Terrain terrainAt(Cell cell) const;
    void setTerrain(Cell cell, Terrain terrain);

private:
    std::size_t indexOf(Cell cell) const;

    qint64 width_ = 0;
    qint64 height_ = 0;
    std::vector<Terrain> cells_;
};

/// A fixed route for enemies: straight segments through at least two different points.
class Path
{
public:
    Path() = default;
    explicit Path(std::vector<Point> points);

    const std::vector<Point> &points() const
    {
        return points_;
    }
    Fixed length() const
    {
        return lengths_.empty() ? Fixed() : lengths_.back();
    }

    /// The point at the given distance from the start, clamped to the ends of the path.
    Point positionAt(Fixed travelled) const;

private:
    std::vector<Point> points_;
    std::vector<Fixed> lengths_; // distance from the start to each point
};

/// How far a tower is turned clockwise, around the cell it stands on.
enum class Rotation : quint8 {
    None,
    Quarter,
    Half,
    ThreeQuarters,
};
constexpr quint32 kRotations = 4;

/// The next quarter turn, after the last one comes None.
Rotation nextRotation(Rotation rotation);

enum class DamageType : quint8 {
    Physical,
    Magic,
    Fire,
    Ice,
};
constexpr std::size_t kDamageTypes = 4;

enum class StatusKind : quint8 {
    Slow,   // strength: the share of the speed that is lost
    Poison, // strength: the damage per tick
};

/// The strongest slow there is: an enemy always keeps moving, so that every game ends.
Fixed maxSlow();

/// What a hit leaves behind, on top of its damage.
struct StatusEffect
{
    StatusKind kind = StatusKind::Slow;
    Fixed strength;
    qint64 durationTicks = 1;
};

/// Which enemy in range a tower fires at. Ties go to the lowest id.
enum class TargetPriority : quint8 {
    First,     // the furthest along the path
    Last,      // the least far along the path
    Strongest, // the most health left
    Nearest,   // the closest to the tower
};
constexpr quint32 kTargetPriorities = 4;

/// The names of these values in files and requests.
std::optional<DamageType> damageTypeNamed(QStringView name);
std::optional<StatusKind> statusKindNamed(QStringView name);
std::optional<TargetPriority> targetPriorityNamed(QStringView name);
QString toString(TargetPriority priority);

/// What a tower does at one level.
struct TowerLevel
{
    qint64 cost = 0; // to build the tower, or to reach this level from the one before
    Fixed range;
    qint64 cooldownTicks = 1;
    Fixed damageMin; // every shot rolls a raw value in [damageMin, damageMax]
    Fixed damageMax;
    Fixed splashRadius;                // around the target, 0 hits the target alone
    std::vector<StatusEffect> effects; // at most one of each kind
};

struct TowerType
{
    QString id;
    DamageType damageType = DamageType::Physical;
    std::vector<Cell> footprint;    // the cells it takes, relative to the cell it stands on
    std::vector<TowerLevel> levels; // at least one: what gets built, then the upgrades
};

/// The cells a tower of this type takes when it stands on the given cell, turned as given.
std::vector<Cell> footprintAt(const TowerType &type, Cell cell, Rotation rotation);

struct EnemyType
{
    QString id;
    Fixed health;
    Fixed speedPerTick;
    qint64 bounty = 0;
    qint64 lifeCost = 1;
    Fixed regeneration; // health per tick, up to its start value, not while poisoned
    bool boss = false;  // only the look: the rules treat a boss like any other enemy
    // By damage type, the share of a hit that does not count: 1 is immune, -1 takes double. It
    // applies to the damage and to the strength of status effects alike.
    std::array<Fixed, kDamageTypes> resistances{};
};

/// What is left of a hit of this damage type on this enemy, from 0 to 2.
Fixed hitShare(const EnemyType &enemy, DamageType damage);

/// What a player casts at a cell of the map, when it has cooled down. Every enemy in the radius
/// takes one roll of damage with its own resistance, and the effects.
struct SpellType
{
    QString id;
    qint64 cooldownTicks = 1;
    Fixed radius;
    Fixed damageMin; // may be 0, for a spell that only leaves its effects behind
    Fixed damageMax;
    DamageType damageType = DamageType::Physical;
    std::vector<StatusEffect> effects; // at most one of each kind
};

/// Enemies of one type that enter the path one after the other.
struct SpawnGroup
{
    std::size_t enemyType = 0;
    qint64 count = 1;
    qint64 delayTicks = 0;    // ticks to wait before the first enemy of this group
    qint64 intervalTicks = 1; // ticks from one enemy of this group to the next
};

/// Everything the data files decide. Times are in ticks and speeds in tiles per tick, so no value
/// needs a conversion that rounds (E-16).
struct Content
{
    qint64 startGold = 0;
    qint64 startLives = 1;
    qint64 sellPercent = 70;          // of all the gold a tower cost, rounded down
    qint64 earlyCallTicksPerGold = 0; // a wave called early pays per so many ticks, 0 pays nothing
    qint64 rollPercent = 80;          // a roll of the dice, of the cheapest tower, rounded down
    qint64 coinPercent = 25;          // a bonus coin, of the bounty, rounded down
    qint64 coinTicks = 120;           // until a bonus coin is gone
    Grid grid;
    Path path;
    std::vector<TowerType> towers;
    std::vector<EnemyType> enemies;
    std::vector<SpawnGroup> wave;
    std::vector<SpellType> spells;
};

/// The gold for a roll of the dice, below the cheapest tower unless that one is free. Content
/// has at least one tower.
qint64 rollCost(const Content &content);

} // namespace qtd::core
