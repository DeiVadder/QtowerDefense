#include "automation/invariants.h"

#include <cstddef>
#include <set>

using namespace Qt::StringLiterals;

namespace qtd::automation {

namespace {

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
    }
    return {};
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
