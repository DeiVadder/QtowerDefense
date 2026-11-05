#include "core/state_hash.h"

#include <array>
#include <numeric>

namespace qtd::core {

namespace {

constexpr quint64 kPrime = 1'099'511'628'211ULL;

/// Adds the fields of a hash in the given order. Signed numbers count as their two's complement.
class Fields
{
public:
    template <typename... Numbers>
    void add(Numbers... numbers)
    {
        (hash_.addNumber(static_cast<quint64>(numbers)), ...);
    }

    void addPoints(const std::vector<Point> &points)
    {
        add(points.size());
        for (const Point point : points) {
            add(point.x.raw(), point.y.raw());
        }
    }

    quint64 value() const
    {
        return hash_.value();
    }

private:
    Fnv1a64 hash_;
};

} // namespace

void Fnv1a64::addBytes(QByteArrayView bytes)
{
    value_ = std::accumulate(bytes.begin(), bytes.end(), value_, [](quint64 hash, char byte) {
        return (hash ^ static_cast<quint8>(byte)) * kPrime;
    });
}

void Fnv1a64::addNumber(quint64 number)
{
    std::array<char, sizeof(quint64)> bytes{};
    for (char &byte : bytes) {
        byte = static_cast<char>(number & 0xFFU);
        number >>= 8U;
    }
    addBytes(QByteArrayView(bytes.data(), bytes.size()));
}

quint64 contentHash(const Content &content)
{
    Fields fields;
    fields.add(content.startGold, content.startLives);
    fields.addPoints(content.path.points());
    fields.add(content.grid.width(), content.grid.height());
    for (const Terrain terrain : content.grid.cells()) {
        fields.add(terrain);
    }
    fields.add(content.towers.size());
    for (const TowerType &tower : content.towers) {
        fields.add(tower.cost,
                   tower.cooldownTicks,
                   tower.range.raw(),
                   tower.damageMin.raw(),
                   tower.damageMax.raw(),
                   tower.damageType);
        fields.add(tower.effects.size());
        for (const StatusEffect &effect : tower.effects) {
            fields.add(effect.kind, effect.strength.raw(), effect.durationTicks);
        }
        fields.add(tower.footprint.size());
        for (const Cell cell : tower.footprint) {
            fields.add(cell.x, cell.y);
        }
    }
    fields.add(content.enemies.size());
    for (const EnemyType &enemy : content.enemies) {
        fields.add(enemy.health.raw(), enemy.speedPerTick.raw(), enemy.bounty, enemy.lifeCost);
        for (const Fixed resistance : enemy.resistances) {
            fields.add(resistance.raw());
        }
    }
    fields.add(content.wave.size());
    for (const SpawnGroup &group : content.wave) {
        fields.add(group.enemyType, group.count, group.delayTicks, group.intervalTicks);
    }
    return fields.value();
}

quint64 stateHash(const SimulationState &state)
{
    Fields fields;
    fields.add(state.contentHash, state.tick, state.status, state.gold, state.lives);
    fields.add(state.random.state(), state.random.increment());
    fields.add(
        state.nextEntityId, state.spawnGroup, state.spawnedInGroup, state.spawnCountdownTicks);
    fields.add(state.enemies.size());
    for (const Enemy &enemy : state.enemies) {
        fields.add(enemy.id, enemy.type, enemy.health.raw(), enemy.travelled.raw());
        fields.add(enemy.slow.ticks, enemy.slow.strength.raw());
        fields.add(enemy.poison.ticks, enemy.poison.strength.raw());
    }
    fields.add(state.towers.size());
    for (const Tower &tower : state.towers) {
        fields.add(
            tower.id, tower.type, tower.cell.x, tower.cell.y, tower.cooldownTicks, tower.rotation);
        fields.add(tower.priority);
    }
    return fields.value();
}

} // namespace qtd::core
