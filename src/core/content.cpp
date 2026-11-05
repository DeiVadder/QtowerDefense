#include "core/content.h"

#include <algorithm>
#include <iterator>
#include <utility>

namespace qtd::core {

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

std::vector<Cell> footprintAt(const TowerType &type, Cell cell)
{
    std::vector<Cell> cells;
    cells.reserve(type.footprint.size());
    std::ranges::transform(type.footprint, std::back_inserter(cells), [cell](Cell offset) {
        return Cell{cell.x + offset.x, cell.y + offset.y};
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
