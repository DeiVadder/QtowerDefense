#pragma once

#include "client/map_instances.h"

#include <QColor>
#include <QVector3D>
#include <QtGlobal>

#include <vector>

namespace qtd::core {
struct Tower;
struct TowerType;
} // namespace qtd::core

namespace qtd::client {

/// How much higher a tower looks with each level above the first, of its height.
inline constexpr float kTowerHeightPerLevel = 0.25F;

/// The shapes a shot takes in flight, see MapInstances::Shot (E-52).
using ShotShape = MapInstances::Shot;

/// How the shot of one tower type looks. A picture only: the simulation hits at once.
struct ShotLook
{
    ShotShape shape = ShotShape::Bolt;
    QColor color;
    qint64 ticks = 4;   // the time of flight
    int count = 1;      // shots side by side, or a volley
    float spread = 0;   // fields between two shots of one volley, across the way
    qint64 stagger = 0; // ticks between two shots of one volley
    float arc = 0;      // fields, how high the way rises above the straight line
    QVector3D size;     // fields: width, length along the way, depth across it
    float spin = 0;     // degrees a tick
    float launch = 1;   // fields above the ground where it leaves a tower of the first level
};

/// The look of the shots of a tower type: its own for the towers of the catalog, else one for its
/// damage type.
[[nodiscard]] ShotLook shotLookOf(const core::TowerType &type);

/// One shot in the picture, in the units of the map.
struct ShotPiece
{
    QVector3D position;
    QVector3D scale;
    QVector3D angles;
    QColor color;
};

/// The shots of one shape in flight, `fieldSize` units to a field. A shot flies to the nearest
/// enemy, which most of the time is the target; the simulation does not tell the target.
[[nodiscard]] std::vector<ShotPiece>
shotPieces(const GameSession &game, float fieldSize, ShotShape shape);

/// How much of the kick of a shot is left in a tower, from 1 just after it fired down to 0.
[[nodiscard]] float recoilOf(const GameSession &game, const core::Tower &tower);

} // namespace qtd::client
