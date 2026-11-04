#pragma once

#include "core/fixed.h"

#include <QString>

#include <cstddef>
#include <vector>

namespace qtd::core {

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

struct TowerType
{
    QString id;
    qint64 cost = 0;
    Fixed range;
    qint64 cooldownTicks = 1;
    Fixed damageMin; // every shot rolls a raw value in [damageMin, damageMax]
    Fixed damageMax;
};

struct EnemyType
{
    QString id;
    Fixed health;
    Fixed speedPerTick;
    qint64 bounty = 0;
    qint64 lifeCost = 1;
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
    Path path;
    std::vector<Point> buildSlots;
    std::vector<TowerType> towers;
    std::vector<EnemyType> enemies;
    std::vector<SpawnGroup> wave;
};

} // namespace qtd::core
