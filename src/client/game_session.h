#pragma once

#include "core/replay.h"
#include "core/simulation.h"
#include "core/tick_clock.h"

#include <QObject>
#include <QPoint>
#include <QPointF>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <map>
#include <memory>
#include <set>
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
    Q_PROPERTY(QStringList towerModels READ towerModels NOTIFY mapChanged)
    Q_PROPERTY(QStringList enemyModels READ enemyModels NOTIFY mapChanged)
    Q_PROPERTY(QVariantList hand READ hand NOTIFY handChanged)
    Q_PROPERTY(int handLimit READ handLimit CONSTANT)
    Q_PROPERTY(int rollCost READ rollCost NOTIFY mapChanged)
    Q_PROPERTY(int chosenCard READ chosenCard NOTIFY selectionChanged)
    Q_PROPERTY(int selectedTower READ selectedTower WRITE setSelectedTower NOTIFY selectionChanged)
    Q_PROPERTY(QVariantList selectedShape READ selectedShape NOTIFY selectionChanged)
    Q_PROPERTY(QStringList spellNames READ spellNames NOTIFY mapChanged)
    Q_PROPERTY(QVariantList spellSeconds READ spellSeconds NOTIFY stateChanged)
    Q_PROPERTY(int selectedSpell READ selectedSpell NOTIFY selectionChanged)
    Q_PROPERTY(bool canCallWave READ canCallWave NOTIFY stateChanged)
    Q_PROPERTY(qint64 waveCallBonus READ waveCallBonus NOTIFY stateChanged)
    Q_PROPERTY(QString saveFile READ saveFile WRITE setSaveFile NOTIFY saveChanged)
    Q_PROPERTY(bool canSave READ canSave NOTIFY stateChanged)
    Q_PROPERTY(bool hasSave READ hasSave NOTIFY saveChanged)
    Q_PROPERTY(QString saveError READ saveError NOTIFY saveChanged)
    Q_PROPERTY(int chosenPriority READ chosenPriority NOTIFY towersChanged)
    Q_PROPERTY(qint64 chosenUpgradeCost READ chosenUpgradeCost NOTIFY towersChanged)
    Q_PROPERTY(qint64 chosenSellValue READ chosenSellValue NOTIFY towersChanged)
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
    Q_PROPERTY(int ticksPerSecond READ ticksPerSecond CONSTANT)

