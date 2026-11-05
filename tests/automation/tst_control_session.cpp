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
