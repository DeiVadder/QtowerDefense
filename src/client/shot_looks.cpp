#include "client/shot_looks.h"

#include "client/game_session.h"
#include "core/content.h"
#include "core/simulation.h"

#include <QPointF>
#include <QQuaternion>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>

using namespace Qt::StringLiterals;

namespace qtd::client {

namespace {

constexpr float kBuiltInSize = 100; // the edge of the built-in shapes of Qt Quick 3D
constexpr float kHitHeight = 0.35F; // fields, where it meets an enemy
constexpr qint64 kDustTicks = 6;
constexpr float kDustHeight = 0.03F;
constexpr float kRecoilTicks = 3;
constexpr float kHeightPerLevel = 0.25F; // kTowerHeightPerLevel in map_instances.cpp
constexpr float kPulse = 0.15F;          // of its size, how much a fireball pulses
constexpr float kWobble = 0.2F;          // of its size, how much a glob wobbles
constexpr float kPulseRate = 2.5F;       // radians a tick
constexpr float kWobbleRate = 3.0F;

ShotLook lookOfDamage(core::DamageType damage)
{
    switch (damage) {
    case core::DamageType::Physical:
        break;
    case core::DamageType::Magic:
        return {ShotShape::Glob, QColor(0x9a6bff), 8, 1, 0, 0, 0.5F, {0.28F, 0.28F, 0.28F}, 0};
    case core::DamageType::Fire:
        return {ShotShape::Fireball, QColor(0xff7a2a), 7, 1, 0, 0, 0.35F, {0.38F, 0.38F, 0.38F}, 0};
    case core::DamageType::Ice:
        return {ShotShape::Shard, QColor(0xa8e6ff), 5, 1, 0, 0, 0.1F, {0.15F, 0.5F, 0.15F}, 60};
    }
    return {ShotShape::Bolt, QColor(0xc98a3c), 4, 1, 0, 0, 0.05F, {0.09F, 0.6F, 0.09F}, 0};
}

/// The time of the picture in ticks since the tower fired, or nothing when it did not just fire.
std::optional<float> sinceFired(const GameSession &game, const core::Tower &tower)
{
    const core::TowerType &type = game.simulation()->content().towers[tower.type];
    const qint64 fired =
        type.levels[static_cast<std::size_t>(tower.level)].cooldownTicks - tower.cooldownTicks;
    // An upgrade may shorten the cooldown below what is left of it.
    if (tower.cooldownTicks == 0 || fired < 0) {
        return std::nullopt;
    }
    // The picture is up to a tick behind the game, like the enemies (GameSession::enemyPlaces).
    return std::max(0.0F, static_cast<float>(fired) - 1 + static_cast<float>(game.tickShare()));
}

/// Turns the y axis of a shape, along which rods and cones lie, into the given direction.
QVector3D anglesAlong(const QVector3D &direction, float spin)
{
    const QQuaternion turn = QQuaternion::rotationTo(QVector3D(0, 1, 0), direction.normalized()) *
                             QQuaternion::fromAxisAndAngle(QVector3D(0, 1, 0), spin);
    return turn.toEulerAngles();
}

struct Flight
{
    QVector3D from;
    QVector3D to;
};

/// The volley of one tower in flight, at `time` ticks since it fired.
void addVolley(std::vector<ShotPiece> &pieces,
               const ShotLook &look,
               const Flight &flight,
               float time,
               float fieldSize)
{
    const QVector3D up(0, 1, 0);
    const QVector3D ground =
        QVector3D(flight.to.x() - flight.from.x(), 0, flight.to.z() - flight.from.z());
    const QVector3D side =
        ground.isNull() ? QVector3D(1, 0, 0) : QVector3D::crossProduct(ground, up).normalized();
    const float unit = fieldSize / kBuiltInSize;
    for (int index = 0; index < look.count; ++index) {
        const float own = time - static_cast<float>(index * look.stagger);
        if (own < 0 || own >= static_cast<float>(look.ticks)) {
            continue;
        }
        const float share = own / static_cast<float>(look.ticks);
        const float across = (static_cast<float>(index) - static_cast<float>(look.count - 1) / 2) *
                             look.spread * fieldSize;
        const float rise = look.arc * fieldSize;
        const QVector3D position = flight.from + (flight.to - flight.from) * share +
                                   up * (4 * rise * share * (1 - share)) + side * across;
        const QVector3D heading = flight.to - flight.from + up * (4 * rise * (1 - 2 * share));
        QVector3D scale(look.size.x(), look.size.y(), look.size.z());
        QVector3D angles = anglesAlong(heading, own * look.spin);
        switch (look.shape) {
        case ShotShape::Fireball:
            scale *= 1 + kPulse * std::sin(own * kPulseRate);
            angles = {};
            break;
        case ShotShape::Glob: {
            const float wobble = kWobble * std::sin(own * kWobbleRate);
            scale *= QVector3D(1 - wobble / 2, 1 + wobble, 1 - wobble / 2);
            angles = {};
            break;
        }
        case ShotShape::Boulder:
            angles = QVector3D(own * look.spin, own * look.spin * 0.7F, 0);
            break;
        case ShotShape::Bolt:
        case ShotShape::Arrow:
        case ShotShape::Shard:
        case ShotShape::Dust:
            break;
        }
        pieces.push_back({position, scale * unit, angles, look.color});
    }
}

/// The ring of dust where a boulder lands, growing to the splash radius as it fades.
void addDust(std::vector<ShotPiece> &pieces,
             const ShotLook &look,
             const core::TowerLevel &level,
             const QVector3D &where,
             float time,
             float fieldSize)
{
    const float after = time - static_cast<float>(look.ticks);
    if (after < 0 || after >= static_cast<float>(kDustTicks)) {
        return;
    }
    const float share = after / static_cast<float>(kDustTicks);
    const float radius = std::max(0.5F,
                                  static_cast<float>(level.splashRadius.raw()) /
                                      static_cast<float>(core::Fixed::kScale));
    const float width = 2 * radius * (0.4F + 0.6F * share) * fieldSize / kBuiltInSize;
    QColor color(0xc8b48a);
    color.setAlphaF(0.7F * (1 - share));
    pieces.push_back({QVector3D(where.x(), kDustHeight * fieldSize, where.z()),
                      QVector3D(width, kDustHeight * fieldSize / kBuiltInSize, width),
                      {},
                      color});
}

/// The look without the height it leaves from.
ShotLook flightOf(const core::TowerType &type)
{
    if (type.id == u"ballista"_s) {
        return lookOfDamage(core::DamageType::Physical);
    }
    if (type.id == u"twin_ballista"_s) {
        return {ShotShape::Bolt, QColor(0xb08cff), 4, 2, 0.18F, 0, 0.05F, {0.09F, 0.6F, 0.09F}, 0};
    }
    if (type.id == u"horseshoe"_s) {
        return {ShotShape::Arrow, QColor(0xd8c08a), 6, 3, 0.25F, 1, 0.6F, {0.05F, 0.42F, 0.05F}, 0};
    }
    if (type.id == u"venom_spire"_s) {
        return {ShotShape::Glob, QColor(0x5fd14a), 9, 1, 0, 0, 0.7F, {0.28F, 0.28F, 0.28F}, 0};
    }
    if (type.id == u"catapult"_s) {
        return {ShotShape::Boulder, QColor(0x8b8680), 14, 1, 0, 0, 2.2F, {0.42F, 0.36F, 0.42F}, 40};
    }
    return lookOfDamage(type.damageType); // bastion, frost_spire and any tower without its own
}

/// Fields above the ground where a shot leaves a tower of the first level: at its battlements,
/// under the roof. The models of the catalog are 0.9 fields wide and 0.9 to 1.7 fields high.
float launchOf(const core::TowerType &type)
{
    if (type.id == u"ballista"_s || type.id == u"venom_spire"_s) {
        return 1.35F;
    }
    if (type.id == u"twin_ballista"_s || type.id == u"bastion"_s) {
        return 1.25F;
    }
    if (type.id == u"catapult"_s) {
        return 1.15F;
    }
    if (type.id == u"frost_spire"_s) {
        return 1.05F;
    }
    if (type.id == u"horseshoe"_s) {
        return 0.75F;
    }
    return 1;
}

} // namespace

ShotLook shotLookOf(const core::TowerType &type)
{
    ShotLook look = flightOf(type);
    look.launch = launchOf(type);
    return look;
}

std::vector<ShotPiece> shotPieces(const GameSession &game, float fieldSize, ShotShape shape)
{
    std::vector<ShotPiece> pieces;
    const std::vector<QPointF> places = game.enemyPlaces();
    if (places.empty()) {
        return pieces;
    }
    const core::Simulation &simulation = *game.simulation();
    for (const core::Tower &tower : simulation.state().towers) {
        const core::TowerType &type = simulation.content().towers[tower.type];
        const ShotLook look = shotLookOf(type);
        const bool dust = look.shape == ShotShape::Boulder && shape == ShotShape::Dust;
        if (look.shape != shape && !dust) {
            continue;
        }
        const std::optional<float> time = sinceFired(game, tower);
        if (!time) {
            continue;
        }
        const QPointF from(static_cast<double>(tower.cell.x), static_cast<double>(tower.cell.y));
        const QPointF target = *std::ranges::min_element(places, {}, [from](QPointF place) {
            const QPointF away = place - from;
            return QPointF::dotProduct(away, away);
        });
        const float launch = look.launch * (1 + kHeightPerLevel * static_cast<float>(tower.level));
        const Flight flight{
            QVector3D(static_cast<float>(from.x()), launch, static_cast<float>(from.y())) *
                fieldSize,
            QVector3D(static_cast<float>(target.x()), kHitHeight, static_cast<float>(target.y())) *
                fieldSize};
        if (dust) {
            addDust(pieces,
                    look,
                    type.levels[static_cast<std::size_t>(tower.level)],
                    flight.to,
                    *time,
                    fieldSize);
        } else {
            addVolley(pieces, look, flight, *time, fieldSize);
        }
    }
    return pieces;
}

float recoilOf(const GameSession &game, const core::Tower &tower)
{
    const std::optional<float> time = sinceFired(game, tower);
    if (!time || *time >= kRecoilTicks) {
        return 0;
    }
    return 1 - *time / kRecoilTicks;
}

} // namespace qtd::client
