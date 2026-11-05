#include "client/map_instances.h"

#include "core/content.h"

#include <QColor>
#include <QPoint>
#include <QPointF>
#include <QVector3D>
#include <QVector4D>

#include <algorithm>
#include <cstring>
#include <iterator>
#include <vector>

namespace qtd::client {

namespace {

/// A cube of the model has this edge length.
constexpr float kModelSize = 100;
/// The board has no gaps, a tap always finds a cell. Towers stand apart from each other.
constexpr float kBoardShare = 1;
constexpr float kTowerShare = 0.9F;
/// Heights in shares of a field.
constexpr float kFlatHeight = 0.04F;
constexpr float kRockHeight = 0.5F;
constexpr float kTreeHeight = 0.9F;
constexpr float kTowerHeight = 0.7F;
constexpr float kTowerHeightPerLevel = 0.25F;
constexpr float kBossScale = 1.8F;
constexpr int kChosenLight = 150; // percent, how much brighter the chosen tower is

struct Piece
{
    QVector3D position;
    QVector3D scale;
    QColor color;
};

struct Look
{
    QColor color;
    float height = kFlatHeight;
};

Look lookOf(core::Terrain terrain, core::Cell cell)
{
    switch (terrain) {
    case core::Terrain::Open:
        return {QColor((cell.x + cell.y) % 2 == 0 ? 0x4f7942 : 0x5c8a4e)};
    case core::Terrain::Path:
        return {QColor((cell.x + cell.y) % 2 == 0 ? 0x8a6f4d : 0x96795a)};
    case core::Terrain::Rock:
        return {QColor(0x80817f), kRockHeight};
    case core::Terrain::Tree:
        return {QColor(0x2f5d34), kTreeHeight};
    case core::Terrain::Water:
        return {QColor(0x3b6ea5)};
    }
    return {};
}

/// A block that stands on the cell.
Piece blockOn(QPoint cell, float fieldSize, float share, const QColor &color, float height)
{
    const float size = fieldSize / kModelSize;
    return {QVector3D(static_cast<float>(cell.x()) * fieldSize,
                      height * fieldSize / 2,
                      static_cast<float>(cell.y()) * fieldSize),
            QVector3D(size * share, size * height, size * share),
            color};
}

std::vector<Piece> boardPieces(const GameSession &game, float fieldSize)
{
    std::vector<Piece> pieces;
    const core::Grid *grid = game.grid();
    if (grid == nullptr) {
        return pieces;
    }
    pieces.reserve(grid->cells().size());
    for (qint64 y = 0; y < grid->height(); ++y) {
        for (qint64 x = 0; x < grid->width(); ++x) {
            const core::Cell cell{x, y};
            const Look look = lookOf(grid->terrainAt(cell), cell);
            pieces.push_back(blockOn(QPoint(static_cast<int>(x), static_cast<int>(y)),
                                     fieldSize,
                                     kBoardShare,
                                     look.color,
                                     look.height));
        }
    }
    return pieces;
}

QColor colorOf(core::DamageType damage)
{
    switch (damage) {
    case core::DamageType::Physical:
        break;
    case core::DamageType::Magic:
        return QColor(0x8e5bd0);
    case core::DamageType::Fire:
        return QColor(0xd9622b);
    case core::DamageType::Ice:
        return QColor(0x6fc3e8);
    }
    return QColor(0xc9a227);
}

QColor colorOf(const core::Enemy &enemy)
{
    const bool slowed = enemy.slow.ticks > 0;
    const bool poisoned = enemy.poison.ticks > 0;
    if (slowed && poisoned) {
        return QColor(0x3fa7a0);
    }
    if (slowed) {
        return QColor(0x5b8fd9);
    }
    return poisoned ? QColor(0x4fa84a) : QColor(0xb5372e);
}

/// In the order of GameSession::towerCells(), a tap on a piece names the cell that way.
std::vector<Piece> towerPieces(const GameSession &game, float fieldSize)
{
    std::vector<Piece> pieces;
    const core::Simulation *simulation = game.simulation();
    if (simulation == nullptr) {
        return pieces;
    }
    for (const core::Tower &tower : simulation->state().towers) {
        const core::TowerType &type = simulation->content().towers[tower.type];
        const QColor color = colorOf(type.damageType);
        const QColor shown = tower.id == game.chosenTower() ? color.lighter(kChosenLight) : color;
        std::ranges::transform(core::footprintAt(type, tower.cell, tower.rotation),
                               std::back_inserter(pieces),
                               [&](core::Cell cell) {
                                   return blockOn(
                                       QPoint(static_cast<int>(cell.x), static_cast<int>(cell.y)),
                                       fieldSize,
                                       kTowerShare,
                                       shown,
                                       kTowerHeight +
                                           kTowerHeightPerLevel * static_cast<float>(tower.level));
                               });
    }
    return pieces;
}

std::vector<Piece> enemyPieces(const GameSession &game, float fieldSize, float enemyScale)
{
    std::vector<Piece> pieces;
    const std::vector<QPointF> places = game.enemyPlaces();
    if (places.empty()) {
        return pieces;
    }
    const std::vector<core::Enemy> &enemies = game.simulation()->state().enemies;
    const std::vector<core::EnemyType> &types = game.simulation()->content().enemies;
    pieces.reserve(places.size());
    std::ranges::transform(
        places, enemies, std::back_inserter(pieces), [&](QPointF place, const core::Enemy &enemy) {
            return Piece{QVector3D(static_cast<float>(place.x()) * fieldSize,
                                   0,
                                   static_cast<float>(place.y()) * fieldSize),
                         QVector3D(1, 1, 1) * enemyScale *
                             (types[enemy.type].boss ? kBossScale : 1.0F),
                         colorOf(enemy)};
        });
    return pieces;
}

} // namespace

MapInstances::MapInstances(QQuick3DObject *parent) : QQuick3DInstancing(parent) {}

GameSession *MapInstances::game() const
{
    return game_;
}

MapInstances::Layer MapInstances::layer() const
{
    return layer_;
}

float MapInstances::fieldSize() const
{
    return fieldSize_;
}

float MapInstances::enemyScale() const
{
    return enemyScale_;
}

void MapInstances::setGame(GameSession *game)
{
    if (game_ == game) {
        return;
    }
    if (game_) {
        disconnect(game_, nullptr, this, nullptr);
    }
    game_ = game;
    if (game_) {
        connect(game_, &GameSession::mapChanged, this, [this] { markLayerDirty(Layer::Board); });
        connect(
            game_, &GameSession::towersChanged, this, [this] { markLayerDirty(Layer::Towers); });
        connect(
            game_, &GameSession::stateChanged, this, [this] { markLayerDirty(Layer::Enemies); });
        connect(
            game_, &GameSession::frameChanged, this, [this] { markLayerDirty(Layer::Enemies); });
    }
    markDirty();
    emit gameChanged();
}

void MapInstances::setLayer(Layer layer)
{
    if (layer_ != layer) {
        layer_ = layer;
        markDirty();
        emit layerChanged();
    }
}

void MapInstances::setFieldSize(float size)
{
    if (!qFuzzyCompare(fieldSize_, size)) {
        fieldSize_ = size;
        markDirty();
        emit fieldSizeChanged();
    }
}

void MapInstances::setEnemyScale(float scale)
{
    if (!qFuzzyCompare(enemyScale_, scale)) {
        enemyScale_ = scale;
        markDirty();
        emit enemyScaleChanged();
    }
}

void MapInstances::markLayerDirty(Layer layer)
{
    if (layer_ == layer) {
        markDirty();
    }
}

QByteArray MapInstances::getInstanceBuffer(int *instanceCount)
{
    std::vector<Piece> pieces;
    if (game_) {
        switch (layer_) {
        case Layer::Board:
            pieces = boardPieces(*game_, fieldSize_);
            break;
        case Layer::Towers:
            pieces = towerPieces(*game_, fieldSize_);
            break;
        case Layer::Enemies:
            pieces = enemyPieces(*game_, fieldSize_, enemyScale_);
            break;
        }
    }
    constexpr qsizetype entrySize = sizeof(InstanceTableEntry);
    QByteArray buffer(static_cast<qsizetype>(pieces.size()) * entrySize, Qt::Uninitialized);
    char *next = buffer.data();
    for (const Piece &piece : pieces) {
        const InstanceTableEntry entry =
            calculateTableEntry(piece.position, piece.scale, QVector3D(), piece.color, QVector4D());
        std::memcpy(next, &entry, entrySize);
        next += entrySize;
    }
    if (instanceCount != nullptr) {
        *instanceCount = static_cast<int>(pieces.size());
    }
    return buffer;
}

} // namespace qtd::client
