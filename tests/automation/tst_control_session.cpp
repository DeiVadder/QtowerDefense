#include "../core/scenario.h"
#include "automation/control_session.h"
#include "core/content_loader.h"
#include "core/replay.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

#include <memory>
#include <utility>

using qtd::automation::ControlSession;
using qtd::tests::required;

using namespace Qt::StringLiterals;

namespace {

QString mapFile()
{
    return QDir(QStringLiteral(QTD_CONTENT_DIR)).filePath(u"training_ground.json"_s);
}

QJsonObject ask(ControlSession &session, const QJsonObject &request)
{
    const QByteArray line = QJsonDocument(request).toJson(QJsonDocument::Compact);
    return QJsonDocument::fromJson(session.handle(line)).object();
}

QJsonObject start(ControlSession &session, const QString &seed = u"7"_s)
{
    return ask(session, {{"action"_L1, "startMap"_L1}, {"map"_L1, mapFile()}, {"seed"_L1, seed}});
}

QJsonObject build(ControlSession &session, int x, int y)
{
    return ask(session,
               {{"action"_L1, "buildTower"_L1}, {"x"_L1, x}, {"y"_L1, y}, {"towerType"_L1, 0}});
}

QJsonObject advance(ControlSession &session, qint64 ticks)
{
    return ask(session, {{"action"_L1, "advance"_L1}, {"ticks"_L1, ticks}});
}

} // namespace

/// Counts the requests that reach the game.
class CountingGame : public qtd::automation::ControlledGame
{
public:
    QString loadSaved(const QString & /*map*/, const QString & /*file*/) override
    {
        return {};
    }

    QString start(const QString &map, quint64 seed) override
    {
        qtd::core::ContentLoadResult loaded = qtd::core::loadContentFile(map);
        if (!loaded.content) {
            return loaded.error;
        }
        recorder_ =
            std::make_unique<qtd::core::ReplayRecorder>(*std::move(loaded.content), map, seed);
        return {};
    }
    qtd::core::ReplayRecorder *recorder() override
    {
        return recorder_.get();
    }
    QString setPaused(bool /*paused*/) override
    {
        ++calls;
        return {};
    }
    QString quit() override
    {
        ++calls;
        return {};
    }

    int calls = 0;

private:
    std::unique_ptr<qtd::core::ReplayRecorder> recorder_;
};

class ControlSessionTest : public QObject
{
    Q_OBJECT

private slots:
    void aMapStartsAGame()
    {
        ControlSession session;
        const QJsonObject answer = start(session);
        QVERIFY(answer["ok"_L1].toBool());
        QCOMPARE(answer["status"_L1].toString(), u"running"_s);
        QCOMPARE(answer["tick"_L1].toInteger(), 0);
        QCOMPARE(answer["gold"_L1].toInteger(), 120);
        QCOMPARE(answer["lives"_L1].toInteger(), 5);
        QCOMPARE(answer["hash"_L1].toString().size(), 16);
        const QJsonObject wave = answer["wave"_L1].toObject();
        QCOMPARE(wave["group"_L1].toInteger(), 0);
        QCOMPARE(wave["spawnedInGroup"_L1].toInteger(), 0);
        QCOMPARE(wave["countdownTicks"_L1].toInteger(), 60);
    }

    void aTowerCostsGoldAndShows()
    {
        ControlSession session;
        start(session);
        const QJsonObject answer = build(session, 6, 0);
        QCOMPARE(answer["result"_L1].toString(), u"accepted"_s);
        QCOMPARE(answer["gold"_L1].toInteger(), 70);
        const QJsonObject tower = answer["towers"_L1].toArray().first().toObject();
        QCOMPARE(tower["x"_L1].toInteger(), 6);
        QCOMPARE(tower["y"_L1].toInteger(), 0);
        QCOMPARE(tower["type"_L1].toInteger(), 0);
    }

    void aTowerStandsAsItWasTurned()
    {
        ControlSession session;
        start(session);
        const QByteArray request =
            R"({"action":"buildTower","x":6,"y":5,"towerType":0,"rotation":3})";
        const QJsonObject answer = QJsonDocument::fromJson(session.handle(request)).object();
        QCOMPARE(answer["result"_L1].toString(), u"accepted"_s);
        const QJsonObject tower = answer["towers"_L1].toArray().first().toObject();
        QCOMPARE(tower["rotation"_L1].toInteger(), 3);
    }

