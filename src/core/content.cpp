#include "core/content.h"

#include <QLatin1StringView>

#include <algorithm>
#include <array>
#include <iterator>
#include <utility>

using namespace Qt::StringLiterals;

namespace qtd::core {

namespace {

constexpr qint64 kMaxSlowPercent = 90;

constexpr std::array<QLatin1StringView, kDamageTypes> kDamageTypeNames{
    "physical"_L1, "magic"_L1, "fire"_L1, "ice"_L1};
constexpr std::array<QLatin1StringView, 2> kStatusKindNames{"slow"_L1, "poison"_L1};
constexpr std::array<QLatin1StringView, kTargetPriorities> kTargetPriorityNames{
    "first"_L1, "last"_L1, "strongest"_L1, "nearest"_L1};

static_assert(kDamageTypeNames.size() == static_cast<std::size_t>(DamageType::Ice) + 1);
static_assert(kStatusKindNames.size() == static_cast<std::size_t>(StatusKind::Poison) + 1);
static_assert(kTargetPriorityNames.size() == static_cast<std::size_t>(TargetPriority::Nearest) + 1);

template <typename Value, std::size_t Size>
std::optional<Value> valueNamed(const std::array<QLatin1StringView, Size> &names, QStringView name)
{
    for (std::size_t index = 0; index < Size; ++index) {
        if (name == names[index]) {
            return static_cast<Value>(index);
        }
    }
    return std::nullopt;
}

/// Turns a cell around 0, 0. On the map, y grows towards the viewer, so x turns into y.
Cell turned(Cell offset, Rotation rotation)
{
    switch (rotation) {
    case Rotation::None:
        break;
    case Rotation::Quarter:
        return {-offset.y, offset.x};
    case Rotation::Half:
        return {-offset.x, -offset.y};
    case Rotation::ThreeQuarters:
        return {offset.y, -offset.x};
    }
    return offset;
}

} // namespace

Fixed maxSlow()
{
    return Fixed::fromInt(kMaxSlowPercent) / Fixed::fromInt(100);
}

std::optional<DamageType> damageTypeNamed(QStringView name)
{
    return valueNamed<DamageType>(kDamageTypeNames, name);
}

std::optional<StatusKind> statusKindNamed(QStringView name)
{
    return valueNamed<StatusKind>(kStatusKindNames, name);
}

std::optional<TargetPriority> targetPriorityNamed(QStringView name)
{
    return valueNamed<TargetPriority>(kTargetPriorityNames, name);
}

QString toString(TargetPriority priority)
{
    return kTargetPriorityNames[static_cast<std::size_t>(priority)];
}

Fixed hitShare(const EnemyType &enemy, DamageType damage)
{
    return Fixed::fromInt(1) - enemy.resistances[static_cast<std::size_t>(damage)];
}

Rotation nextRotation(Rotation rotation)
{
    return static_cast<Rotation>((static_cast<quint32>(rotation) + 1) % kRotations);
}

Point centreOf(Cell cell)
{
    return {Fixed::fromInt(cell.x), Fixed::fromInt(cell.y)};
}

Grid::Grid(qint64 width, qint64 height)
    : width_(width), height_(height), cells_(static_cast<std::size_t>(width * height))
{
}

bool Grid::contains(Cell cell) const
{
    return cell.x >= 0 && cell.x < width_ && cell.y >= 0 && cell.y < height_;
}

Terrain Grid::terrainAt(Cell cell) const
{
    return cells_[indexOf(cell)];
}

void Grid::setTerrain(Cell cell, Terrain terrain)
{
    cells_[indexOf(cell)] = terrain;
}

std::size_t Grid::indexOf(Cell cell) const
{
    return static_cast<std::size_t>(cell.y * width_ + cell.x);
}

std::vector<Cell> footprintAt(const TowerType &type, Cell cell, Rotation rotation)
{
    std::vector<Cell> cells;
    cells.reserve(type.footprint.size());
    std::ranges::transform(type.footprint, std::back_inserter(cells), [=](Cell offset) {
        const Cell place = turned(offset, rotation);
        return Cell{cell.x + place.x, cell.y + place.y};
    });
    return cells;
}

Path::Path(std::vector<Point> points) : points_(std::move(points))
{
    Fixed travelled;
    for (std::size_t index = 0; index < points_.size(); ++index) {
        if (index > 0) {
            travelled = travelled + distance(points_[index - 1], points_[index]);
        }
        lengths_.push_back(travelled);
    }
}

Point Path::positionAt(Fixed travelled) const
{
    if (points_.empty()) {
        return {};
    }
    if (travelled <= Fixed()) {
        return points_.front();
    }
    if (travelled >= length()) {
        return points_.back();
    }
    const std::vector<Fixed>::const_iterator next = std::ranges::upper_bound(lengths_, travelled);
    const auto end = static_cast<std::size_t>(std::distance(lengths_.begin(), next));
    const Point from = points_[end - 1];
    const Point to = points_[end];
    const Fixed along = travelled - lengths_[end - 1];
    const Fixed segment = lengths_[end] - lengths_[end - 1];
    return {from.x + (to.x - from.x) * along / segment, from.y + (to.y - from.y) * along / segment};
}

} // namespace qtd::core
