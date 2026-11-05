#include "../core/scenario.h"
#include "client/game_session.h"
#include "client/graphics.h"
#include "client/screen_control.h"
#include "core/replay.h"

#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLatin1StringView>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QtQml/qqmlextensionplugin.h>

#include <memory>

Q_IMPORT_QML_PLUGIN(QtowerDefenseClientPlugin)

using qtd::client::GameSession;
using qtd::client::ScreenControl;
using namespace Qt::StringLiterals;

namespace {

QJsonObject ask(ScreenControl *control, const QJsonObject &request)
{
    return QJsonDocument::fromJson(
               control->handle(QJsonDocument(request).toJson(QJsonDocument::Compact)))
        .object();
}

QJsonObject start(ScreenControl *control)
{
    return ask(control,
               {{"action"_L1, "startMap"_L1},
                {"map"_L1, QStringLiteral(QTD_CONTENT_DIR "/training_ground.json")},
                {"seed"_L1, "7"_L1}});
}

/// The real window with its game under control.
class ControlledWindow
{
public:
    ControlledWindow()
        : window_(load(&engine_)), game_(window_->findChild<GameSession *>()),
          control_(std::make_unique<ScreenControl>(game_, window_))
    {
    }

    [[nodiscard]] bool isOnScreen() const
    {
        return QTest::qWaitForWindowExposed(window_);
    }
    [[nodiscard]] GameSession *game() const
    {
        return game_;
    }
    [[nodiscard]] ScreenControl *control() const
    {
        return control_.get();
    }

private:
    static QQuickWindow *load(QQmlApplicationEngine *engine)
    {
        engine->loadFromModule("QtowerDefense.Client", "Main");
        return qobject_cast<QQuickWindow *>(engine->rootObjects().constFirst());
    }

    QQmlApplicationEngine engine_;
    QQuickWindow *window_;
    GameSession *game_;
    std::unique_ptr<ScreenControl> control_;
};

} // namespace

class TestScreenControl : public QObject
{
    Q_OBJECT

private slots:
    static void initTestCase()
    {
        qtd::client::configureGraphics();
    }

    static void requestsPlayTheGameOnTheScreen()
    {
        const ControlledWindow window;
        QVERIFY(window.isOnScreen());
        QVERIFY(start(window.control())["ok"_L1].toBool());
        QVERIFY(window.game()->paused());
        const QSignalSpy towers(window.game(), &GameSession::towersChanged);
        const QJsonObject built =
            ask(window.control(),
                {{"action"_L1, "buildTower"_L1}, {"x"_L1, 4}, {"y"_L1, 4}, {"towerType"_L1, 0}});
        QCOMPARE(built["result"_L1].toString(), u"accepted"_s);
        QVERIFY(!towers.isEmpty());
        QCOMPARE(window.game()->towerCount(), 1);
        QCOMPARE(window.game()->gold(), 70);
        const QJsonObject later =
            ask(window.control(), {{"action"_L1, "advance"_L1}, {"ticks"_L1, 100}});
        QCOMPARE(later["tick"_L1].toInteger(), 100);
        QCOMPARE(window.game()->tick(), 100);
    }

    // Real time on purpose: the question is whether the frames of the window move the game.
    static void aControlledGameRunsOnlyWhenItIsTold()
    {
        const ControlledWindow window;
        QVERIFY(window.isOnScreen());
        start(window.control());
        ScreenControl *control = window.control();
        const auto frames = [control] {
            return ask(control, {{"action"_L1, "observe"_L1}})["frames"_L1].toInteger();
        };
        const qint64 first = frames();
        QVERIFY(QTest::qWaitFor([&frames, first] { return frames() > first; }));
        QCOMPARE(window.game()->tick(), 0);
        QVERIFY(!ask(control, {{"action"_L1, "resume"_L1}})["paused"_L1].toBool());
        GameSession *game = window.game();
        QVERIFY(QTest::qWaitFor([game] { return game->tick() > 0; }));
        QVERIFY(ask(control, {{"action"_L1, "pause"_L1}})["paused"_L1].toBool());
    }

    static void aScreenshotIsAFileWithAPicture()
    {
        const QTemporaryDir folder;
        const ControlledWindow window;
        QVERIFY(window.isOnScreen());
        start(window.control());
        const QString file = folder.filePath(u"shot.png"_s);
        const QJsonObject answer =
            ask(window.control(), {{"action"_L1, "screenshot"_L1}, {"file"_L1, file}});
        QVERIFY(answer["ok"_L1].toBool());
        const QJsonObject picture = answer["screenshot"_L1].toObject();
        const QImage image(file);
        QCOMPARE(picture["width"_L1].toInt(), image.width());
        QCOMPARE(picture["height"_L1].toInt(), image.height());
        QVERIFY(picture["colors"_L1].toInt() > 8);
    }

    static void aBadScreenshotNameIsAnAnswer()
    {
        const ControlledWindow window;
        QVERIFY(window.isOnScreen());
        start(window.control());
        const QJsonObject answer =
            ask(window.control(), {{"action"_L1, "screenshot"_L1}, {"file"_L1, ""_L1}});
        QVERIFY(!answer["ok"_L1].toBool());
    }

    static void aControlledGameBeginsWithStartMap()
    {
        const ControlledWindow window;
        const QJsonObject answer = ask(window.control(), {{"action"_L1, "observe"_L1}});
        QVERIFY(!answer["ok"_L1].toBool());
        QVERIFY(answer["error"_L1].toString().contains(u"startMap"_s));
    }

    static void aFailedStartKeepsTheGame()
    {
        const ControlledWindow window;
        start(window.control());
        ask(window.control(), {{"action"_L1, "advance"_L1}, {"ticks"_L1, 5}});
        const QJsonObject answer =
            ask(window.control(),
                {{"action"_L1, "startMap"_L1}, {"map"_L1, "none.json"_L1}, {"seed"_L1, "7"_L1}});
        QVERIFY(!answer["ok"_L1].toBool());
        QCOMPARE(window.game()->tick(), 5);
    }

    static void theRecordingIsTheGameOfTheRequests()
    {
        const QTemporaryDir folder;
        const ControlledWindow window;
        start(window.control());
        ask(window.control(),
            {{"action"_L1, "buildTower"_L1}, {"x"_L1, 4}, {"y"_L1, 4}, {"towerType"_L1, 0}});
        const QJsonObject last =
            ask(window.control(), {{"action"_L1, "advance"_L1}, {"ticks"_L1, 90}});
        const QString file = folder.filePath(u"game.replay.json"_s);
        QVERIFY(ask(window.control(), {{"action"_L1, "saveReplay"_L1}, {"file"_L1, file}})["ok"_L1]
                    .toBool());
        const qtd::core::Replay replay =
            qtd::tests::required(qtd::core::loadReplayFile(file).replay);
        QCOMPARE(replay.content, u"training_ground.json"_s);
        QCOMPARE(replay.seed, 7U);
        QCOMPARE(replay.commands.size(), std::size_t{1});
        QCOMPARE(qtd::core::hashToText(replay.checkpoints.back().hash), last["hash"_L1].toString());
    }

    // The end of the program itself shows in scripts/smoke_test.py, it needs a running event loop.
    static void quitIsAnAnswer()
    {
        const ControlledWindow window;
        QVERIFY(ask(window.control(), {{"action"_L1, "quit"_L1}})["ok"_L1].toBool());
    }
};

QTEST_MAIN(TestScreenControl)
#include "tst_screen_control.moc"
