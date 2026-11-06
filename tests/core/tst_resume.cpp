#include "core/replay.h"
#include "scenario.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;

using qtd::core::BuildTowerCommand;
using qtd::core::CommandResult;
using qtd::core::Replay;
using qtd::core::ReplayRecorder;
using qtd::core::resumeGame;
using qtd::core::ResumeResult;
using qtd::tests::contentOf;
using qtd::tests::scenarioJson;

namespace {

/// A game in the middle: one tower that stands and one command that failed.
ReplayRecorder runningGame()
{
    ReplayRecorder game(contentOf(scenarioJson()), u"map.json"_s, 7);
    game.advance(2);
    game.execute(BuildTowerCommand{{5, 1}, 0});
    game.execute(BuildTowerCommand{{5, 1}, 0});
    game.advance(35);
    return game;
}

void write(const QString &fileName, const QByteArray &data)
{
    QFile file(fileName);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(data);
}

} // namespace

class ResumeTest : public QObject
{
    Q_OBJECT

private slots:
    static void aResumedGameStandsWhereItWasSaved()
    {
        const ReplayRecorder game = runningGame();
        const ResumeResult resumed = resumeGame(contentOf(scenarioJson()), game.replay());
        QCOMPARE(resumed.error, QString());
        QCOMPARE(resumed.game->simulation().state().tick, game.simulation().state().tick);
        QCOMPARE(resumed.game->simulation().hash(), game.simulation().hash());
    }

    static void aResumedGameRecordsTheWholeGame()
    {
        const Replay saved = runningGame().replay();
        const ResumeResult resumed = resumeGame(contentOf(scenarioJson()), saved);
        QCOMPARE(toJson(resumed.game->replay()), toJson(saved));
    }

    static void aResumedGameGoesOnLikeTheOriginal()
    {
        ReplayRecorder game = runningGame();
        const ResumeResult resumed = resumeGame(contentOf(scenarioJson()), game.replay());
        game.advance(10);
        resumed.game->advance(10);
        QCOMPARE(resumed.game->simulation().hash(), game.simulation().hash());
        QCOMPARE(toJson(resumed.game->replay()), toJson(game.replay()));
    }

    static void aGameOfOtherContentIsRefused()
    {
        const Replay saved = runningGame().replay();
        const ResumeResult resumed =
            resumeGame(contentOf(qtd::tests::editedJson(R"("cost": 40)", R"("cost": 41)")), saved);
        QVERIFY(resumed.game == nullptr);
        QVERIFY2(resumed.error.contains(u"other content"_s), qPrintable(resumed.error));
    }

    static void aDifferentEndIsRefused()
    {
        Replay saved = runningGame().replay();
        ++saved.checkpoints.back().hash;
        const ResumeResult resumed = resumeGame(contentOf(scenarioJson()), saved);
        QVERIFY(resumed.game == nullptr);
        QVERIFY2(resumed.error.contains(u"does not repeat"_s), qPrintable(resumed.error));
    }

    static void aDifferentResultIsRefused()
    {
        Replay saved = runningGame().replay();
        saved.commands.back().result = CommandResult::Accepted;
        const ResumeResult resumed = resumeGame(contentOf(scenarioJson()), saved);
        QVERIFY(resumed.game == nullptr);
        QVERIFY2(resumed.error.contains(u"does not repeat"_s), qPrintable(resumed.error));
    }

    static void aGameComesBackFromItsFiles()
    {
        const QTemporaryDir folder;
        const QString map = folder.filePath(u"map.json"_s);
        const QString save = folder.filePath(u"save.json"_s);
        const ReplayRecorder game = runningGame();
        write(map, scenarioJson());
        write(save, toJson(game.replay()));
        const ResumeResult resumed = resumeGame(map, save);
        QCOMPARE(resumed.error, QString());
        QCOMPARE(resumed.game->simulation().hash(), game.simulation().hash());
    }

    static void brokenFilesAreNamed()
    {
        const QTemporaryDir folder;
        const QString map = folder.filePath(u"map.json"_s);
        const QString save = folder.filePath(u"save.json"_s);
        QVERIFY(resumeGame(map, save).error.startsWith(map));
        write(map, scenarioJson());
        QVERIFY(resumeGame(map, save).error.startsWith(save));
        Replay broken = runningGame().replay();
        ++broken.checkpoints.back().hash;
        write(save, toJson(broken));
        const ResumeResult resumed = resumeGame(map, save);
        QVERIFY(resumed.game == nullptr);
        QVERIFY(resumed.error.startsWith(save));
    }
};

QTEST_APPLESS_MAIN(ResumeTest)
#include "tst_resume.moc"
