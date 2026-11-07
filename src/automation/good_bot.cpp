#include "automation/good_bot.h"

#include <QtTypes>

#include <algorithm>
#include <array>
#include <cstddef>
#include <iterator>
#include <map>
#include <optional>
#include <tuple>
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
/// resistances. A tower that only slows is worth nothing here, see E-38.
double damagePerTick(const core::TowerLevel &level)
{
    const auto cooldown = static_cast<double>(level.cooldownTicks);
    double damage = (real(level.damageMin) + real(level.damageMax)) / 2 / cooldown;
    damage *= 1 + real(level.splashRadius);
    for (const core::StatusEffect &effect : level.effects) {
        switch (effect.kind) {
        case core::StatusKind::Slow:
            damage *= 1 + real(effect.strength);
            break;
        case core::StatusKind::Poison:
            damage += real(effect.strength) *
                      static_cast<double>(std::min(effect.durationTicks, level.cooldownTicks)) /
                      cooldown;
            break;
        }
    }
    return damage;
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
    qint64 coverage(core::Cell cell, core::Fixed range)
    {
        const std::tuple<qint64, qint64, qint64> key{cell.x, cell.y, range.raw()};
        const std::map<std::tuple<qint64, qint64, qint64>, qint64>::const_iterator known =
            coverage_.find(key);
        if (known != coverage_.end()) {
            return known->second;
        }
        const core::Point centre = core::centreOf(cell);
        const core::Fixed reach = range * range;
        const qint64 count = std::ranges::count_if(samples_, [&](core::Point point) {
            return core::distanceSquared(centre, point) <= reach;
        });
        coverage_.emplace(key, count);
        return count;
    }

    double worth(const core::TowerType &type, std::size_t level, core::Cell cell)
    {
        const core::TowerLevel &stats = type.levels[level];
        return damagePerTick(stats) * shares_[static_cast<std::size_t>(type.damageType)] *
               static_cast<double>(coverage(cell, stats.range));
    }

    std::optional<Plan> bestPlan(const core::Simulation &game)
    {
        std::optional<Plan> best = bestUpgrade(game);
        for (std::size_t type = 0; type < game.content().towers.size(); ++type) {
            improveWithBuild(game, type, &best);
        }
        return best;
    }

    /// Replaces the plan with a tower of this type where it is worth more per gold.
    void improveWithBuild(const core::Simulation &game, std::size_t type, std::optional<Plan> *best)
    {
        const core::Content &content = game.content();
        const core::TowerType &tower = content.towers[type];
        const auto cost = static_cast<double>(std::max<qint64>(tower.levels.front().cost, 1));
        for (qint64 y = 0; y < content.grid.height(); ++y) {
            for (qint64 x = 0; x < content.grid.width(); ++x) {
                const double value = worth(tower, 0, {x, y}) / cost;
                if (value <= 0 || (*best && value <= (*best)->value)) {
                    continue;
                }
                if (const std::optional<core::BuildTowerCommand> build =
                        placement(game, type, {x, y})) {
                    *best = Plan{*build, tower.levels.front().cost, value};
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

    std::optional<Plan> bestUpgrade(const core::Simulation &game)
    {
        std::optional<Plan> best;
        for (const core::Tower &tower : game.state().towers) {
            const core::TowerType &type = game.content().towers[tower.type];
            if (tower.level + 1 >= type.levels.size()) {
                continue;
            }
            const qint64 cost = type.levels[tower.level + 1].cost;
            const double gain =
                worth(type, tower.level + 1, tower.cell) - worth(type, tower.level, tower.cell);
            const double value = gain / static_cast<double>(std::max<qint64>(cost, 1));
            if (value > 0 && (!best || value > best->value)) {
                best = Plan{core::UpgradeTowerCommand{tower.id}, cost, value};
            }
        }
        return best;
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
    std::map<std::tuple<qint64, qint64, qint64>, qint64> coverage_;
    std::optional<Plan> plan_;
    qint64 replanTick_ = 0;
};

} // namespace

std::unique_ptr<Bot> makeGoodBot()
{
    return std::make_unique<GoodBot>();
}

} // namespace qtd::automation
