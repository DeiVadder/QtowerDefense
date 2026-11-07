#include "client/world_session.h"

#include <QFile>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QVariantMap>

using namespace Qt::StringLiterals;
using qtd::client::WorldSession;

namespace {

QVariantMap sectionOf(const WorldSession &world, int section)
{
    return world.sections().at(section).toMap();
}

} // namespace

class TestWorldSession : public QObject
{
    Q_OBJECT

private slots:
    static void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true); // the progress of a player stays as it is
    }

    static void theShippedWorldStartsWithTheFirstSection()
    {
        const QTemporaryDir folder;
        WorldSession world;
        world.setProgressFile(folder.filePath(u"progress.json"_s));
        QVERIFY(world.error().isEmpty());
        QCOMPARE(world.sections().size(), 9);
        QCOMPARE(world.links().size(), 9);
        QCOMPARE(world.selected(), 0);
        QVERIFY(sectionOf(world, 0)[u"playable"_s].toBool());
        QVERIFY(!sectionOf(world, 1)[u"open"_s].toBool());
        QCOMPARE(sectionOf(world, 0)[u"landmarks"_s].toList().size(), 3);
    }

    // A new session on the same file, as after a start of the program.
    static void progressSurvivesANewSession()
    {
        const QTemporaryDir folder;
        const QString file = folder.filePath(u"deeper/progress.json"_s);
        WorldSession first;
        first.setProgressFile(file);
        QVERIFY(first.recordGame(0, 2));
        QCOMPARE(first.selected(), 1); // the way goes on where the fog has lifted
        WorldSession second;
        second.setProgressFile(file);
        QCOMPARE(sectionOf(second, 0)[u"stars"_s].toInt(), 2);
        QVERIFY(sectionOf(second, 1)[u"playable"_s].toBool());
        QVERIFY(sectionOf(second, 4)[u"open"_s].toBool());
        QVERIFY(!sectionOf(second, 4)[u"playable"_s].toBool()); // its map is not made yet
        QVERIFY(!sectionOf(second, 2)[u"open"_s].toBool());
        QCOMPARE(second.selected(), 1);
    }

    static void onlyAWonGameOfAKnownSectionCounts_data()
    {
        QTest::addColumn<int>("section");
        QTest::addColumn<int>("stars");
        QTest::newRow("no stars") << 0 << 0;
        QTest::newRow("before the first") << -1 << 2;
        QTest::newRow("after the last") << 9 << 2;
    }

    static void onlyAWonGameOfAKnownSectionCounts()
    {
        QFETCH(const int, section);
        QFETCH(const int, stars);
        const QTemporaryDir folder;
        WorldSession world;
        world.setProgressFile(folder.filePath(u"progress.json"_s));
        QVERIFY(!world.recordGame(section, stars));
        QVERIFY(!QFile::exists(world.progressFile()));
    }

    static void starsStayWhenTheFileCannotBeWritten()
    {
        const QTemporaryDir folder;
        QFile blocker(folder.filePath(u"file"_s));
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        blocker.close();
        WorldSession world;
        world.setProgressFile(folder.filePath(u"file/progress.json"_s));
        QTest::ignoreMessage(QtWarningMsg,
                             QRegularExpression(u"^The progress could not be saved"_s));
        QVERIFY(!world.recordGame(0, 1));
        QCOMPARE(sectionOf(world, 0)[u"stars"_s].toInt(), 1);
    }

    static void aMapLiesNextToTheWorld()
    {
        const WorldSession world;
        QCOMPARE(world.mapFile(0), u":/content/training_ground.json"_s);
        QVERIFY(world.mapFile(2).isEmpty());
        QVERIFY(world.mapFile(-1).isEmpty());
        QVERIFY(world.mapFile(9).isEmpty());
    }

    static void aBrokenWorldIsAnErrorWithoutSections()
    {
        WorldSession world;
        world.setWorldFile(u":/content/landscapes.json"_s);
        QVERIFY(!world.error().isEmpty());
        QVERIFY(world.sections().isEmpty());
        QVERIFY(world.mapFile(0).isEmpty());
    }

    static void selectionStaysOnKnownSections()
    {
        WorldSession world;
        world.setSelected(3);
        QCOMPARE(world.selected(), 3);
        world.setSelected(9);
        world.setSelected(-1);
        QCOMPARE(world.selected(), 3);
    }
};

QTEST_GUILESS_MAIN(TestWorldSession)
#include "tst_world_session.moc"
