#include "automation/control_session.h"

#include "automation/report.h"
#include "core/content_loader.h"
#include "core/simulation.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QLatin1StringView>

#include <optional>
#include <utility>

using namespace Qt::StringLiterals;

namespace qtd::automation {

namespace {

constexpr qint64 kMaxTicksPerRequest = 1'000'000;

enum class Action : quint8 {
    StartMap,
    BuildTower,
    Advance,
    Observe,
    SaveReplay,
};

std::optional<Action> actionFromName(const QString &name)
{
    if (name == "startMap"_L1) {
        return Action::StartMap;
    }
    if (name == "buildTower"_L1) {
        return Action::BuildTower;
    }
    if (name == "advance"_L1) {
        return Action::Advance;
    }
    if (name == "observe"_L1) {
        return Action::Observe;
    }
    if (name == "saveReplay"_L1) {
        return Action::SaveReplay;
    }
    return std::nullopt;
}

QByteArray failure(const QString &message)
{
    return compact({{"ok"_L1, false}, {"error"_L1, message}});
}

QJsonObject observation(const core::Simulation &game)
{
    const core::SimulationState &state = game.state();
    QJsonArray towers;
    for (const core::Tower &tower : state.towers) {
        towers.append(QJsonObject{{"id"_L1, tower.id},
                                  {"type"_L1, static_cast<qint64>(tower.type)},
                                  {"slot"_L1, static_cast<qint64>(tower.slot)},
                                  {"cooldownTicks"_L1, tower.cooldownTicks}});
    }
    QJsonArray enemies;
    for (const core::Enemy &enemy : state.enemies) {
        const core::Point place = game.position(enemy);
        enemies.append(QJsonObject{{"id"_L1, enemy.id},
                                   {"type"_L1, static_cast<qint64>(enemy.type)},
                                   {"health"_L1, core::toString(enemy.health)},
                                   {"travelled"_L1, core::toString(enemy.travelled)},
                                   {"x"_L1, core::toString(place.x)},
                                   {"y"_L1, core::toString(place.y)}});
    }
    QJsonObject answer = stateReport(game);
    answer.insert("ok"_L1, true);
    answer.insert("wave"_L1,
                  QJsonObject{{"group"_L1, static_cast<qint64>(state.spawnGroup)},
                              {"spawnedInGroup"_L1, state.spawnedInGroup},
                              {"countdownTicks"_L1, state.spawnCountdownTicks}});
    answer.insert("towers"_L1, towers);
    answer.insert("enemies"_L1, enemies);
    return answer;
}

} // namespace

ControlSession::ControlSession() = default;
ControlSession::~ControlSession() = default;

QByteArray ControlSession::handle(QByteArrayView request)
{
    QString error;
    std::optional<QJsonObject> object = core::parseExactJsonObject(request, &error);
    if (!object) {
        return failure(error);
    }
    core::ObjectReader reader(*std::move(object), u"request"_s, &error);
    const QString name = reader.text("action"_L1);
    if (reader.failed()) {
        return failure(error);
    }
    const std::optional<Action> action = actionFromName(name);
    if (!action) {
        return failure(u"unknown action \"%1\""_s.arg(name));
    }
    if (*action != Action::StartMap && !game_) {
        return failure(u"no game, use startMap first"_s);
    }
    switch (*action) {
    case Action::StartMap:
        return startMap(reader);
    case Action::BuildTower:
        return buildTower(reader);
    case Action::Advance:
        return advance(reader);
    case Action::SaveReplay:
        return saveReplay(reader);
    case Action::Observe:
        break;
    }
    reader.rejectUnknownKeys();
    return reader.failed() ? failure(error) : compact(observation(game_->simulation()));
}

QByteArray ControlSession::startMap(core::ObjectReader &request)
{
    const QString map = request.text("map"_L1);
    const QString seedText = request.text("seed"_L1);
    request.rejectUnknownKeys();
    if (request.failed()) {
        return failure(request.error());
    }
    const std::optional<quint64> seed = core::seedFromText(seedText);
    if (!seed) {
        return failure(u"seed must be a text of decimal digits that fits 64 bits"_s);
    }
    core::ContentLoadResult loaded = core::loadContentFile(map);
    if (!loaded.content) {
        return failure(loaded.error);
    }
    game_ = std::make_unique<core::ReplayRecorder>(
        *std::move(loaded.content), QFileInfo(map).fileName(), *seed);
    return compact(observation(game_->simulation()));
}

QByteArray ControlSession::buildTower(core::ObjectReader &request)
{
    const qint64 slot = request.integer("slot"_L1, -core::kMaxExactInteger, core::kMaxExactInteger);
    const qint64 tower =
        request.integer("towerType"_L1, -core::kMaxExactInteger, core::kMaxExactInteger);
    request.rejectUnknownKeys();
    if (request.failed()) {
        return failure(request.error());
    }
    const core::CommandResult result = game_->execute(core::BuildTowerCommand{slot, tower});
    QJsonObject answer = observation(game_->simulation());
    answer.insert("result"_L1, core::toString(result));
    return compact(answer);
}

QByteArray ControlSession::advance(core::ObjectReader &request)
{
    const qint64 ticks = request.integer("ticks"_L1, 0, kMaxTicksPerRequest);
    request.rejectUnknownKeys();
    if (request.failed()) {
        return failure(request.error());
    }
    game_->advance(ticks);
    return compact(observation(game_->simulation()));
}

QByteArray ControlSession::saveReplay(core::ObjectReader &request)
{
    const QString name = request.text("file"_L1);
    request.rejectUnknownKeys();
    if (request.failed()) {
        return failure(request.error());
    }
    QString error;
    if (!writeFile(name, core::toJson(game_->replay()), &error)) {
        return failure(error);
    }
    return compact(observation(game_->simulation()));
}

} // namespace qtd::automation
