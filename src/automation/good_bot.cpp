#include "automation/good_bot.h"

#include <QtTypes>

#include <algorithm>
#include <array>
#include <cstddef>
#include <iterator>
#include <map>
#include <numeric>
#include <optional>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace qtd::automation {

namespace {

constexpr qint64 kSampleStepRaw = 5'000; // the bot looks at the path every half field
constexpr qint64 kPlanTicks = 30;        // a plan that waits for gold is thought over each second
constexpr qint64 kCrowd = 3;             // a spell waits for this many enemies in its radius
constexpr qint64 kLateShare = 4;         // or for an enemy in the last quarter of the path
constexpr double kRawPerUnit = 10'000.0; // see Fixed

/// A rough value for the estimates of the bot. The bot is no part of the rules: its numbers need
/// no fixed point, only the same result for the same game on the same machine.
double real(core::Fixed value)
{
    return static_cast<double>(value.raw()) / kRawPerUnit;
}

/// Something to spend gold on and what it is worth, in damage on the path per gold.
struct Plan
{
    core::Command command;
    qint64 cost = 0;
    double value = 0;
};

/// The best plan, and the best one the gold pays for now. Waiting for the better one only pays
/// when it is clearly better: the wave does not wait.
class Choice
{
public:
    explicit Choice(qint64 gold) : gold_(gold) {}

    [[nodiscard]] bool wants(double value, qint64 cost) const
    {
        return value > 0 && (!best_ || value > best_->value ||
                             (cost <= gold_ && (!affordable_ || value > affordable_->value)));
    }

    void offer(const Plan &plan)
    {
        if (!best_ || plan.value > best_->value) {
            best_ = plan;
        }
        if (plan.cost <= gold_ && (!affordable_ || plan.value > affordable_->value)) {
            affordable_ = plan;
        }
    }

    [[nodiscard]] std::optional<Plan> pick() const
    {
        if (affordable_ && best_ && affordable_->value * kPatience >= best_->value) {
            return affordable_;
        }
        return best_;
    }

private:
    static constexpr double kPatience = 1.5; // waits for a plan worth half as much again
    qint64 gold_ = 0;
    std::optional<Plan> best_;
    std::optional<Plan> affordable_;
};

/// Points along the path, where the enemies will be.
std::vector<core::Point> pathSamples(const core::Path &path)
{
    std::vector<core::Point> samples;
    for (qint64 raw = 0; raw <= path.length().raw(); raw += kSampleStepRaw) {
        samples.push_back(path.positionAt(core::Fixed::fromRaw(raw)));
    }
    return samples;
}

/// By damage type, how much of a hit the wave of the map takes, weighed by health.
std::array<double, core::kDamageTypes> waveShares(const core::Content &content)
{
    std::array<double, core::kDamageTypes> shares{};
    double total = 0;
    for (const core::SpawnGroup &group : content.wave) {
        const core::EnemyType &enemy = content.enemies[group.enemyType];
        const double weight = real(enemy.health) * static_cast<double>(group.count);
        total += weight;
        for (std::size_t type = 0; type < core::kDamageTypes; ++type) {
            shares[type] +=
                weight * real(core::hitShare(enemy, static_cast<core::DamageType>(type)));
        }
    }
    std::ranges::transform(
        shares, shares.begin(), [total](double share) { return total > 0 ? share / total : 1; });
    return shares;
}

/// Damage a tower of this level does per tick on each point of the path it reaches, before the
/// resistances: its hits, and its poison apart, because the poisons of two towers do not add up.
/// The strongest one works (E-28).
struct Damage
{
    double hit = 0;
    double poison = 0;
};

/// Poison counts for as long as it works between two shots: the tower mostly hits the same enemy
/// again.
Damage damagePerTick(const core::TowerLevel &level, double share)
{
    const auto cooldown = static_cast<double>(level.cooldownTicks);
    const double hit = (real(level.damageMin) + real(level.damageMax)) / 2 / cooldown;
    const double poison = std::accumulate(
        level.effects.begin(),
        level.effects.end(),
        0.0,
        [&level, cooldown](double strongest, const core::StatusEffect &effect) {
            if (effect.kind != core::StatusKind::Poison) {
                return strongest;
            }
            const qint64 working = std::min(effect.durationTicks, level.cooldownTicks);
            return std::max(strongest,
                            real(effect.strength) * static_cast<double>(working) / cooldown);
        });
    const double spread = share * (1 + real(level.splashRadius));
    return {hit * spread, poison * spread};
}

/// How much longer the enemies stay on a point a tower of this level slows: at half the speed,
/// twice as long, so every other tower there does twice the damage.
double slowGain(const core::TowerLevel &level, double share)
{
    double gain = 0;
    for (const core::StatusEffect &effect : level.effects) {
        if (effect.kind == core::StatusKind::Slow) {
            const double strength = std::min(real(effect.strength) * share, real(core::maxSlow()));
            gain = std::max(gain, strength / (1 - strength));
        }
    }
    return gain;
}

class GoodBot : public Bot
{
public:
    std::optional<core::Command> nextCommand(const core::Simulation &game) override
    {
        const core::SimulationState &state = game.state();
        if (state.status != core::GameStatus::Running) {
            return std::nullopt;
        }
        if (samples_.empty()) {
            samples_ = pathSamples(game.content().path);
            shares_ = waveShares(game.content());
        }
        if (const std::optional<core::CastSpellCommand> spell = crowdSpell(game)) {
            return *spell;
        }
        if (!plan_ || state.tick >= replanTick_) {
            plan_ = bestPlan(game);
            replanTick_ = state.tick + kPlanTicks;
        }
        if (!plan_ || state.gold < plan_->cost) {
            return std::nullopt;
        }
        const core::Command command = plan_->command;
        plan_.reset();
        const bool accepted = std::visit(
            [&game](const auto &chosen) {
                return game.check(chosen) == core::CommandResult::Accepted;
            },
            command);
        return accepted ? std::optional<core::Command>(command) : std::nullopt;
    }

private:
    /// The points of the path a tower on this cell reaches.
    const std::vector<std::size_t> &pointsInRange(core::Cell cell, core::Fixed range)
    {
        const std::tuple<qint64, qint64, qint64> key{cell.x, cell.y, range.raw()};
        const std::map<std::tuple<qint64, qint64, qint64>, std::vector<std::size_t>>::iterator
            known = reach_.find(key);
        if (known != reach_.end()) {
            return known->second;
        }
        const core::Point centre = core::centreOf(cell);
        const core::Fixed limit = range * range;
        std::vector<std::size_t> points;
        for (std::size_t index = 0; index < samples_.size(); ++index) {
            if (core::distanceSquared(centre, samples_[index]) <= limit) {
                points.push_back(index);
            }
        }
        return reach_.emplace(key, std::move(points)).first->second;
    }

