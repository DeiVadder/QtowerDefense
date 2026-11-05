#include "client/game_session.h"

#include "core/content_loader.h"

#include <QCoreApplication>
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
    core::ContentLoadResult loaded = core::loadContentFile(fileName);
    error_ = loaded.error;
    game_.reset();
    lastPlaces_.clear();
    if (loaded.content) {
        game_ = std::make_unique<core::Simulation>(*std::move(loaded.content), seed);
    }
    clock_ = core::TickClock();
    clock_.setSpeed(speed_);
    paused_ = false;
    selectedTower_ = 0;
    emit mapChanged();
    emit selectionChanged();
    emit towersChanged();
    emit stateChanged();
    emit clockChanged();
}

bool GameSession::buildTower(int x, int y)
{
    if (!game_ || game_->execute(core::BuildTowerCommand{{x, y}, selectedTower_}) !=
                      core::CommandResult::Accepted) {
        return false;
    }
    emit towersChanged();
    emit stateChanged();
    return true;
}

void GameSession::advanceTime(qint64 microseconds)
{
    if (!game_) {
        return;
    }
    const qint64 before = game_->state().tick;
    const qint64 ticks =
        clock_.ticksFor(std::clamp<qint64>(microseconds, 0, kMaxFrameMicroseconds));
    if (ticks > 0) {
        game_->advance(ticks - 1);
        lastPlaces_.clear();
        for (const core::Enemy &enemy : game_->state().enemies) {
            lastPlaces_.emplace(enemy.id, game_->position(enemy));
        }
        game_->advance(1);
    }
    if (game_->state().tick != before) {
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
    return game_ ? QSize(static_cast<int>(game_->content().grid.width()),
                         static_cast<int>(game_->content().grid.height()))
                 : QSize();
}

QVariantList GameSession::towerTypes() const
{
    QVariantList types;
    if (game_) {
        for (const core::TowerType &type : game_->content().towers) {
            types.append(QVariantMap{{u"name"_s, type.id}, {u"cost"_s, type.cost}});
        }
    }
    return types;
}

int GameSession::selectedTower() const
{
    return selectedTower_;
}

int GameSession::towerCount() const
{
    return game_ ? static_cast<int>(game_->state().towers.size()) : 0;
}

const core::Grid *GameSession::grid() const
{
    return game_ ? &game_->content().grid : nullptr;
}

QVariantList GameSession::towerCells() const
{
    QVariantList cells;
    if (game_) {
        for (const core::Tower &tower : game_->state().towers) {
            std::ranges::transform(
                core::footprintAt(game_->content().towers[tower.type], tower.cell),
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
    return game_ ? game_->state().gold : 0;
}

qint64 GameSession::lives() const
{
    return game_ ? game_->state().lives : 0;
}

qint64 GameSession::tick() const
{
    return game_ ? game_->state().tick : 0;
}

int GameSession::enemyCount() const
{
    return game_ ? static_cast<int>(game_->state().enemies.size()) : 0;
}

bool GameSession::won() const
{
    return game_ && game_->state().status == core::GameStatus::Won;
}

bool GameSession::lost() const
{
    return game_ && game_->state().status == core::GameStatus::Lost;
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
    const bool moving = game_->state().status == core::GameStatus::Running;
    const double share = static_cast<double>(clock_.pendingMillionths()) / kMillionthsPerTick;
    places.reserve(game_->state().enemies.size());
    std::ranges::transform(
        game_->state().enemies, std::back_inserter(places), [&](const core::Enemy &enemy) {
            const QPointF place = mapPlace(game_->position(enemy));
            const std::map<qint64, core::Point>::const_iterator last = lastPlaces_.find(enemy.id);
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
    }
}

void GameSession::setSelectedTower(int index)
{
    const int last = game_ ? static_cast<int>(game_->content().towers.size()) - 1 : 0;
    const int limited = std::clamp(index, 0, last);
    if (selectedTower_ != limited) {
        selectedTower_ = limited;
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
