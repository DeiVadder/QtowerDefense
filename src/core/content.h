#pragma once

#include "core/fixed.h"

#include <QString>

#include <cstddef>
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

struct TowerType
{
    QString id;
    qint64 cost = 0;
    Fixed range;
    qint64 cooldownTicks = 1;
    Fixed damageMin; // every shot rolls a raw value in [damageMin, damageMax]
    Fixed damageMax;
    std::vector<Cell> footprint; // the cells it takes, relative to the cell it stands on
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
    Grid grid;
    Path path;
    std::vector<TowerType> towers;
    std::vector<EnemyType> enemies;
    std::vector<SpawnGroup> wave;
};

} // namespace qtd::core
