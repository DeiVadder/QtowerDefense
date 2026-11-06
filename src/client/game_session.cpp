#include "client/game_session.h"

#include "automation/report.h"
#include "core/content_loader.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QRandomGenerator>
#include <QVariantMap>

#include <algorithm>
#include <iterator>
#include <utility>

using namespace Qt::StringLiterals;

namespace qtd::client {

namespace {

constexpr qint64 kMaxFrameMicroseconds = 250'000;
constexpr int kMaxSpeed = 3;
constexpr double kMillionthsPerTick = 1'000'000;

double mapUnits(core::Fixed value)
{
    return static_cast<double>(value.raw()) / static_cast<double>(core::Fixed::fromInt(1).raw());
}

QPointF mapPlace(const core::Point &point)
{
    return {mapUnits(point.x), mapUnits(point.y)};
}

} // namespace

GameSession::GameSession(QObject *parent) : QObject(parent)
{
    if (const QGuiApplication *application =
            qobject_cast<QGuiApplication *>(QCoreApplication::instance())) {
        connect(application,
                &QGuiApplication::applicationStateChanged,
                this,
                &GameSession::applicationStateChanged);
    }
}

GameSession::~GameSession() = default;

void GameSession::startNewGame(const QString &fileName)
{
    startGame(fileName, QRandomGenerator::global()->generate64());
}

void GameSession::restart()
{
    startNewGame(fileName_);
}

void GameSession::startGame(const QString &fileName, quint64 seed)
{
    fileName_ = fileName;
    saveIsOurs_ = false;
    saveError_.clear();
    emit saveChanged();
    core::ContentLoadResult loaded = core::loadContentFile(fileName);
    error_ = loaded.error;
    game_.reset();
    if (loaded.content) {
        game_ = std::make_unique<core::ReplayRecorder>(
            *std::move(loaded.content), QFileInfo(fileName).fileName(), seed);
    }
    showNewGame();
}

QString GameSession::resumeGame(const QString &fileName, const QString &saveFile)
{
    core::ResumeResult resumed = core::resumeGame(fileName, saveFile);
    if (!resumed.game) {
        return resumed.error;
    }
    fileName_ = fileName;
    error_.clear();
    game_ = std::move(resumed.game);
    showNewGame();
    setPaused(true);
    return {};
}

void GameSession::showNewGame()
{
    lastPlaces_.clear();
    selectedSpell_ = -1;
    clock_ = core::TickClock();
    clock_.setSpeed(speed_);
    paused_ = false;
    chosenTower_ = 0;
    selectedTower_ = 0;
    selectedRotation_ = core::Rotation::None;
    emit mapChanged();
    emit selectionChanged();
    emit towersChanged();
    emit stateChanged();
    emit clockChanged();
}

bool GameSession::buildTower(int x, int y)
{
    if (!game_ ||
        game_->execute(core::BuildTowerCommand{{x, y}, selectedTower_, selectedRotation_}) !=
            core::CommandResult::Accepted) {
        return false;
    }
    emit towersChanged();
    emit stateChanged();
    return true;
}

bool GameSession::chooseTowerAt(int x, int y)
{
    if (!game_) {
        return false;
    }
    const core::Simulation &game = game_->simulation();
    const core::Cell cell{x, y};
    const std::vector<core::Tower>::const_iterator found =
        std::ranges::find_if(game.state().towers, [&](const core::Tower &tower) {
            return std::ranges::contains(
                core::footprintAt(game.content().towers[tower.type], tower.cell, tower.rotation),
                cell);
        });
    if (found == game.state().towers.end()) {
        return false;
    }
    chosenTower_ = found->id;
    emit towersChanged();
    return true;
}

void GameSession::clearChosenTower()
{
    chosenTower_ = 0;
    emit towersChanged();
}

void GameSession::setChosenPriority(int priority)
{
    if (game_ && chosenTower_ != 0 && priority >= 0 &&
        std::cmp_less(priority, core::kTargetPriorities)) {
        game_->execute(core::SetTargetPriorityCommand{chosenTower_,
                                                      static_cast<core::TargetPriority>(priority)});
        emit towersChanged();
    }
}

bool GameSession::upgradeChosenTower()
{
    return chosen() != nullptr && executeOnChosen(core::UpgradeTowerCommand{chosenTower_});
}

bool GameSession::sellChosenTower()
{
    if (chosen() == nullptr || !executeOnChosen(core::SellTowerCommand{chosenTower_})) {
        return false;
    }
    clearChosenTower();
    return true;
}

bool GameSession::executeOnChosen(const core::Command &command)
{
    if (game_->execute(command) != core::CommandResult::Accepted) {
        return false;
    }
    emit towersChanged();
    emit stateChanged();
    return true;
}

void GameSession::selectSpell(int index)
{
    const QVariantList seconds = spellSeconds();
    if (index == selectedSpell_) {
        selectedSpell_ = -1;
    } else if (index >= 0 && index < seconds.size() && seconds[index].toLongLong() == 0) {
        selectedSpell_ = index;
    } else {
        return;
    }
    emit selectionChanged();
}

bool GameSession::castSpellAt(int x, int y)
{
    if (!game_ || selectedSpell_ < 0 ||
        game_->execute(core::CastSpellCommand{selectedSpell_, {x, y}}) !=
            core::CommandResult::Accepted) {
        return false;
    }
    selectedSpell_ = -1;
    emit selectionChanged();
    emit stateChanged();
    emit frameChanged();
    return true;
}

bool GameSession::callWave()
{
    if (!game_ || game_->execute(core::CallWaveCommand{}) != core::CommandResult::Accepted) {
        return false;
    }
    emit stateChanged();
    return true;
}

bool GameSession::canCallWave() const
{
    return game_ &&
           game_->simulation().check(core::CallWaveCommand{}) == core::CommandResult::Accepted;
}

qint64 GameSession::waveCallBonus() const
{
    return game_ ? game_->simulation().waveCallBonus() : 0;
}

bool GameSession::saveGame()
{
    if (!canSave()) {
        return false;
    }
    saveError_.clear();
    QDir().mkpath(QFileInfo(saveFile_).path());
    const bool saved = automation::writeFile(saveFile_, core::toJson(game_->replay()), &saveError_);
    saveIsOurs_ = saveIsOurs_ || saved;
    emit saveChanged();
    return saved;
}

bool GameSession::loadGame()
{
    const core::ReplayLoadResult saved = core::loadReplayFile(saveFile_);
    saveError_ = saved.error;
    if (saved.replay) {
        const QString map = QFileInfo(saved.replay->content).fileName();
        saveError_ = resumeGame(QFileInfo(fileName_).dir().filePath(map), saveFile_);
        saveIsOurs_ = saveError_.isEmpty();
    }
    emit saveChanged();
    return saveError_.isEmpty();
}

const QString &GameSession::saveFile() const
{
    return saveFile_;
}

void GameSession::setSaveFile(const QString &fileName)
{
    saveFile_ = fileName;
    emit saveChanged();
    emit stateChanged();
}

bool GameSession::canSave() const
{
    return game_ && !saveFile_.isEmpty() &&
           game_->simulation().state().status == core::GameStatus::Running;
}

bool GameSession::hasSave() const
{
    return !saveFile_.isEmpty() && QFileInfo::exists(saveFile_);
}

const QString &GameSession::saveError() const
{
    return saveError_;
}

void GameSession::rotateSelection()
{
    selectedRotation_ = core::nextRotation(selectedRotation_);
    emit selectionChanged();
}

core::ReplayRecorder *GameSession::recorder()
{
    return game_.get();
}

void GameSession::refresh()
{
    lastPlaces_.clear();
    emit towersChanged();
    emit stateChanged();
    emit frameChanged();
}

void GameSession::advanceTime(qint64 microseconds)
{
    if (!game_) {
        return;
    }
    const qint64 before = game_->simulation().state().tick;
    const qint64 ticks =
        clock_.ticksFor(std::clamp<qint64>(microseconds, 0, kMaxFrameMicroseconds));
    if (ticks > 0) {
        game_->advance(ticks - 1);
        lastPlaces_.clear();
        for (const core::Enemy &enemy : game_->simulation().state().enemies) {
            lastPlaces_.emplace(enemy.id, game_->simulation().position(enemy));
        }
        game_->advance(1);
    }
    if (game_->simulation().state().tick != before) {
        emit stateChanged();
    }
    emit frameChanged();
}

const QString &GameSession::error() const
{
    return error_;
}

QSize GameSession::gridSize() const
{
    return game_ ? QSize(static_cast<int>(game_->simulation().content().grid.width()),
                         static_cast<int>(game_->simulation().content().grid.height()))
                 : QSize();
}

QVariantList GameSession::towerTypes() const
{
    QVariantList types;
    if (game_) {
        for (const core::TowerType &type : game_->simulation().content().towers) {
            types.append(QVariantMap{{u"name"_s, type.id}, {u"cost"_s, type.levels.front().cost}});
        }
    }
    return types;
}

QStringList GameSession::spellNames() const
{
    QStringList names;
    if (game_) {
        std::ranges::transform(
            game_->simulation().content().spells, std::back_inserter(names), &core::SpellType::id);
    }
    return names;
}

QVariantList GameSession::spellSeconds() const
{
    QVariantList seconds;
    if (game_) {
        for (const qint64 cooldown : game_->simulation().state().spellCooldowns) {
            seconds.append((cooldown + core::kTicksPerSecond - 1) / core::kTicksPerSecond);
        }
    }
    return seconds;
}

int GameSession::selectedSpell() const
{
    return selectedSpell_;
}

int GameSession::selectedTower() const
{
    return selectedTower_;
}

QVariantList GameSession::selectedShape() const
{
    QVariantList shape;
    if (!game_) {
        return shape;
    }
    const core::TowerType &type =
        game_->simulation().content().towers[static_cast<std::size_t>(selectedTower_)];
    const std::vector<core::Cell> cells = core::footprintAt(type, {}, selectedRotation_);
    const qint64 left = std::ranges::min(cells, {}, &core::Cell::x).x;
    const qint64 top = std::ranges::min(cells, {}, &core::Cell::y).y;
    for (const core::Cell cell : cells) {
        shape.append(QVariantMap{
            {u"x"_s, cell.x - left}, {u"y"_s, cell.y - top}, {u"anchor"_s, cell == core::Cell{}}});
    }
    return shape;
}

int GameSession::chosenPriority() const
{
    const core::Tower *tower = chosen();
    return tower != nullptr ? static_cast<int>(tower->priority) : -1;
}

const core::Tower *GameSession::chosen() const
{
    return game_ ? game_->simulation().towerWithId(chosenTower_) : nullptr;
}

qint64 GameSession::chosenUpgradeCost() const
{
    const core::Tower *tower = chosen();
    const core::TowerLevel *next =
        tower != nullptr ? game_->simulation().nextLevelOf(*tower) : nullptr;
    return next != nullptr ? next->cost : -1;
}

qint64 GameSession::chosenSellValue() const
{
    const core::Tower *tower = chosen();
    return tower != nullptr ? game_->simulation().sellValue(*tower) : -1;
}

qint64 GameSession::chosenTower() const
{
    return chosenTower_;
}

int GameSession::towerCount() const
{
    return game_ ? static_cast<int>(game_->simulation().state().towers.size()) : 0;
}

const core::Grid *GameSession::grid() const
{
    return game_ ? &game_->simulation().content().grid : nullptr;
}

const core::Simulation *GameSession::simulation() const
{
    return game_ ? &game_->simulation() : nullptr;
}

QVariantList GameSession::towerCells() const
{
    QVariantList cells;
    if (game_) {
        for (const core::Tower &tower : game_->simulation().state().towers) {
            std::ranges::transform(
                core::footprintAt(
                    game_->simulation().content().towers[tower.type], tower.cell, tower.rotation),
                std::back_inserter(cells),
                [](core::Cell cell) {
                    return QPoint(static_cast<int>(cell.x), static_cast<int>(cell.y));
                });
        }
    }
    return cells;
}

qint64 GameSession::gold() const
{
    return game_ ? game_->simulation().state().gold : 0;
}

qint64 GameSession::lives() const
{
    return game_ ? game_->simulation().state().lives : 0;
}

qint64 GameSession::tick() const
{
    return game_ ? game_->simulation().state().tick : 0;
}

int GameSession::enemyCount() const
{
    return game_ ? static_cast<int>(game_->simulation().state().enemies.size()) : 0;
}

bool GameSession::won() const
{
    return game_ && game_->simulation().state().status == core::GameStatus::Won;
}

bool GameSession::lost() const
{
    return game_ && game_->simulation().state().status == core::GameStatus::Lost;
}

bool GameSession::paused() const
{
    return paused_;
}

int GameSession::speed() const
{
    return speed_;
}

int GameSession::maxSpeed() const
{
    return kMaxSpeed;
}

std::vector<QPointF> GameSession::enemyPlaces() const
{
    std::vector<QPointF> places;
    if (!game_) {
        return places;
    }
    const bool moving = game_->simulation().state().status == core::GameStatus::Running;
    const double share = static_cast<double>(clock_.pendingMillionths()) / kMillionthsPerTick;
    places.reserve(game_->simulation().state().enemies.size());
    std::ranges::transform(game_->simulation().state().enemies,
                           std::back_inserter(places),
                           [&](const core::Enemy &enemy) {
                               const QPointF place = mapPlace(game_->simulation().position(enemy));
                               const std::map<qint64, core::Point>::const_iterator last =
                                   lastPlaces_.find(enemy.id);
                               if (!moving || last == lastPlaces_.end()) {
                                   return place;
                               }
                               const QPointF from = mapPlace(last->second);
                               return from + (place - from) * share;
                           });
    return places;
}

void GameSession::applicationStateChanged(Qt::ApplicationState state)
{
    if (state == Qt::ApplicationSuspended || state == Qt::ApplicationHidden) {
        setPaused(true);
        if (saveIsOurs_ || !hasSave()) {
            saveGame();
        }
    }
}

void GameSession::setSelectedTower(int index)
{
    const int last = game_ ? static_cast<int>(game_->simulation().content().towers.size()) - 1 : 0;
    const int limited = std::clamp(index, 0, last);
    if (selectedTower_ != limited || selectedSpell_ >= 0) {
        selectedTower_ = limited;
        selectedSpell_ = -1; // whoever picks a tower wants to build
        emit selectionChanged();
    }
}

void GameSession::setPaused(bool paused)
{
    if (paused_ != paused) {
        paused_ = paused;
        clock_.setPaused(paused);
        emit clockChanged();
    }
}

void GameSession::setSpeed(int speed)
{
    const int limited = std::clamp(speed, 1, kMaxSpeed);
    if (speed_ != limited) {
        speed_ = limited;
        clock_.setSpeed(limited);
        emit clockChanged();
    }
}

} // namespace qtd::client
