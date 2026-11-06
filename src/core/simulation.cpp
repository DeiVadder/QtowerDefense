#include "core/simulation.h"

#include "core/state_hash.h"

#include <algorithm>
#include <iterator>
#include <utility>

namespace qtd::core {

namespace {

/// All games share one stream of the generator, the seed alone tells them apart.
constexpr quint64 kRandomStream = 54;
constexpr qint64 kPercent = 100;

/// One tick less of an effect.
void countDown(ActiveStatus *status)
{
    --status->ticks;
    if (status->ticks == 0) {
        status->strength = Fixed();
    }
}

/// The same effect does not add up: the stronger one stays, and of equal ones the longer. Nothing
/// is left of a hit that an enemy is immune to.
void afflict(ActiveStatus *status, Fixed strength, qint64 ticks)
{
    if (strength > status->strength) {
        *status = {ticks, strength};
    } else if (strength == status->strength && strength > Fixed()) {
        status->ticks = std::max(status->ticks, ticks);
    }
}

/// Higher is better. The distance may be squared, that keeps the order.
Fixed targetScore(TargetPriority priority, const Enemy &enemy, Fixed distance)
{
    switch (priority) {
    case TargetPriority::First:
        break;
    case TargetPriority::Last:
        return -enemy.travelled;
    case TargetPriority::Strongest:
        return enemy.health;
    case TargetPriority::Nearest:
        return -distance;
    }
    return enemy.travelled;
}

} // namespace

Simulation::Simulation(Content content, quint64 seed) : content_(std::move(content))
{
    state_.contentHash = contentHash(content_);
    state_.gold = content_.startGold;
    state_.lives = content_.startLives;
    state_.random = Random(seed, kRandomStream);
    state_.spawnCountdownTicks = content_.wave.front().delayTicks;
    state_.spellCooldowns.assign(content_.spells.size(), 0);
}

CommandResult Simulation::execute(const Command &command)
{
    return std::visit([this](const auto &concrete) { return run(concrete); }, command);
}

CommandResult Simulation::check(const BuildTowerCommand &command) const
{
    if (state_.status != GameStatus::Running) {
        return CommandResult::GameOver;
    }
    if (command.towerType < 0 ||
        std::cmp_greater_equal(command.towerType, content_.towers.size())) {
        return CommandResult::UnknownTowerType;
    }
    const TowerType &type = content_.towers[static_cast<std::size_t>(command.towerType)];
    const std::vector<Cell> cells = footprintAt(type, command.cell, command.rotation);
    if (!std::ranges::all_of(cells, [this](Cell cell) { return content_.grid.contains(cell); })) {
        return CommandResult::OutsideMap;
    }
    if (!std::ranges::all_of(
            cells, [this](Cell cell) { return content_.grid.terrainAt(cell) == Terrain::Open; })) {
        return CommandResult::CellBlocked;
    }
    if (std::ranges::any_of(cells, [this](Cell cell) { return hasTowerAt(cell); })) {
        return CommandResult::CellOccupied;
    }
    return state_.gold < type.levels.front().cost ? CommandResult::NotEnoughGold
                                                  : CommandResult::Accepted;
}

const Tower *Simulation::towerWithId(qint64 id) const
{
    const std::vector<Tower>::const_iterator found =
        std::ranges::find(state_.towers, id, &Tower::id);
    return found != state_.towers.end() ? &*found : nullptr;
}

CommandResult Simulation::checkTowerCommand(qint64 id) const
{
    if (state_.status != GameStatus::Running) {
        return CommandResult::GameOver;
    }
    return towerWithId(id) != nullptr ? CommandResult::Accepted : CommandResult::UnknownTower;
}

CommandResult Simulation::check(const SetTargetPriorityCommand &command) const
{
    return checkTowerCommand(command.tower);
}

CommandResult Simulation::check(const UpgradeTowerCommand &command) const
{
    const CommandResult result = checkTowerCommand(command.tower);
    if (result != CommandResult::Accepted) {
        return result;
    }
    const TowerLevel *next = nextLevelOf(*towerWithId(command.tower));
    if (next == nullptr) {
        return CommandResult::HighestLevel;
    }
    return state_.gold < next->cost ? CommandResult::NotEnoughGold : CommandResult::Accepted;
}

CommandResult Simulation::check(const SellTowerCommand &command) const
{
    return checkTowerCommand(command.tower);
}

CommandResult Simulation::check(const CastSpellCommand &command) const
{
    if (state_.status != GameStatus::Running) {
        return CommandResult::GameOver;
    }
    if (command.spell < 0 || std::cmp_greater_equal(command.spell, content_.spells.size())) {
        return CommandResult::UnknownSpell;
    }
    if (!content_.grid.contains(command.cell)) {
        return CommandResult::OutsideMap;
    }
    return state_.spellCooldowns[static_cast<std::size_t>(command.spell)] > 0
               ? CommandResult::SpellCooling
               : CommandResult::Accepted;
}

CommandResult Simulation::run(const SetTargetPriorityCommand &command)
{
    const CommandResult result = check(command);
    if (result == CommandResult::Accepted) {
        std::ranges::find(state_.towers, command.tower, &Tower::id)->priority = command.priority;
    }
    return result;
}

CommandResult Simulation::run(const UpgradeTowerCommand &command)
{
    const CommandResult result = check(command);
    if (result == CommandResult::Accepted) {
        Tower &tower = *std::ranges::find(state_.towers, command.tower, &Tower::id);
        const qint64 cost = nextLevelOf(tower)->cost;
        state_.gold -= cost;
        tower.spent += cost;
        ++tower.level;
    }
    return result;
}

CommandResult Simulation::run(const SellTowerCommand &command)
{
    const CommandResult result = check(command);
    if (result == CommandResult::Accepted) {
        state_.gold += sellValue(*towerWithId(command.tower));
        std::erase_if(state_.towers,
                      [&command](const Tower &tower) { return tower.id == command.tower; });
    }
    return result;
}

CommandResult Simulation::run(const BuildTowerCommand &command)
{
    const CommandResult result = check(command);
    if (result == CommandResult::Accepted) {
        const auto type = static_cast<std::size_t>(command.towerType);
        const qint64 cost = content_.towers[type].levels.front().cost;
        state_.gold -= cost;
        state_.towers.push_back({.id = state_.nextEntityId++,
                                 .type = type,
                                 .cell = command.cell,
                                 .rotation = command.rotation,
                                 .spent = cost});
    }
    return result;
}

CommandResult Simulation::run(const CastSpellCommand &command)
{
    const CommandResult result = check(command);
    if (result != CommandResult::Accepted) {
        return result;
    }
    const auto index = static_cast<std::size_t>(command.spell);
    const SpellType &spell = content_.spells[index];
    const Fixed damage = rollDamage(spell.damageMin, spell.damageMax);
    const Point centre = centreOf(command.cell);
    const Fixed reach = spell.radius * spell.radius;
    for (Enemy &enemy : state_.enemies) {
        if (distanceSquared(centre, position(enemy)) <= reach) {
            hit(&enemy, spell.damageType, spell.effects, damage);
        }
    }
    removeDeadEnemies();
    state_.spellCooldowns[index] = spell.cooldownTicks;
    return result;
}

bool Simulation::hasTowerAt(Cell cell) const
{
    return std::ranges::any_of(state_.towers, [this, cell](const Tower &tower) {
        return std::ranges::contains(
            footprintAt(content_.towers[tower.type], tower.cell, tower.rotation), cell);
    });
}

void Simulation::tick()
{
    if (state_.status != GameStatus::Running) {
        return;
    }
    ++state_.tick;
    std::ranges::transform(state_.spellCooldowns,
                           state_.spellCooldowns.begin(),
                           [](qint64 cooldown) { return std::max<qint64>(cooldown - 1, 0); });
    moveEnemies();
    spawnEnemies();
    updateHealth();
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

const TowerLevel &Simulation::levelOf(const Tower &tower) const
{
    return content_.towers[tower.type].levels[tower.level];
}

const TowerLevel *Simulation::nextLevelOf(const Tower &tower) const
{
    const std::vector<TowerLevel> &levels = content_.towers[tower.type].levels;
    return tower.level + 1 < levels.size() ? &levels[tower.level + 1] : nullptr;
}

qint64 Simulation::sellValue(const Tower &tower) const
{
    return tower.spent * content_.sellPercent / kPercent;
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
        Fixed speed = type.speedPerTick;
        if (enemy.slow.ticks > 0) {
            speed = std::max(speed * (Fixed::fromInt(1) - enemy.slow.strength), Fixed::fromRaw(1));
            countDown(&enemy.slow);
        }
        enemy.travelled = enemy.travelled + speed;
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
                              Fixed(),
                              {},
                              {}});
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

void Simulation::updateHealth()
{
    for (Enemy &enemy : state_.enemies) {
        if (enemy.poison.ticks > 0) {
            hurt(&enemy, enemy.poison.strength);
            countDown(&enemy.poison);
        } else {
            const EnemyType &type = content_.enemies[enemy.type];
            enemy.health = std::min(enemy.health + type.regeneration, type.health);
        }
    }
    removeDeadEnemies();
}

std::optional<std::size_t> Simulation::findTarget(const Tower &tower,
                                                  const std::vector<Point> &positions) const
{
    const Fixed range = levelOf(tower).range;
    const Fixed reach = range * range;
    const Point place = centreOf(tower.cell);
    std::optional<std::size_t> target;
    Fixed bestScore;
    for (std::size_t index = 0; index < state_.enemies.size(); ++index) {
        const Enemy &enemy = state_.enemies[index];
        const Fixed away = distanceSquared(place, positions[index]);
        if (enemy.health <= Fixed() || away > reach) {
            continue;
        }
        // The strict comparison keeps the lowest id among equal enemies.
        const Fixed score = targetScore(tower.priority, enemy, away);
        if (!target || score > bestScore) {
            target = index;
            bestScore = score;
        }
    }
    return target;
}

Fixed Simulation::rollDamage(Fixed lowest, Fixed highest)
{
    const qint64 spread = highest.raw() - lowest.raw();
    const quint32 roll = state_.random.nextBelow(static_cast<quint32>(spread + 1));
    return lowest + Fixed::fromRaw(roll);
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
        const std::optional<std::size_t> target =
            tower.cooldownTicks == 0 ? findTarget(tower, positions) : std::nullopt;
        if (target) {
            strike(tower, positions, *target);
            tower.cooldownTicks = levelOf(tower).cooldownTicks;
        }
    }
    removeDeadEnemies();
}

