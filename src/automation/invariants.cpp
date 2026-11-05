#include "automation/invariants.h"

#include <cstddef>
#include <numeric>
#include <set>
#include <vector>

using namespace Qt::StringLiterals;

namespace qtd::automation {

namespace {

/// Running effects have a strength, others have none.
bool isSound(const core::ActiveStatus &status)
{
    return status.ticks > 0 ? status.strength > core::Fixed()
                            : status.ticks == 0 && status.strength == core::Fixed();
}

QString checkEnemies(const core::Content &content, const core::SimulationState &state)
{
    qint64 lastId = 0;
    for (const core::Enemy &enemy : state.enemies) {
        if (enemy.id <= lastId || enemy.id >= state.nextEntityId) {
            return u"enemy %1: its id is out of order"_s.arg(enemy.id);
        }
        lastId = enemy.id;
        if (enemy.type >= content.enemies.size()) {
            return u"enemy %1: unknown type"_s.arg(enemy.id);
        }
        if (enemy.travelled < core::Fixed() || enemy.travelled >= content.path.length()) {
            return u"enemy %1: not on the path"_s.arg(enemy.id);
        }
        if (enemy.health <= core::Fixed()) {
            return u"enemy %1: dead but still in the game"_s.arg(enemy.id);
        }
        if (enemy.health > content.enemies[enemy.type].health) {
            return u"enemy %1: healed beyond its health"_s.arg(enemy.id);
        }
        if (!isSound(enemy.slow) || !isSound(enemy.poison) ||
            enemy.slow.strength > core::maxSlow()) {
            return u"enemy %1: impossible status effect"_s.arg(enemy.id);
        }
    }
    return {};
}

QString checkLevel(const core::Content &content, const core::Tower &tower)
{
    const std::vector<core::TowerLevel> &levels = content.towers[tower.type].levels;
    if (tower.level >= levels.size()) {
        return u"tower %1: unknown level"_s.arg(tower.id);
    }
    const qint64 paid =
        std::accumulate(levels.begin(),
                        levels.begin() + static_cast<std::ptrdiff_t>(tower.level) + 1,
                        qint64{0},
                        [](qint64 sum, const core::TowerLevel &level) { return sum + level.cost; });
    return tower.spent == paid ? QString() : u"tower %1: its gold does not add up"_s.arg(tower.id);
}

QString checkTowers(const core::Content &content, const core::SimulationState &state)
{
    qint64 lastId = 0;
    std::set<core::Cell> taken;
    std::set<qint64> enemyIds;
    for (const core::Enemy &enemy : state.enemies) {
        enemyIds.insert(enemy.id);
    }
    for (const core::Tower &tower : state.towers) {
        if (tower.id <= lastId || tower.id >= state.nextEntityId) {
            return u"tower %1: its id is out of order"_s.arg(tower.id);
        }
        lastId = tower.id;
        if (enemyIds.contains(tower.id)) {
            return u"tower %1: an enemy has its id"_s.arg(tower.id);
        }
        if (tower.type >= content.towers.size()) {
            return u"tower %1: unknown type"_s.arg(tower.id);
        }
        for (const core::Cell cell :
             core::footprintAt(content.towers[tower.type], tower.cell, tower.rotation)) {
            if (!content.grid.contains(cell)) {
                return u"tower %1: outside the map"_s.arg(tower.id);
            }
            if (content.grid.terrainAt(cell) != core::Terrain::Open) {
                return u"tower %1: on a blocked cell"_s.arg(tower.id);
            }
            if (!taken.insert(cell).second) {
                return u"tower %1: its cell is taken"_s.arg(tower.id);
            }
        }
        if (tower.cooldownTicks < 0) {
            return u"tower %1: negative cooldown"_s.arg(tower.id);
        }
        const QString level = checkLevel(content, tower);
        if (!level.isEmpty()) {
            return level;
        }
    }
    return {};
}

} // namespace

QString checkInvariants(const core::Content &content, const core::SimulationState &state)
{
    if (state.gold < 0) {
        return u"gold is negative: %1"_s.arg(state.gold);
    }
    if (state.lives < 0) {
        return u"lives are negative: %1"_s.arg(state.lives);
    }
    const QString enemies = checkEnemies(content, state);
    return enemies.isEmpty() ? checkTowers(content, state) : enemies;
}

} // namespace qtd::automation
