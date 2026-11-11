#include "client/map_instances.h"

#include "client/shot_looks.h"
#include "core/content.h"

#include <QColor>
#include <QPoint>
#include <QPointF>
#include <QVector3D>
#include <QVector4D>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <iterator>
#include <utility>
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
constexpr float kTreeHeight = 0.06F; // the model takes the taps, the block hides no other cell
constexpr float kTowerHeight = 0.7F;
constexpr float kTowerHeightPerLevel = 0.25F;
constexpr float kBossScale = 1.8F;
constexpr int kChosenLight = 150;      // percent, how much brighter the chosen tower is
constexpr float kRecoilSquash = 0.08F; // of its height, how far a tower gives way when it fires

constexpr float kHopHeight = 0.08F; // of a field, how far a walking enemy leaves the ground
constexpr float kHopsPerField = 2.5F;
constexpr qint64 kStrideTenths = 9; // of a field, the way of one walk cycle
constexpr int kAheadRaw = 1000;     // how far along the path an enemy looks to see where it goes

struct Piece
{
    QVector3D position;
    QVector3D scale;
    QColor color;
    QVector3D angles = {};
};

/// How high the block of a cell is. Nobody sees it, the ground shows the terrain.
float blockHeightOf(core::Terrain terrain)
{
    switch (terrain) {
    case core::Terrain::Open:
    case core::Terrain::Path:
    case core::Terrain::Water:
        break;
    case core::Terrain::Rock:
        return kRockHeight;
    case core::Terrain::Tree:
        return kTreeHeight;
    }
    return kFlatHeight;
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
            pieces.push_back(blockOn(QPoint(static_cast<int>(x), static_cast<int>(y)),
                                     fieldSize,
                                     kBoardShare,
                                     Qt::transparent,
                                     blockHeightOf(grid->terrainAt({x, y}))));
        }
    }
    return pieces;
}

/// What stands on the cells of one terrain.
std::vector<Piece>
decorPieces(const GameSession &game, float fieldSize, float modelSize, core::Terrain terrain)
{
    std::vector<Piece> pieces;
    const core::Grid *grid = game.grid();
    if (grid == nullptr) {
        return pieces;
    }
    for (qint64 y = 0; y < grid->height(); ++y) {
        for (qint64 x = 0; x < grid->width(); ++x) {
            if (grid->terrainAt({x, y}) == terrain) {
                pieces.push_back({QVector3D(static_cast<float>(x) * fieldSize,
                                            0,
                                            static_cast<float>(y) * fieldSize),
                                  QVector3D(1, 1, 1) * fieldSize * modelSize,
                                  Qt::white});
            }
        }
    }
    return pieces;
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
    return poisoned ? QColor(0x4fa84a) : QColor(Qt::white);
}

/// The way along the path, around the y axis.
float headingOf(const core::Simulation &game, const core::Enemy &enemy)
{
    core::Enemy ahead = enemy;
    ahead.travelled = enemy.travelled + core::Fixed::fromRaw(kAheadRaw);
    const core::Point from = game.position(enemy);
    const core::Point to = game.position(ahead);
    return qRadiansToDegrees(std::atan2(static_cast<float>(to.x.raw() - from.x.raw()),
                                        static_cast<float>(to.y.raw() - from.y.raw())));
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
        std::ranges::transform(core::footprintAt(type, tower.cell, tower.rotation),
                               std::back_inserter(pieces),
                               [&](core::Cell cell) {
                                   return blockOn(
                                       QPoint(static_cast<int>(cell.x), static_cast<int>(cell.y)),
                                       fieldSize,
                                       kTowerShare,
                                       Qt::transparent,
                                       kTowerHeight +
                                           kTowerHeightPerLevel * static_cast<float>(tower.level));
                               });
    }
    return pieces;
}

/// The towers of one type: a model on each cell, higher with each level, the chosen one bright.
std::vector<Piece>
towerLooks(const GameSession &game, float fieldSize, float modelSize, int typeIndex)
{
    std::vector<Piece> pieces;
    const core::Simulation *simulation = game.simulation();
    if (simulation == nullptr) {
        return pieces;
    }
    const float width = fieldSize * kTowerShare / modelSize;
    for (const core::Tower &tower : simulation->state().towers) {
        if (std::cmp_not_equal(tower.type, typeIndex)) {
            continue;
        }
        const core::TowerType &type = simulation->content().towers[tower.type];
        const QColor color = tower.id == game.chosenTower()
                                 ? QColor(Qt::white)
                                 : QColor(Qt::white).darker(kChosenLight);
        // A tower that just fired gives way a little and swells, then stands up again (E-52).
        const float kick = recoilOf(game, tower);
        const float height = width * (1 + kTowerHeightPerLevel * static_cast<float>(tower.level)) *
                             (1 - kRecoilSquash * kick);
        const float girth = width * (1 + kRecoilSquash / 2 * kick);
        std::ranges::transform(core::footprintAt(type, tower.cell, tower.rotation),
                               std::back_inserter(pieces),
                               [&](core::Cell cell) {
                                   return Piece{QVector3D(static_cast<float>(cell.x) * fieldSize,
                                                          kFlatHeight * fieldSize,
                                                          static_cast<float>(cell.y) * fieldSize),
                                                QVector3D(girth, height, girth),
                                                color};
                               });
    }
    return pieces;
}