    void aTowerTakesAPriority()
    {
        ControlSession session;
        start(session);
        const qint64 id =
            build(session, 6, 0)["towers"_L1].toArray().first().toObject()["id"_L1].toInteger();
        const QJsonObject answer = ask(session,
                                       {{"action"_L1, "setTargetPriority"_L1},
                                        {"tower"_L1, id},
                                        {"priority"_L1, "strongest"_L1}});
        QCOMPARE(answer["result"_L1].toString(), u"accepted"_s);
        const QJsonObject tower = answer["towers"_L1].toArray().first().toObject();
        QCOMPARE(tower["priority"_L1].toString(), u"strongest"_s);
        const QJsonObject missing = ask(session,
                                        {{"action"_L1, "setTargetPriority"_L1},
                                         {"tower"_L1, id + 100},
                                         {"priority"_L1, "last"_L1}});
        QVERIFY(missing["ok"_L1].toBool());
        QCOMPARE(missing["result"_L1].toString(), u"unknownTower"_s);
    }

    void aWaveIsCalled()
    {
        ControlSession session;
        start(session);
        const QJsonObject before = advance(session, 0);
        QCOMPARE(before["wave"_L1].toObject()["callBonus"_L1].toInteger(), 4);
        const QJsonObject called = ask(session, {{"action"_L1, "callWave"_L1}});
        QCOMPARE(called["result"_L1].toString(), u"accepted"_s);
        QCOMPARE(called["gold"_L1].toInteger(), 124);
        QCOMPARE(called["wave"_L1].toObject()["callBonus"_L1].toInteger(), 0);
        const QJsonObject again = ask(session, {{"action"_L1, "callWave"_L1}});
        QCOMPARE(again["result"_L1].toString(), u"noWaveWaiting"_s);
    }

    void aSavedGameGoesOn()
    {
        const QTemporaryDir folder;
        const QString file = folder.filePath(u"save.json"_s);
        ControlSession session;
        start(session);
        advance(session, 40);
        const QJsonObject end = advance(session, 0);
        ask(session, {{"action"_L1, "saveReplay"_L1}, {"file"_L1, file}});

        ControlSession other; // needs no game before
        const QJsonObject loaded =
            ask(other, {{"action"_L1, "loadGame"_L1}, {"map"_L1, mapFile()}, {"file"_L1, file}});
        QCOMPARE(loaded["tick"_L1].toInteger(), 40);
        QCOMPARE(loaded["hash"_L1].toString(), end["hash"_L1].toString());
    }

    void aSaveThatDoesNotLoadKeepsTheGame()
    {
        ControlSession session;
        start(session);
        advance(session, 3);
        const QJsonObject answer = ask(
            session, {{"action"_L1, "loadGame"_L1}, {"map"_L1, mapFile()}, {"file"_L1, "none"_L1}});
        QVERIFY(answer.contains("error"_L1));
        QCOMPARE(advance(session, 0)["tick"_L1].toInteger(), 3);
    }

    void spellsAreCastAndCoolDown()
    {
        ControlSession session;
        start(session);
        const QJsonObject start = advance(session, 0);
        QCOMPARE(start["spells"_L1].toArray().size(), 2);
        const QJsonObject cast = ask(
            session, {{"action"_L1, "castSpell"_L1}, {"spell"_L1, 0}, {"x"_L1, 1}, {"y"_L1, 2}});
        QCOMPARE(cast["result"_L1].toString(), u"accepted"_s);
        const QJsonObject meteor = cast["spells"_L1].toArray().first().toObject();
        QCOMPARE(meteor["cooldownTicks"_L1].toInteger(), 600);
        const QJsonObject again = ask(
            session, {{"action"_L1, "castSpell"_L1}, {"spell"_L1, 0}, {"x"_L1, 1}, {"y"_L1, 2}});
        QCOMPARE(again["result"_L1].toString(), u"spellCooling"_s);
    }

