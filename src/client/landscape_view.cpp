#include "client/landscape_view.h"

#include <algorithm>
#include <iterator>

namespace qtd::client {

GameSession *LandscapeView::game() const
{
    return game_;
}

void LandscapeView::setGame(GameSession *game)
{
    if (game_ == game) {
        return;
    }
    if (game_) {
        disconnect(game_, nullptr, this, nullptr);
    }
    game_ = game;
    if (game_) {
        connect(game_, &GameSession::mapChanged, this, &LandscapeView::refresh);
        connect(game_, &GameSession::towersChanged, this, &LandscapeView::towersChanged);
    }
    refresh();
    emit gameChanged();
}

void LandscapeView::refresh()
{
    const core::Grid *grid = game_ ? game_->grid() : nullptr;
    field_.reset();
    if (grid != nullptr) {
        landscape_ = landscapeOfMap(game_->mapFile());
        field_ = std::make_unique<GroundField>(*grid, landscape_);
    }
    ++generation_;
    emit landscapeChanged();
}

QColor LandscapeView::sky() const
{
    return landscape_.sky;
}

QColor LandscapeView::water() const
{
    return landscape_.water;
}

QColor LandscapeView::plain() const
{
    return landscape_.grassDark;
}

bool LandscapeView::lakes() const
{
    return landscape_.lakes;
}

int LandscapeView::generation() const
{
    return generation_;
}

bool LandscapeView::hasWater() const
{
    return landscape_.water.isValid();
}

float LandscapeView::waterLevel() const
{
    return landscape_.waterLevel;
}

const QString &LandscapeView::treeModel() const
{
    return landscape_.treeModel;
}

const QString &LandscapeView::rockModel() const
{
    return landscape_.rockModel;
}

QStringList LandscapeView::sceneryModels() const
{
    QStringList models;
    std::ranges::transform(landscape_.scenery,
                           std::back_inserter(models),
                           [](const SceneryItem &item) { return item.model; });
    return models;
}

const Landscape &LandscapeView::landscape() const
{
    return landscape_;
}

const GroundField *LandscapeView::field() const
{
    return field_.get();
}

} // namespace qtd::client