/// One roll for the whole shot, each enemy then takes it with its own resistance.
void Simulation::strike(const Tower &tower, const std::vector<Point> &positions, std::size_t target)
{
    const TowerLevel &level = levelOf(tower);
    const Fixed damage = rollDamage(level.damageMin, level.damageMax);
    const DamageType damageType = content_.towers[tower.type].damageType;
    hit(&state_.enemies[target], damageType, level.effects, damage);
    if (level.splashRadius == Fixed()) {
        return;
    }
    const Fixed reach = level.splashRadius * level.splashRadius;
    for (std::size_t index = 0; index < state_.enemies.size(); ++index) {
        Enemy &enemy = state_.enemies[index];
        if (index != target && enemy.health > Fixed() &&
            distanceSquared(positions[target], positions[index]) <= reach) {
            hit(&enemy, damageType, level.effects, damage);
        }
    }
}

void Simulation::hit(Enemy *enemy,
                     DamageType damageType,
                     const std::vector<StatusEffect> &effects,
                     Fixed damage)
{
    const Fixed share = hitShare(content_.enemies[enemy->type], damageType);
    hurt(enemy, damage * share);
    for (const StatusEffect &effect : effects) {
        const Fixed strength = effect.strength * share;
        if (effect.kind == StatusKind::Slow) {
            afflict(&enemy->slow, std::min(strength, maxSlow()), effect.durationTicks);
        } else {
            afflict(&enemy->poison, strength, effect.durationTicks);
        }
    }
}

void Simulation::hurt(Enemy *enemy, Fixed damage)
{
    enemy->health = enemy->health - damage;
    if (enemy->health <= Fixed()) {
        state_.gold += content_.enemies[enemy->type].bounty;
    }
}

void Simulation::removeDeadEnemies()
{
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