    void towersAreUpgradedAndSold()
    {
        ControlSession session;
        start(session);
        ask(session,
            {{"action"_L1, "buildTower"_L1}, {"x"_L1, 6}, {"y"_L1, 3}, {"towerType"_L1, 0}});
        const QJsonObject upgraded =
            ask(session, {{"action"_L1, "upgradeTower"_L1}, {"tower"_L1, 1}});
        QCOMPARE(upgraded["result"_L1].toString(), u"accepted"_s);
        const QJsonObject tower = upgraded["towers"_L1].toArray().first().toObject();
        QCOMPARE(tower["level"_L1].toInteger(), 1);
        QCOMPARE(tower["sellValue"_L1].toInteger(), 63);
        const QJsonObject sold = ask(session, {{"action"_L1, "sellTower"_L1}, {"tower"_L1, 1}});
        QCOMPARE(sold["result"_L1].toString(), u"accepted"_s);
        QCOMPARE(sold["gold"_L1].toInteger(), 120 - 50 - 40 + 63);
        QVERIFY(sold["towers"_L1].toArray().isEmpty());
        const QJsonObject unknown =
            ask(session, {{"action"_L1, "upgradeTower"_L1}, {"tower"_L1, 1}});
        QCOMPARE(unknown["result"_L1].toString(), u"unknownTower"_s);
    }

    // Slow 0.4 against a resistance of 0.25, poison 0.05 without one.
    void statusEffectsShow()
    {
        ControlSession session;
        start(session);
        const auto buildAt = [&session](int x, int type) {
            return ask(session,
                       {{"action"_L1, "buildTower"_L1},
                        {"x"_L1, x},
                        {"y"_L1, 3},
                        {"towerType"_L1, type}})["result"_L1]
                .toString();
        };
        QCOMPARE(buildAt(6, 4), u"accepted"_s);
        QCOMPARE(buildAt(7, 5), u"accepted"_s);
        QJsonObject enemy;
        for (int tick = 0; tick < 500 && enemy["poison"_L1].toObject().isEmpty(); ++tick) {
            const QJsonObject first =
                advance(session, 1)["enemies"_L1].toArray().first().toObject();
            if (first["poison"_L1].toObject()["ticks"_L1].toInteger() > 0) {
                enemy = first;
            }
        }
        const QJsonObject slow = enemy["slow"_L1].toObject();
        const QJsonObject poison = enemy["poison"_L1].toObject();
        QCOMPARE(slow["strength"_L1].toString(), u"0.3"_s);
        QVERIFY(slow["ticks"_L1].toInteger() > 0);
        QCOMPARE(poison["strength"_L1].toString(), u"0.05"_s);
        QCOMPARE(poison["ticks"_L1].toInteger(), 90);
    }

    void aRequestWithAWrongKeyDoesNothing_data()
    {
        QTest::addColumn<QByteArray>("request");
        QTest::newRow("pause") << QByteArray(R"({"action":"pause","x":1})");
        QTest::newRow("resume") << QByteArray(R"({"action":"resume","x":1})");
        QTest::newRow("quit") << QByteArray(R"({"action":"quit","x":1})");
    }

    void aRequestWithAWrongKeyDoesNothing()
    {
        QFETCH(const QByteArray, request);
        CountingGame game;
        ControlSession session(&game);
        start(session);
        QVERIFY(!QJsonDocument::fromJson(session.handle(request)).object()["ok"_L1].toBool());
        QCOMPARE(game.calls, 0);
    }

    void theRealGameHasActionsOfItsOwn_data()
    {
        QTest::addColumn<QByteArray>("request");
        QTest::newRow("screenshot") << QByteArray(R"({"action":"screenshot","file":"a.png"})");
        QTest::newRow("pause") << QByteArray(R"({"action":"pause"})");
        QTest::newRow("resume") << QByteArray(R"({"action":"resume"})");
        QTest::newRow("quit") << QByteArray(R"({"action":"quit"})");
    }

    void theRealGameHasActionsOfItsOwn()
    {
        QFETCH(const QByteArray, request);
        ControlSession session;
        start(session);
        const QJsonObject answer = QJsonDocument::fromJson(session.handle(request)).object();
        QVERIFY(!answer["ok"_L1].toBool());
        QVERIFY(answer["error"_L1].toString().contains(u"real game"_s));
    }

