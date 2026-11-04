#include "core/simulation.h"

#include "core/state_hash.h"

#include <algorithm>
#include <iterator>
#include <utility>

namespace qtd::core {

namespace {

/// All games share one stream of the generator, the seed alone tells them apart.
constexpr quint64 kRandomStream = 54;

} // namespace

Simulation::Simulation(Content content, quint64 seed) : content_(std::move(content))
{
    state_.contentHash = contentHash(content_);
    state_.gold = content_.startGold;
    state_.lives = content_.startLives;
    state_.random = Random(seed, kRandomStream);
    state_.spawnCountdownTicks = content_.wave.front().delayTicks;
}

CommandResult Simulation::execute(const Command &command)
{
    return std::visit([this](const auto &concrete) { return build(concrete); }, command);
}

CommandResult Simulation::build(const BuildTowerCommand &command)
{
    if (state_.status != GameStatus::Running) {
        return CommandResult::GameOver;
    }
    if (command.slot < 0 || std::cmp_greater_equal(command.slot, content_.buildSlots.size())) {
        return CommandResult::UnknownSlot;
    }
    if (command.towerType < 0 ||
        std::cmp_greater_equal(command.towerType, content_.towers.size())) {
        return CommandResult::UnknownTowerType;
    }
    const auto slot = static_cast<std::size_t>(command.slot);
    if (std::ranges::find(state_.towers, slot, &Tower::slot) != state_.towers.end()) {
        return CommandResult::SlotOccupied;
    }
    const auto type = static_cast<std::size_t>(command.towerType);
    const qint64 cost = content_.towers[type].cost;
    if (state_.gold < cost) {
        return CommandResult::NotEnoughGold;
    }
    state_.gold -= cost;
    state_.towers.push_back({state_.nextEntityId++, type, slot, 0});
    return CommandResult::Accepted;
}

void Simulation::tick()
{
    if (state_.status != GameStatus::Running) {
        return;
    }
    ++state_.tick;
    moveEnemies();
    spawnEnemies();
    fireTowers();
    updateStatus();
}

void Simulation::advance(qint64 ticks)
{
    for (qint64 done = 0; done < ticks && state_.status == GameStatus::Running; ++done) {
        tick();
    }
}

Point Simulation::position(const Enemy &enemy) const
{
    return content_.path.positionAt(enemy.travelled);
}

quint64 Simulation::hash() const
{
    return stateHash(state_);
}

void Simulation::moveEnemies()
{
    const Fixed end = content_.path.length();
    for (Enemy &enemy : state_.enemies) {
        const EnemyType &type = content_.enemies[enemy.type];
        enemy.travelled = enemy.travelled + type.speedPerTick;
        if (enemy.travelled >= end) {
            state_.lives -= type.lifeCost;
        }
    }
    std::erase_if(state_.enemies, [end](const Enemy &enemy) { return enemy.travelled >= end; });
}

void Simulation::spawnEnemies()
{
    if (state_.spawnGroup >= content_.wave.size()) {
        return;
    }
    if (state_.spawnCountdownTicks > 0) {
        --state_.spawnCountdownTicks;
        return;
    }
    const SpawnGroup &group = content_.wave[state_.spawnGroup];
    state_.enemies.push_back({state_.nextEntityId++,
                              group.enemyType,
                              content_.enemies[group.enemyType].health,
                              Fixed()});
    ++state_.spawnedInGroup;
    if (state_.spawnedInGroup < group.count) {
        state_.spawnCountdownTicks = group.intervalTicks - 1;
        return;
    }
    ++state_.spawnGroup;
    state_.spawnedInGroup = 0;
    if (state_.spawnGroup < content_.wave.size()) {
        state_.spawnCountdownTicks = content_.wave[state_.spawnGroup].delayTicks;
    }
}

Enemy *Simulation::findTarget(const Tower &tower, const std::vector<Point> &positions)
{
    const Fixed range = content_.towers[tower.type].range;
    const Fixed reach = range * range;
    const Point place = content_.buildSlots[tower.slot];
    Enemy *target = nullptr;
    for (std::size_t index = 0; index < state_.enemies.size(); ++index) {
        Enemy &enemy = state_.enemies[index];
        // The strict comparison keeps the lowest id among enemies at the same place.
        if (enemy.health > Fixed() && distanceSquared(place, positions[index]) <= reach &&
            (target == nullptr || enemy.travelled > target->travelled)) {
            target = &enemy;
        }
    }
    return target;
}

Fixed Simulation::rollDamage(const TowerType &type)
{
    const qint64 spread = type.damageMax.raw() - type.damageMin.raw();
    const quint32 roll = state_.random.nextBelow(static_cast<quint32>(spread + 1));
    return type.damageMin + Fixed::fromRaw(roll);
}

void Simulation::fireTowers()
{
    std::vector<Point> positions;
    positions.reserve(state_.enemies.size());
    std::ranges::transform(state_.enemies,
                           std::back_inserter(positions),
                           [this](const Enemy &enemy) { return position(enemy); });
    for (Tower &tower : state_.towers) {
        if (tower.cooldownTicks > 0) {
            --tower.cooldownTicks;
        }
        Enemy *target = tower.cooldownTicks == 0 ? findTarget(tower, positions) : nullptr;
        if (target == nullptr) {
            continue;
        }
        const TowerType &type = content_.towers[tower.type];
        target->health = target->health - rollDamage(type);
        tower.cooldownTicks = type.cooldownTicks;
        if (target->health <= Fixed()) {
            state_.gold += content_.enemies[target->type].bounty;
        }
    }
    std::erase_if(state_.enemies, [](const Enemy &enemy) { return enemy.health <= Fixed(); });
}

void Simulation::updateStatus()
{
    if (state_.lives <= 0) {
        state_.lives = 0;
        state_.status = GameStatus::Lost;
    } else if (state_.spawnGroup >= content_.wave.size() && state_.enemies.empty()) {
        state_.status = GameStatus::Won;
    }
}

} // namespace qtd::core
