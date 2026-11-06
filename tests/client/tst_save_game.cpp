#include "client/game_session.h"

#include <QFile>
#include <QGuiApplication>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;

using qtd::client::GameSession;

namespace {

QString mapFile()
{
    return QStringLiteral(QTD_CONTENT_DIR "/training_ground.json");
}

quint64 hashOf(GameSession &game)
{
    return game.recorder()->simulation().hash();
}

} // namespace

class SaveGameTest : public QObject
{
    Q_OBJECT

private slots:
    void aGameComesBackPaused()
    {
        const QTemporaryDir folder;
        GameSession game;
        game.setSaveFile(folder.filePath(u"deep/down/save.json"_s));
        QVERIFY(!game.hasSave());
        game.startGame(mapFile(), 7);
        QVERIFY(game.buildTower(4, 4));
        game.recorder()->advance(100);
        const quint64 saved = hashOf(game);
        const QSignalSpy changes(&game, &GameSession::saveChanged);
        QVERIFY(game.saveGame());
        QCOMPARE(changes.size(), 1);
        QVERIFY(game.hasSave());

        GameSession other;
        other.setSaveFile(game.saveFile());
        other.startGame(mapFile(), 8);
        QVERIFY(other.loadGame());
        QCOMPARE(hashOf(other), saved);
        QCOMPARE(other.towerCount(), 1);
        QCOMPARE(other.tick(), 100);
        QVERIFY(other.paused());
        QCOMPARE(other.saveError(), QString());
    }

    void aLoadedGameCanBeSavedAndRestarted()
    {
        const QTemporaryDir folder;
        GameSession game;
        game.setSaveFile(folder.filePath(u"save.json"_s));
        game.startGame(mapFile(), 7);
        game.recorder()->advance(10);
        game.saveGame();
        QVERIFY(game.loadGame());
        game.recorder()->advance(10);
        QVERIFY(game.saveGame());
        QVERIFY(game.loadGame());
        QCOMPARE(game.tick(), 20);
        game.restart();
        QCOMPARE(game.tick(), 0);
    }

    void withoutAFileNothingIsSaved()
    {
        GameSession game;
        game.startGame(mapFile(), 7);
        QVERIFY(!game.canSave());
        QVERIFY(!game.saveGame());
        QVERIFY(!game.hasSave());
    }

    void aGameThatHasEndedIsNotSaved()
    {
        const QTemporaryDir folder;
        GameSession game;
        game.setSaveFile(folder.filePath(u"save.json"_s));
        game.startGame(mapFile(), 7);
        QVERIFY(game.canSave());
        game.recorder()->advance(100'000);
        QVERIFY(game.lost());
        QVERIFY(!game.canSave());
        QVERIFY(!game.saveGame());
    }

    void aBrokenSaveKeepsTheGame()
    {
        const QTemporaryDir folder;
        GameSession game;
        game.setSaveFile(folder.filePath(u"save.json"_s));
        game.startGame(mapFile(), 7);
        game.recorder()->advance(5);
        QVERIFY(!game.loadGame()); // no file yet
        QVERIFY(game.saveError().startsWith(game.saveFile()));
        QFile file(game.saveFile());
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("{");
        file.close();
        QVERIFY(!game.loadGame());
        QVERIFY(!game.saveError().isEmpty());
        QCOMPARE(game.tick(), 5);
        QVERIFY(!game.paused());
    }

    void aSaveThatCannotBeWrittenSaysSo()
    {
        const QTemporaryDir folder;
        GameSession game;
        game.setSaveFile(folder.path()); // a folder is no file
        game.startGame(mapFile(), 7);
        QVERIFY(!game.saveGame());
        QVERIFY(!game.saveError().isEmpty());
    }

    void aSaveOnlyNamesItsMapInItsFolder()
    {
        const QTemporaryDir folder;
        GameSession game;
        game.setSaveFile(folder.filePath(u"save.json"_s));
        game.startGame(mapFile(), 7);
        game.saveGame();
        QFile file(game.saveFile());
        QVERIFY(file.open(QIODevice::ReadOnly));
        QByteArray text = file.readAll();
        file.close();
        text.replace("\"training_ground.json\"", "\"../../elsewhere/mire_siege.json\"");
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(text);
        file.close();
        // The other map lies beside the first, and its content differs.
        QVERIFY(!game.loadGame());
        QVERIFY2(game.saveError().contains(u"other content"_s), qPrintable(game.saveError()));
    }

    void goingToTheBackgroundSavesAndPauses()
    {
        const QTemporaryDir folder;
        GameSession game;
        game.setSaveFile(folder.filePath(u"save.json"_s));
        game.startGame(mapFile(), 7);
        game.applicationStateChanged(Qt::ApplicationSuspended);
        QVERIFY(game.paused());
        QVERIFY(game.hasSave());
    }

    // The program starts a new game each time: leaving it must not eat the game that was saved.
    void goingToTheBackgroundKeepsTheSaveOfAnotherGame()
    {
        const QTemporaryDir folder;
        GameSession game;
        game.setSaveFile(folder.filePath(u"save.json"_s));
        game.startGame(mapFile(), 7);
        game.recorder()->advance(50);
        QVERIFY(game.saveGame());

        game.restart();
        game.applicationStateChanged(Qt::ApplicationHidden);
        QVERIFY(game.loadGame());
        QCOMPARE(game.tick(), 50);

        game.recorder()->advance(10); // the loaded game is that game
        game.applicationStateChanged(Qt::ApplicationHidden);
        game.restart();
        QVERIFY(game.loadGame());
        QCOMPARE(game.tick(), 60);
    }

    void aNewGameForgetsTheLastError()
    {
        const QTemporaryDir folder;
        GameSession game;
        game.setSaveFile(folder.filePath(u"save.json"_s));
        game.startGame(mapFile(), 7);
        QVERIFY(!game.loadGame());
        game.restart();
        QCOMPARE(game.saveError(), QString());
    }

    void aWaveIsCalledForItsBonus()
    {
        GameSession game;
        QVERIFY(!game.canCallWave());
        QVERIFY(!game.callWave());
        QCOMPARE(game.waveCallBonus(), 0);
        game.startGame(mapFile(), 7);
        QVERIFY(game.canCallWave());
        QCOMPARE(game.waveCallBonus(), 4); // 60 ticks, 15 per gold
        const QSignalSpy state(&game, &GameSession::stateChanged);
        QVERIFY(game.callWave());
        QCOMPARE(state.size(), 1);
        QCOMPARE(game.gold(), 124);
        QVERIFY(!game.canCallWave());
        QVERIFY(!game.callWave());
    }

    // The clock decides how fast ticks run, never what they do.
    void theSpeedDoesNotChangeTheGame()
    {
        GameSession slow;
        GameSession fast;
        for (GameSession *game : {&slow, &fast}) {
            game->startGame(mapFile(), 7);
            QVERIFY(game->buildTower(4, 4));
        }
        fast.setSpeed(3);
        for (int frame = 0; frame < 150; ++frame) {
            slow.advanceTime(100'000);
        }
        for (int frame = 0; frame < 50; ++frame) {
            fast.advanceTime(100'000);
        }
        QCOMPARE(slow.tick(), 450);
        QCOMPARE(fast.tick(), 450);
        QCOMPARE(hashOf(fast), hashOf(slow));
    }
};

QTEST_MAIN(SaveGameTest)
#include "tst_save_game.moc"