    void aRejectedCommandIsAnAnswerAndChangesNothing()
    {
        ControlSession session;
        const QString hash = start(session)["hash"_L1].toString();
        const QJsonObject answer = build(session, 99, 99);
        QVERIFY(answer["ok"_L1].toBool());
        QCOMPARE(answer["result"_L1].toString(), u"outsideMap"_s);
        QCOMPARE(answer["hash"_L1].toString(), hash);
    }

    void ticksRunAndEnemiesShow()
    {
        ControlSession session;
        start(session);
        // delayTicks 60: the countdown takes the ticks 1 to 60, the first enemy comes at tick 61.
        QVERIFY(advance(session, 60)["enemies"_L1].toArray().isEmpty());
        const QJsonObject answer = advance(session, 1);
        QCOMPARE(answer["tick"_L1].toInteger(), 61);
        const QJsonObject enemy = answer["enemies"_L1].toArray().first().toObject();
        QCOMPARE(enemy["health"_L1].toString(), u"12"_s);
        QCOMPARE(enemy["x"_L1].toString(), u"0"_s);
        QCOMPARE(enemy["y"_L1].toString(), u"2"_s);
        QCOMPARE(answer["wave"_L1].toObject()["spawnedInGroup"_L1].toInteger(), 1);
    }

    void observeChangesNothing()
    {
        ControlSession session;
        const QJsonObject first = start(session);
        const QJsonObject answer = ask(session, {{"action"_L1, "observe"_L1}});
        QCOMPARE(answer, first);
    }

    void aControlledGameIsTheGameOfItsCommands()
    {
        ControlSession session;
        start(session);
        build(session, 4, 4);
        const QJsonObject answer = advance(session, 150);

        qtd::core::Simulation game(required(qtd::core::loadContentFile(mapFile()).content), 7);
        game.execute(qtd::core::BuildTowerCommand{{4, 4}, 0});
        game.advance(150);
        QCOMPARE(answer["hash"_L1].toString(), qtd::core::hashToText(game.hash()));
        QCOMPARE(answer["tick"_L1].toInteger(), 150);
    }