/// The walk frame of an enemy follows its way, so that a slowed enemy walks slower. A boss is
/// larger and takes longer steps.
int walkFrameOf(const core::Enemy &enemy, bool boss)
{
    qint64 stride = core::Fixed::fromInt(1).raw() * kStrideTenths / 10;
    if (boss) {
        stride = static_cast<qint64>(static_cast<float>(stride) * kBossScale);
    }
    return static_cast<int>((enemy.travelled.raw() * kWalkFrames / stride + enemy.id * 3) %
                            kWalkFrames);
}

std::vector<Piece> enemyPieces(
    const GameSession &game, float fieldSize, float enemyScale, int typeIndex, int walkFrame)
{
    std::vector<Piece> pieces;
    const std::vector<QPointF> places = game.enemyPlaces();
    if (places.empty()) {
        return pieces;
    }
    const core::Simulation &simulation = *game.simulation();
    const std::vector<core::Enemy> &enemies = simulation.state().enemies;
    const std::vector<core::EnemyType> &types = simulation.content().enemies;
    for (std::size_t index = 0; index < places.size(); ++index) {
        const core::Enemy &enemy = enemies[index];
        if (std::cmp_not_equal(enemy.type, typeIndex) ||
            (walkFrame >= 0 && walkFrameOf(enemy, types[enemy.type].boss) != walkFrame)) {
            continue;
        }
        const auto x = static_cast<float>(places[index].x());
        const auto z = static_cast<float>(places[index].y());
        const float hop =
            walkFrame >= 0 ? 0.0F
                           : std::abs(std::sin((x + z) * kHopsPerField)) * kHopHeight * fieldSize;
        pieces.push_back(
            {QVector3D(x * fieldSize, hop, z * fieldSize),
             QVector3D(1, 1, 1) * enemyScale * (types[enemy.type].boss ? kBossScale : 1.0F),
             colorOf(enemy),
             QVector3D(0, headingOf(simulation, enemy), 0)});
    }
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

float MapInstances::modelSize() const
{
    return modelSize_;
}

float MapInstances::enemyScale() const
{
    return enemyScale_;
}

int MapInstances::typeIndex() const
{
    return typeIndex_;
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
        connect(game_, &GameSession::mapChanged, this, [this] {
            markDirtyFor({Layer::Board, Layer::Rocks, Layer::Trees});
        });
        connect(game_, &GameSession::towersChanged, this, [this] {
            markDirtyFor({Layer::Towers, Layer::TowerLooks, Layer::Shots});
        });
        connect(game_, &GameSession::stateChanged, this, [this] {
            markDirtyFor({Layer::Enemies, Layer::Shots});
        });
        connect(game_, &GameSession::frameChanged, this, [this] {
            markDirtyFor({Layer::Enemies, Layer::Shots, Layer::TowerLooks});
        });
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

void MapInstances::setModelSize(float size)
{
    if (!qFuzzyCompare(modelSize_, size)) {
        modelSize_ = size;
        markDirtyFor({Layer::TowerLooks, Layer::Rocks, Layer::Trees});
        emit modelSizeChanged();
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

void MapInstances::setTypeIndex(int index)
{
    if (typeIndex_ != index) {
        typeIndex_ = index;
        markDirty();
        emit typeIndexChanged();
    }
}

int MapInstances::walkFrame() const
{
    return walkFrame_;
}

void MapInstances::setWalkFrame(int frame)
{
    if (walkFrame_ != frame) {
        walkFrame_ = frame;
        markDirty();
        emit walkFrameChanged();
    }
}

MapInstances::Shot MapInstances::shot() const
{
    return shot_;
}

void MapInstances::setShot(Shot shot)
{
    if (shot_ != shot) {
        shot_ = shot;
        markDirty();
        emit shotChanged();
    }
}

void MapInstances::markDirtyFor(std::initializer_list<Layer> layers)
{
    if (std::ranges::find(layers, layer_) != layers.end()) {
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
        case Layer::TowerLooks:
            pieces = towerLooks(*game_, fieldSize_, modelSize_, typeIndex_);
            break;
        case Layer::Enemies:
            pieces = enemyPieces(*game_, fieldSize_, enemyScale_, typeIndex_, walkFrame_);
            break;
        case Layer::Shots:
            std::ranges::transform(shotPieces(*game_, fieldSize_, shot_),
                                   std::back_inserter(pieces),
                                   [](const ShotPiece &shot) {
                                       return Piece{
                                           shot.position, shot.scale, shot.color, shot.angles};
                                   });
            break;
        case Layer::Rocks:
            pieces = decorPieces(*game_, fieldSize_, modelSize_, core::Terrain::Rock);
            break;
        case Layer::Trees:
            pieces = decorPieces(*game_, fieldSize_, modelSize_, core::Terrain::Tree);
            break;
        }
    }
    constexpr qsizetype entrySize = sizeof(InstanceTableEntry);
    QByteArray buffer(static_cast<qsizetype>(pieces.size()) * entrySize, Qt::Uninitialized);
    char *next = buffer.data();
    for (const Piece &piece : pieces) {
        const InstanceTableEntry entry = calculateTableEntry(
            piece.position, piece.scale, piece.angles, piece.color, QVector4D());
        std::memcpy(next, &entry, entrySize);
        next += entrySize;
    }
    if (instanceCount != nullptr) {
        *instanceCount = static_cast<int>(pieces.size());
    }
    return buffer;
}

} // namespace qtd::client