public:
    /// Something a player should hear.
    enum class Cue : quint8 {
        Build,
        Upgrade,
        Sell,
        Spell,
        Wave,
        Won,
        Lost,
        Roll,
        Coin,
    };
    Q_ENUM(Cue)

    explicit GameSession(QObject *parent = nullptr);
    [[nodiscard]] static int ticksPerSecond();
    ~GameSession() override;

    /// Starts a game with a seed of its own. A map that cannot be loaded leaves no game and an
    /// error.
    Q_INVOKABLE void startNewGame(const QString &fileName);
    /// The same map again.
    Q_INVOKABLE void restart();
    /// Puts a tower of the selected type on a cell, true if the game took the command.
    Q_INVOKABLE bool buildTower(int x, int y);
    /// Chooses the tower that takes this cell, true if there is one.
    Q_INVOKABLE bool chooseTowerAt(int x, int y);
    Q_INVOKABLE void clearChosenTower();
    /// Tells the chosen tower what to aim at, as an index into core::TargetPriority.
    Q_INVOKABLE void setChosenPriority(int priority);
    /// Takes the chosen tower to its next level, true if the game took the command.
    Q_INVOKABLE bool upgradeChosenTower();
    /// Sells the chosen tower, true if the game took the command. Then no tower is chosen.
    Q_INVOKABLE bool sellChosenTower();
    /// Makes the next tap on the map cast this spell, once it has cooled down. Asking for the same
    /// spell again takes that back.
    Q_INVOKABLE void selectSpell(int index);
    /// Casts the selected spell at a cell, true if the game took the command. The next tap is a
    /// tap as usual again.
    Q_INVOKABLE bool castSpellAt(int x, int y);
    /// Lets the next wave come now, for gold. True if the game took the command.
    Q_INVOKABLE bool callWave();
    /// Writes the running game to the save file. A failure shows in saveError.
    Q_INVOKABLE bool saveGame();
    /// Goes on with the save file, paused, on the map it names. The maps lie side by side. A
    /// failure shows in saveError and keeps the current game.
    Q_INVOKABLE bool loadGame();
    /// Makes the card in this slot the next thing a tap on the map builds, or none at all.
    Q_INVOKABLE void chooseCard(int slot);
    /// A random card for the price of a roll. True if the game took the command.
    Q_INVOKABLE bool rollCard();
    /// The card of this tower type at its full price. True if the game took the command.
    Q_INVOKABLE bool buyCard(int towerType);
    /// Throws the chosen card away, its gold is gone. True if there was one.
    Q_INVOKABLE bool discardCard();
    /// Takes a bonus coin from the map. True if it still lay there.
    Q_INVOKABLE bool collectCoin(int coin);
    /// Turns what a tap on the map builds by a quarter, clockwise.
    Q_INVOKABLE void rotateSelection();
    /// Runs the ticks that are due. Long frames count as short ones, the game does not jump.
    Q_INVOKABLE void advanceTime(qint64 microseconds);

    void startGame(const QString &fileName, quint64 seed);
    /// Goes on with a saved game, paused. Gives the reason if it cannot and keeps the current game.
    QString resumeGame(const QString &fileName, const QString &saveFile);

    [[nodiscard]] const QString &error() const;
    /// By type: the name of a model in the resources of the client, empty if it has none.
    [[nodiscard]] QStringList towerModels() const;
    [[nodiscard]] QStringList enemyModels() const;
    [[nodiscard]] bool canCallWave() const;
    [[nodiscard]] qint64 waveCallBonus() const;
    [[nodiscard]] const QString &saveFile() const;
    void setSaveFile(const QString &fileName);
    [[nodiscard]] bool canSave() const;
    [[nodiscard]] bool hasSave() const;
    [[nodiscard]] const QString &saveError() const;
    [[nodiscard]] QSize gridSize() const;
    /// What can be built, as maps with the name and the cost of each type.
    [[nodiscard]] QVariantList towerTypes() const;
    /// The tower type of each card, in the order of the hand.
    [[nodiscard]] QVariantList hand() const;
    [[nodiscard]] static int handLimit();
    [[nodiscard]] int rollCost() const;
    [[nodiscard]] int chosenCard() const;
    [[nodiscard]] int selectedTower() const;
    [[nodiscard]] QStringList spellNames() const;
    /// The seconds until each spell can be cast, rounded up, 0 for a spell that is ready.
    [[nodiscard]] QVariantList spellSeconds() const;
    /// The spell that the next tap casts, -1 if there is none.
    [[nodiscard]] int selectedSpell() const;
    /// The cells of what a tap builds, turned as it would stand, as maps with x and y from 0 and
    /// anchor for the cell that the tap names.
    [[nodiscard]] QVariantList selectedShape() const;
    /// What the chosen tower aims at, as an index into core::TargetPriority, -1 if none is chosen.
    [[nodiscard]] int chosenPriority() const;
    /// The gold for the next level of the chosen tower, -1 if there is none or no tower is chosen.
    [[nodiscard]] qint64 chosenUpgradeCost() const;
    /// The gold that selling the chosen tower brings, -1 if none is chosen.
    [[nodiscard]] qint64 chosenSellValue() const;
    /// The id of the chosen tower, 0 if none is chosen.
    [[nodiscard]] qint64 chosenTower() const;
    [[nodiscard]] int towerCount() const;
    /// The chessboard of the map, nothing without a game.
    [[nodiscard]] const core::Grid *grid() const;
    /// The file of the running map.
    [[nodiscard]] const QString &mapFile() const;
    /// The game to look at, nothing without a game.
    [[nodiscard]] const core::Simulation *simulation() const;
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

    /// The current game, for the control interface, nothing without a game.
    [[nodiscard]] core::ReplayRecorder *recorder();
    /// Shows a game that someone else has changed through recorder().
    void refresh();

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
    void saveChanged();
    void handChanged();
    /// A roll brought this card, it is in the hand already.
    void cardRolled(int towerType);
    /// No card for now: the hand is full, or else the gold is too little.
    void handRefused(bool full);
    /// In tiles of the map.
    void coinDropped(int coin, double x, double y, int value);
    void coinGone(int coin);
    void cue(qtd::client::GameSession::Cue what);
    void towerBuilt(int x, int y);
    /// A spell has hit this cell, in map units around it.
    void spellCast(int x, int y, double radius, int spell);
    /// Time went on, enemies are somewhere else on the screen.
    void frameChanged();

private:
    /// Starts to show the game that was just put in place.
    void showNewGame();
    /// The tower that is chosen, nothing if there is none, or no longer.
    [[nodiscard]] const core::Tower *chosen() const;
    /// Gives a command on the chosen tower and shows what it changed.
    bool executeOnChosen(const core::Command &command);
    /// Gives a command on the hand and shows what it changed, or why it could not be.
    bool executeOnHand(const core::Command &command);
    void forgetCard();
    /// Tells of the coins that came and went since the last time.
    void reportCoins();

    std::unique_ptr<core::ReplayRecorder> game_;
    core::TickClock clock_;
    std::map<qint64, core::Point> lastPlaces_; // of the enemies, by id, one tick ago
    QString fileName_;
    QString error_;
    QString saveFile_;
    QString saveError_;
    bool saveIsOurs_ = false; // the save file holds this game, not one the player may want back
    qint64 chosenTower_ = 0;
    int selectedTower_ = 0;
    int chosenCard_ = -1;
    std::set<qint64> shownCoins_;
    int selectedSpell_ = -1;
    core::Rotation selectedRotation_ = core::Rotation::None;
    bool paused_ = false;
    int speed_ = 1;
};

} // namespace qtd::client