    void aStrangeFileNameIsAnAnswer_data()
    {
        QTest::addColumn<QString>("name");
        QTest::newRow("NUL") << u"a"_s + QChar::Null + u"b"_s;
        QTest::newRow("empty") << QString();
        QTest::newRow("too long") << QString(40'000, u'y');
    }

    void aStrangeFileNameIsAnAnswer()
    {
        QFETCH(const QString, name);
        ControlSession session;
        start(session);
        const QJsonObject answer =
            ask(session, {{"action"_L1, "saveReplay"_L1}, {"file"_L1, name}});
        QVERIFY(!answer["ok"_L1].toBool());
        QVERIFY(answer["error"_L1].toString().contains(u"file"_s));
    }

    void aGameBecomesAReplay()
    {
        const QTemporaryDir directory;
        const QString file = directory.filePath(u"game.replay.json"_s);
        ControlSession session;
        start(session);
        build(session, 4, 4);
        advance(session, 90);
        const QJsonObject answer =
            ask(session, {{"action"_L1, "saveReplay"_L1}, {"file"_L1, file}});
        QVERIFY(answer["ok"_L1].toBool());
        qtd::core::Replay replay = required(qtd::core::loadReplayFile(file).replay);
        QCOMPARE(replay.checkpoints.back().tick, 90);
        QCOMPARE(qtd::core::hashToText(replay.checkpoints.back().hash),
                 answer["hash"_L1].toString());
        const qtd::core::Content content = required(qtd::core::loadContentFile(mapFile()).content);
        QCOMPARE(qtd::core::verifyReplay(content, std::move(replay)), QString());
    }

    void wrongRequestsGiveAnAnswer_data()
    {
        QTest::addColumn<QByteArray>("request");
        QTest::addColumn<QString>("error");
        QTest::newRow("no JSON") << QByteArray("{") << u"invalid JSON"_s;
        QTest::newRow("no object") << QByteArray("[1]") << u"the top level must be an object"_s;
        QTest::newRow("no action") << QByteArray("{}") << u"action"_s;
        QTest::newRow("unknown") << QByteArray(R"({"action":"dance"})")
                                 << u"unknown action \"dance\""_s;
        QTest::newRow("fraction") << QByteArray(R"({"action":"advance","ticks":1.5})")
                                  << u"is no integer"_s;
        QTest::newRow("negative ticks")
            << QByteArray(R"({"action":"advance","ticks":-1})") << u"ticks"_s;
        QTest::newRow("too many ticks")
            << QByteArray(R"({"action":"advance","ticks":1000001})") << u"ticks"_s;
        QTest::newRow("no ticks") << QByteArray(R"({"action":"advance"})") << u"ticks"_s;
        QTest::newRow("text as cell")
            << QByteArray(R"({"action":"buildTower","x":"1","y":0,"towerType":0})") << u"x"_s;
        QTest::newRow("wave with a wrong key")
            << QByteArray(R"({"action":"callWave","z":1})") << u"z"_s;
        QTest::newRow("load without a file")
            << QByteArray(R"({"action":"loadGame","map":"m"})") << u"file"_s;
        QTest::newRow("load with a wrong key")
            << QByteArray(R"({"action":"loadGame","map":"m","file":"f","z":1})") << u"z"_s;
        QTest::newRow("spell without a place")
            << QByteArray(R"({"action":"castSpell","spell":0,"x":1})") << u"y"_s;
        QTest::newRow("spell with a wrong key")
            << QByteArray(R"({"action":"castSpell","spell":0,"x":1,"y":1,"z":1})") << u"z"_s;
        QTest::newRow("upgrade without a tower")
            << QByteArray(R"({"action":"upgradeTower"})") << u"tower"_s;
        QTest::newRow("sale with a wrong key")
            << QByteArray(R"({"action":"sellTower","tower":1,"x":1})") << u"x"_s;
        QTest::newRow("rotation too far")
            << QByteArray(R"({"action":"buildTower","x":1,"y":1,"towerType":0,"rotation":4})")
            << u"rotation"_s;
        QTest::newRow("negative rotation")
            << QByteArray(R"({"action":"buildTower","x":1,"y":1,"towerType":0,"rotation":-1})")
            << u"rotation"_s;
        QTest::newRow("unknown priority")
            << QByteArray(R"({"action":"setTargetPriority","tower":1,"priority":"weakest"})")
            << u"unknown target priority \"weakest\""_s;
        QTest::newRow("no tower") << QByteArray(
                                         R"({"action":"setTargetPriority","priority":"last"})")
                                  << u"tower"_s;
        QTest::newRow("no file") << QByteArray(R"({"action":"saveReplay"})") << u"file"_s;
        QTest::newRow("unknown key")
            << QByteArray(R"({"action":"observe","extra":1})") << u"extra"_s;
        QTest::newRow("unknown key of an action")
            << QByteArray(R"({"action":"advance","ticks":1,"extra":1})") << u"extra"_s;
        QTest::newRow("signed seed")
            << QByteArray(R"({"action":"startMap","map":"m","seed":"+7"})") << u"seed must be"_s;
        QTest::newRow("unwritable file")
            << QByteArray(R"({"action":"saveReplay","file":"no/such/folder/x"})")
            << u"no/such/folder/x"_s;
        QTest::newRow("bad seed") << QByteArray(R"({"action":"startMap","map":"m","seed":"x"})")
                                  << u"seed must be"_s;
        QTest::newRow("no map") << QByteArray(R"({"action":"startMap","map":"none","seed":"1"})")
                                << u"none"_s;
    }

    void wrongRequestsGiveAnAnswer()
    {
        QFETCH(const QByteArray, request);
        QFETCH(const QString, error);
        ControlSession session;
        start(session);
        const QByteArray answer = session.handle(request);
        const QJsonObject object = QJsonDocument::fromJson(answer).object();
        QVERIFY(!object["ok"_L1].toBool());
        QVERIFY2(object["error"_L1].toString().contains(error), answer.constData());
    }

    void nothingWorksBeforeAGame()
    {
        ControlSession session;
        const QJsonObject answer = advance(session, 1);
        QCOMPARE(answer["error"_L1].toString(), u"no game, use startMap first"_s);
    }

    void aFailedStartKeepsTheGame()
    {
        ControlSession session;
        start(session);
        build(session, 4, 4);
        session.handle(R"({"action":"startMap","map":"none","seed":"1"})");
        QCOMPARE(ask(session, {{"action"_L1, "observe"_L1}})["gold"_L1].toInteger(), 70);
    }
};

QTEST_APPLESS_MAIN(ControlSessionTest)
#include "tst_control_session.moc"