    /// What the towers that stand do on each point of the path: hits, the strongest poison and
    /// how much they slow. A tower that is to be upgraded counts here with its old level.
    void mapTowers(const core::Simulation &game)
    {
        hitAt_.assign(samples_.size(), 0);
        poisonAt_.assign(samples_.size(), 0);
        slowAt_.assign(samples_.size(), 0);
        for (const core::Tower &tower : game.state().towers) {
            const core::TowerType &type = game.content().towers[tower.type];
            const core::TowerLevel &stats = type.levels[tower.level];
            const double share = shares_[static_cast<std::size_t>(type.damageType)];
            const Damage damage = damagePerTick(stats, share);
            const double slow = slowGain(stats, share);
            for (const std::size_t point : pointsInRange(tower.cell, stats.range)) {
                hitAt_[point] += damage.hit;
                poisonAt_[point] = std::max(poisonAt_[point], damage.poison);
                slowAt_[point] = std::max(slowAt_[point], slow);
            }
        }
    }

    /// The damage a tower adds on the path: its hits and the poison beyond the strongest there,
    /// longer where others slow, and the damage of the others where it slows more than they do.
    double worth(const core::TowerType &type, std::size_t level, core::Cell cell)
    {
        const core::TowerLevel &stats = type.levels[level];
        const double share = shares_[static_cast<std::size_t>(type.damageType)];
        const Damage damage = damagePerTick(stats, share);
        const double slow = slowGain(stats, share);
        const std::vector<std::size_t> &points = pointsInRange(cell, stats.range);
        return std::accumulate(
            points.begin(), points.end(), 0.0, [&](double sum, std::size_t point) {
                const double own = damage.hit + std::max(0.0, damage.poison - poisonAt_[point]);
                const double others = hitAt_[point] + poisonAt_[point];
                return sum + own * (1 + slowAt_[point]) +
                       others * std::max(0.0, slow - slowAt_[point]);
            });
    }

    std::optional<Plan> bestPlan(const core::Simulation &game)
    {
        mapTowers(game);
        Choice choice(game.state().gold);
        offerUpgrades(game, &choice);
        for (std::size_t type = 0; type < game.content().towers.size(); ++type) {
            offerBuilds(game, type, &choice);
        }
        return choice.pick();
    }

