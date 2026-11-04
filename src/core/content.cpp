#include "core/content.h"

#include <algorithm>
#include <iterator>
#include <utility>

namespace qtd::core {

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
    const auto next = std::ranges::upper_bound(lengths_, travelled);
    const auto end = static_cast<std::size_t>(std::distance(lengths_.begin(), next));
    const Point from = points_[end - 1];
    const Point to = points_[end];
    const Fixed along = travelled - lengths_[end - 1];
    const Fixed segment = lengths_[end] - lengths_[end - 1];
    return {from.x + (to.x - from.x) * along / segment, from.y + (to.y - from.y) * along / segment};
}

} // namespace qtd::core
