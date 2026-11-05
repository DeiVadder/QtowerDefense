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
    Screenshot,
    Pause,
    Resume,
    Quit,
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
    if (name == "screenshot"_L1) {
        return Action::Screenshot;
    }
    if (name == "pause"_L1) {
        return Action::Pause;
    }
    if (name == "resume"_L1) {
        return Action::Resume;
    }
    if (name == "quit"_L1) {
        return Action::Quit;
    }
    return std::nullopt;
}

QByteArray failure(const QString &message)
{
    return compact({{"ok"_L1, false}, {"error"_L1, message}});
}

QJsonObject observationOf(const core::Simulation &game)
{
    const core::SimulationState &state = game.state();
    QJsonArray towers;
    for (const core::Tower &tower : state.towers) {
        towers.append(QJsonObject{{"id"_L1, tower.id},
                                  {"type"_L1, static_cast<qint64>(tower.type)},
                                  {"x"_L1, tower.cell.x},
                                  {"y"_L1, tower.cell.y},
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

/// A game without a window, for qtd_headless.
class HeadlessGame : public ControlledGame
{
public:
    QString start(const QString &map, quint64 seed) override
    {
        core::ContentLoadResult loaded = core::loadContentFile(map);
        if (!loaded.content) {
            return loaded.error;
        }
        recorder_ = std::make_unique<core::ReplayRecorder>(
            *std::move(loaded.content), QFileInfo(map).fileName(), seed);
        return {};
    }

    core::ReplayRecorder *recorder() override
    {
        return recorder_.get();
    }

private:
    std::unique_ptr<core::ReplayRecorder> recorder_;
};

QString onlyInTheRealGame()
{
    return u"this works only in the real game"_s;
}

} // namespace

void ControlledGame::changed() {}

void ControlledGame::observe(QJsonObject * /*answer*/) const {}

QString ControlledGame::saveScreenshot(const QString & /*file*/, QJsonObject * /*answer*/)
{
    return onlyInTheRealGame();
}

QString ControlledGame::setPaused(bool /*paused*/)
{
    return onlyInTheRealGame();
}

QString ControlledGame::quit()
{
    return onlyInTheRealGame();
}

ControlSession::ControlSession() : ownGame_(std::make_unique<HeadlessGame>()), game_(ownGame_.get())
{
}

ControlSession::ControlSession(ControlledGame *game) : game_(game) {}

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
    if (*action != Action::StartMap && *action != Action::Quit && game_->recorder() == nullptr) {
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
    case Action::Screenshot:
        return saveScreenshot(reader);
    case Action::Pause:
        return finish(reader, [this] { return game_->setPaused(true); });
    case Action::Resume:
        return finish(reader, [this] { return game_->setPaused(false); });
    case Action::Quit:
        return finish(reader, [this] { return game_->quit(); });
    case Action::Observe:
        break;
    }
    return finish(reader, [] { return QString(); });
}

QByteArray ControlSession::finish(const core::ObjectReader &request,
                                  const std::function<QString()> &act)
{
    request.rejectUnknownKeys();
    if (request.failed()) {
        return failure(request.error());
    }
    const QString problem = act();
    if (!problem.isEmpty()) {
        return failure(problem);
    }
    return game_->recorder() != nullptr ? compact(observation()) : compact({{"ok"_L1, true}});
}

QJsonObject ControlSession::observation() const
{
    QJsonObject answer = observationOf(game_->recorder()->simulation());
    game_->observe(&answer);
    return answer;
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
    const QString problem = game_->start(map, *seed);
    if (!problem.isEmpty()) {
        return failure(problem);
    }
    game_->changed();
    return compact(observation());
}

QByteArray ControlSession::buildTower(core::ObjectReader &request)
{
    const qint64 x = request.integer("x"_L1, -core::kMaxExactInteger, core::kMaxExactInteger);
    const qint64 y = request.integer("y"_L1, -core::kMaxExactInteger, core::kMaxExactInteger);
    const qint64 tower =
        request.integer("towerType"_L1, -core::kMaxExactInteger, core::kMaxExactInteger);
    request.rejectUnknownKeys();
    if (request.failed()) {
        return failure(request.error());
    }
    const core::CommandResult result =
        game_->recorder()->execute(core::BuildTowerCommand{{x, y}, tower});
    game_->changed();
    QJsonObject answer = observation();
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
    game_->recorder()->advance(ticks);
    game_->changed();
    return compact(observation());
}

QByteArray ControlSession::saveReplay(core::ObjectReader &request)
{
    const QString name = request.text("file"_L1);
    request.rejectUnknownKeys();
    if (request.failed()) {
        return failure(request.error());
    }
    QString error;
    if (!writeFile(name, core::toJson(game_->recorder()->replay()), &error)) {
        return failure(error);
    }
    return compact(observation());
}

QByteArray ControlSession::saveScreenshot(core::ObjectReader &request)
{
    const QString name = request.text("file"_L1);
    request.rejectUnknownKeys();
    if (request.failed()) {
        return failure(request.error());
    }
    QJsonObject picture;
    const QString problem = game_->saveScreenshot(name, &picture);
    if (!problem.isEmpty()) {
        return failure(problem);
    }
    QJsonObject answer = observation();
    answer.insert("screenshot"_L1, picture);
    return compact(answer);
}

} // namespace qtd::automation
