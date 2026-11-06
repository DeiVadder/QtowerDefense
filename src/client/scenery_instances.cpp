#include "client/scenery_instances.h"

#include "core/content.h"

#include <QColor>
#include <QPoint>
#include <QVariant>
#include <QVector3D>
#include <QVector4D>

#include <cstddef>
#include <cstring>
#include <set>
#include <utility>
#include <vector>

namespace qtd::client {

namespace {

constexpr int kTriesPerPlant = 8;
constexpr float kBorderStart = 0.9F;
constexpr float kBorderEnd = 4;
constexpr float kFarStart = 3.5F;
constexpr float kFarSouthStart = 6; // the camera looks from the south, nothing tall in front
constexpr float kPathRoom = 0.05F;

struct Plant
{
    float x = 0;
    float y = 0;
};

using Cells = std::set<std::pair<int, int>>;

Cells towerCellsOf(const GameSession &game)
{
    Cells cells;
    const QVariantList towers = game.towerCells();
    for (const QVariant &tower : towers) {
        const QPoint cell = tower.toPoint();
        cells.insert({cell.x(), cell.y()});
    }
    return cells;
}

bool onLand(const GroundField &field, Plant plant)
{
    return field.pathAt(plant.x, plant.y) < kPathRoom && !field.isUnderWater(plant.x, plant.y);
}

bool fitsBoard(const GameSession &game, Plant plant)
{
    const int x = qRound(plant.x);
    const int y = qRound(plant.y);
    const core::Grid *grid = game.grid();
    if (x < 0 || y < 0 || x >= grid->width() || y >= grid->height()) {
        return false;
    }
    return grid->terrainAt({x, y}) == core::Terrain::Open;
}

bool fits(SceneryZone zone, const GameSession &game, const GroundField &field, Plant plant)
{
    const float distance = field.distanceToGrid(plant.x, plant.y);
    switch (zone) {
    case SceneryZone::Board:
        return fitsBoard(game, plant) && onLand(field, plant);
    case SceneryZone::Border:
        return distance > kBorderStart && distance < kBorderEnd && onLand(field, plant);
    case SceneryZone::Surroundings: {
        const bool south = plant.y > static_cast<float>(field.height());
        return distance > (south ? kFarSouthStart : kFarStart) && onLand(field, plant);
    }
    case SceneryZone::Water:
        return distance > kBorderStart && field.isUnderWater(plant.x, plant.y);
    }
    return false;
}

/// Where a try puts a plant: the zones near the board do not search the whole ground.
Plant tryPlace(SceneryZone zone, const GroundField &field, int item, int attempt)
{
    const bool near = zone == SceneryZone::Board || zone == SceneryZone::Border;
    const float reach = near ? kBorderEnd : static_cast<float>(kGroundMargin) - 1;
    const float spanX = static_cast<float>(field.width()) + 2 * reach;
    const float spanY = static_cast<float>(field.height()) + 2 * reach;
    return {groundRandom(item, attempt, 1) * spanX - reach,
            groundRandom(item, attempt, 2) * spanY - reach};
}

} // namespace

LandscapeView *SceneryInstances::view() const
{
    return view_;
}

void SceneryInstances::setView(LandscapeView *view)
{
    if (view_ == view) {
        return;
    }
    if (view_) {
        disconnect(view_, nullptr, this, nullptr);
    }
    view_ = view;
    if (view_) {
        connect(view_, &LandscapeView::landscapeChanged, this, &SceneryInstances::markDirty);
        connect(view_, &LandscapeView::towersChanged, this, &SceneryInstances::towersMoved);
    }
    markDirty();
    emit viewChanged();
}

void SceneryInstances::towersMoved()
{
    const std::vector<SceneryItem> &items = view_->landscape().scenery;
    if (std::cmp_less(item_, items.size()) &&
        items[static_cast<std::size_t>(item_)].zone == SceneryZone::Board) {
        markDirty();
    }
}

int SceneryInstances::item() const
{
    return item_;
}

void SceneryInstances::setItem(int item)
{
    if (item_ != item) {
        item_ = item;
        markDirty();
        emit itemChanged();
    }
}

float SceneryInstances::fieldSize() const
{
    return fieldSize_;
}

void SceneryInstances::setFieldSize(float size)
{
    if (!qFuzzyCompare(fieldSize_, size)) {
        fieldSize_ = size;
        markDirty();
        emit fieldSizeChanged();
    }
}

QByteArray SceneryInstances::getInstanceBuffer(int *instanceCount)
{
    QByteArray buffer;
    int shown = 0;
    const GroundField *field = view_ ? view_->field() : nullptr;
    if (field != nullptr && view_->game() != nullptr &&
        std::cmp_less(item_, view_->landscape().scenery.size())) {
        const SceneryItem &kind = view_->landscape().scenery[static_cast<std::size_t>(item_)];
        const GameSession &game = *view_->game();
        const Cells towers = kind.zone == SceneryZone::Board ? towerCellsOf(game) : Cells();
        const int tries = kind.count * kTriesPerPlant;
        int placed = 0;
        for (int attempt = 0; attempt < tries && placed < kind.count; ++attempt) {
            const Plant plant = tryPlace(kind.zone, *field, item_, attempt);
            if (!fits(kind.zone, game, *field, plant)) {
                continue;
            }
            ++placed; // a tower hides a plant, the others stay where they are
            if (towers.contains({qRound(plant.x), qRound(plant.y)})) {
                continue;
            }
            const float share = groundRandom(item_, attempt, 3);
            const float size = (kind.minSize + (kind.maxSize - kind.minSize) * share) * fieldSize_;
            const float ground = kind.zone == SceneryZone::Water
                                     ? view_->waterLevel()
                                     : field->heightAt(plant.x, plant.y);
            const float light = 0.8F + 0.2F * groundRandom(item_, attempt, 4);
            const QColor &tint = view_->landscape().sceneryTint;
            const InstanceTableEntry entry = calculateTableEntry(
                QVector3D(plant.x * fieldSize_, ground * fieldSize_, plant.y * fieldSize_),
                QVector3D(size, size, size),
                QVector3D(0, 360 * groundRandom(item_, attempt, 5), 0),
                QColor::fromRgbF(tint.redF() * light, tint.greenF() * light, tint.blueF() * light),
                QVector4D());
            buffer.resize(buffer.size() + qsizetype{sizeof(entry)});
            std::memcpy(
                buffer.data() + buffer.size() - qsizetype{sizeof(entry)}, &entry, sizeof(entry));
            ++shown;
        }
    }
    if (instanceCount != nullptr) {
        *instanceCount = shown;
    }
    return buffer;
}

} // namespace qtd::client
