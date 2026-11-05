#pragma once

#include "core/simulation.h"
#include "core/tick_clock.h"

#include <QObject>
#include <QPoint>
#include <QPointF>
#include <QSize>
#include <QString>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <map>
#include <memory>
#include <vector>

namespace qtd::client {

/// The running game for the QML side. It shows the state and passes on commands, the same commands
/// that bots use. Positions are in map units, the scene decides how large a unit is.
class GameSession : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString error READ error NOTIFY mapChanged)
    Q_PROPERTY(QSize gridSize READ gridSize NOTIFY mapChanged)
    Q_PROPERTY(QVariantList towerTypes READ towerTypes NOTIFY mapChanged)
    Q_PROPERTY(int selectedTower READ selectedTower WRITE setSelectedTower NOTIFY selectionChanged)
    Q_PROPERTY(int towerCount READ towerCount NOTIFY towersChanged)
    Q_PROPERTY(QVariantList towerCells READ towerCells NOTIFY towersChanged)
    Q_PROPERTY(qint64 gold READ gold NOTIFY stateChanged)
    Q_PROPERTY(qint64 lives READ lives NOTIFY stateChanged)
    Q_PROPERTY(qint64 tick READ tick NOTIFY stateChanged)
    Q_PROPERTY(int enemyCount READ enemyCount NOTIFY stateChanged)
    Q_PROPERTY(bool won READ won NOTIFY stateChanged)
    Q_PROPERTY(bool lost READ lost NOTIFY stateChanged)
    Q_PROPERTY(bool paused READ paused WRITE setPaused NOTIFY clockChanged)
    Q_PROPERTY(int speed READ speed WRITE setSpeed NOTIFY clockChanged)
    Q_PROPERTY(int maxSpeed READ maxSpeed CONSTANT)

public:
    explicit GameSession(QObject *parent = nullptr);
    ~GameSession() override;

    /// Starts a game with a seed of its own. A map that cannot be loaded leaves no game and an
    /// error.
    Q_INVOKABLE void startNewGame(const QString &fileName);
    /// The same map again.
    Q_INVOKABLE void restart();
    /// Puts a tower of the selected type on a cell, true if the game took the command.
    Q_INVOKABLE bool buildTower(int x, int y);
    /// Runs the ticks that are due. Long frames count as short ones, the game does not jump.
    Q_INVOKABLE void advanceTime(qint64 microseconds);

    void startGame(const QString &fileName, quint64 seed);

    [[nodiscard]] const QString &error() const;
    [[nodiscard]] QSize gridSize() const;
    /// What can be built, as maps with the name and the cost of each type.
    [[nodiscard]] QVariantList towerTypes() const;
    [[nodiscard]] int selectedTower() const;
    [[nodiscard]] int towerCount() const;
    /// The chessboard of the map, nothing without a game.
    [[nodiscard]] const core::Grid *grid() const;
    /// All cells that towers take, as QPoint.
    [[nodiscard]] QVariantList towerCells() const;
    [[nodiscard]] qint64 gold() const;
    [[nodiscard]] qint64 lives() const;
    [[nodiscard]] qint64 tick() const;
    [[nodiscard]] int enemyCount() const;
    [[nodiscard]] bool won() const;
    [[nodiscard]] bool lost() const;
    [[nodiscard]] bool paused() const;
    [[nodiscard]] int speed() const;
    [[nodiscard]] int maxSpeed() const;
    /// Where enemies are drawn: between their last two ticks, as the display runs faster than the
    /// game. So the picture is up to a tick behind the game.
    [[nodiscard]] std::vector<QPointF> enemyPlaces() const;

    /// A game in the background must not go on without its player.
    void applicationStateChanged(Qt::ApplicationState state);

    void setSelectedTower(int index);
    void setPaused(bool paused);
    void setSpeed(int speed);

signals:
    void mapChanged();
    void selectionChanged();
    void towersChanged();
    void stateChanged();
    void clockChanged();
    /// Time went on, enemies are somewhere else on the screen.
    void frameChanged();

private:
    std::unique_ptr<core::Simulation> game_;
    core::TickClock clock_;
    std::map<qint64, core::Point> lastPlaces_; // of the enemies, by id, one tick ago
    QString fileName_;
    QString error_;
    int selectedTower_ = 0;
    bool paused_ = false;
    int speed_ = 1;
};

} // namespace qtd::client