    /// Offers a tower of this type where it is worth more per gold.
    void offerBuilds(const core::Simulation &game, std::size_t type, Choice *choice)
    {
        const core::Content &content = game.content();
        const core::TowerType &tower = content.towers[type];
        const qint64 cost = tower.levels.front().cost;
        for (qint64 y = 0; y < content.grid.height(); ++y) {
            for (qint64 x = 0; x < content.grid.width(); ++x) {
                const double value =
                    worth(tower, 0, {x, y}) / static_cast<double>(std::max<qint64>(cost, 1));
                if (!choice->wants(value, cost)) {
                    continue;
                }
                if (const std::optional<core::BuildTowerCommand> build =
                        placement(game, type, {x, y})) {
                    choice->offer(Plan{*build, cost, value});
                }
            }
        }
    }

    /// A way to stand on the cell, turned if the tower must, also while the gold is missing.
    static std::optional<core::BuildTowerCommand>
    placement(const core::Simulation &game, std::size_t type, core::Cell cell)
    {
        for (quint32 turn = 0; turn < core::kRotations; ++turn) {
            const core::BuildTowerCommand command{
                cell, static_cast<qint64>(type), static_cast<core::Rotation>(turn)};
            const core::CommandResult result = game.check(command);
            if (result == core::CommandResult::Accepted ||
                result == core::CommandResult::NotEnoughGold) {
                return command;
            }
        }
        return std::nullopt;
    }

    void offerUpgrades(const core::Simulation &game, Choice *choice)
    {
        for (const core::Tower &tower : game.state().towers) {
            const core::TowerType &type = game.content().towers[tower.type];
            if (tower.level + 1 >= type.levels.size()) {
                continue;
            }
            const qint64 cost = type.levels[tower.level + 1].cost;
            const double gain =
                worth(type, tower.level + 1, tower.cell) - worth(type, tower.level, tower.cell);
            const double value = gain / static_cast<double>(std::max<qint64>(cost, 1));
            if (choice->wants(value, cost)) {
                choice->offer(Plan{core::UpgradeTowerCommand{tower.id}, cost, value});
            }
        }
    }

    /// A ready spell into the thickest crowd, or on an enemy that is about to get through.
    static std::optional<core::CastSpellCommand> crowdSpell(const core::Simulation &game)
    {
        const core::SimulationState &state = game.state();
        const core::Content &content = game.content();
        std::vector<core::Point> places;
        for (std::size_t spell = 0; spell < content.spells.size(); ++spell) {
            if (state.spellCooldowns[spell] > 0) {
                continue;
            }
            if (places.empty()) {
                std::ranges::transform(
                    state.enemies, std::back_inserter(places), [&game](const core::Enemy &enemy) {
                        return game.position(enemy);
                    });
            }
            if (const std::optional<core::Point> target =
                    crowdCentre(game, places, content.spells[spell].radius)) {
                const core::CastSpellCommand command{static_cast<qint64>(spell),
                                                     core::cellAt(*target)};
                if (game.check(command) == core::CommandResult::Accepted) {
                    return command;
                }
            }
        }
        return std::nullopt;
    }

    /// The enemy with the most others in the radius, if they make a crowd or one escapes.
    static std::optional<core::Point> crowdCentre(const core::Simulation &game,
                                                  const std::vector<core::Point> &places,
                                                  core::Fixed radius)
    {
        const std::vector<core::Enemy> &enemies = game.state().enemies;
        const core::Fixed reach = radius * radius;
        const core::Fixed lateFrom = game.content().path.length() *
                                     core::Fixed::fromInt(kLateShare - 1) /
                                     core::Fixed::fromInt(kLateShare);
        std::optional<core::Point> target;
        qint64 most = 0;
        for (std::size_t index = 0; index < enemies.size(); ++index) {
            const qint64 near = std::ranges::count_if(places, [&](core::Point other) {
                return core::distanceSquared(places[index], other) <= reach;
            });
            const bool late = enemies[index].travelled >= lateFrom;
            if ((near >= kCrowd || late) && near > most) {
                most = near;
                target = places[index];
            }
        }
        return target;
    }

    std::vector<core::Point> samples_;
    std::array<double, core::kDamageTypes> shares_{};
    std::map<std::tuple<qint64, qint64, qint64>, std::vector<std::size_t>> reach_;
    std::vector<double> hitAt_;
    std::vector<double> poisonAt_;
    std::vector<double> slowAt_;
    std::optional<Plan> plan_;
    qint64 replanTick_ = 0;
};

} // namespace

std::unique_ptr<Bot> makeGoodBot()
{
    return std::make_unique<GoodBot>();
}

} // namespace qtd::automation
