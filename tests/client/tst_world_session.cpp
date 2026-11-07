#include "client/landscape.h"
#include "client/world_session.h"
#include "core/content_loader.h"

#include <QFile>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QVariantMap>

#include <optional>

using namespace Qt::StringLiterals;
using qtd::client::WorldSession;

namespace {

bool hasWater(const qtd::core::Grid &grid)
{
    for (qint64 y = 0; y < grid.height(); ++y) {
        for (qint64 x = 0; x < grid.width(); ++x) {
            if (grid.terrainAt({x, y}) == qtd::core::Terrain::Water) {
                return true;
            }
        }
    }
    return false;
}

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
        QVERIFY(sectionOf(second, 4)[u"playable"_s].toBool());
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

    // The endless game of a section opens with its win and keeps the most rounds (E-36).
    static void theMostRoundsSurviveANewSession()
    {
        const QTemporaryDir folder;
        const QString file = folder.filePath(u"progress.json"_s);
        WorldSession first;
        first.setProgressFile(file);
        QVERIFY(!sectionOf(first, 0)[u"endlessOpen"_s].toBool());
        QVERIFY(first.recordGame(0, 1));
        QVERIFY(sectionOf(first, 0)[u"endlessOpen"_s].toBool());
        QVERIFY(!sectionOf(first, 1)[u"endlessOpen"_s].toBool());
        QVERIFY(first.recordRounds(0, 6));
        QVERIFY(!first.recordRounds(0, 4));
        QVERIFY(!first.recordRounds(0, 6));
        QVERIFY(!first.recordRounds(9, 8));
        WorldSession second;
        second.setProgressFile(file);
        QCOMPARE(sectionOf(second, 0)[u"best"_s].toInt(), 6);
        QCOMPARE(sectionOf(second, 1)[u"best"_s].toInt(), 0);
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

    // A swamp on the world map is a swamp in the game (E-32).
    static void everySectionPlaysInItsLandscape_data()
    {
        QTest::addColumn<int>("section");
        const WorldSession world;
        for (int section = 0; section < world.sections().size(); ++section) {
            QTest::newRow(qPrintable(sectionOf(world, section)[u"id"_s].toString())) << section;
        }
    }

    static void everySectionPlaysInItsLandscape()
    {
        QFETCH(const int, section);
        const WorldSession world;
        QVERIFY(QFile::exists(world.mapFile(section))); // in the resources of the game
        const qtd::client::Landscape inGame = qtd::client::landscapeOfMap(world.mapFile(section));
        const qtd::client::Landscape onMap =
            qtd::client::landscapeNamed(sectionOf(world, section)[u"landscape"_s].toString());
        QCOMPARE(inGame.grass, onMap.grass);
        QCOMPARE(inGame.treeModel, onMap.treeModel);
        // Ponds on the board need water in the landscape, else they are empty pits.
        const std::optional<qtd::core::Content> content =
            qtd::core::loadContentFile(world.mapFile(section)).content;
        if (!content) {
            QFAIL("the map does not load");
            return;
        }
        QVERIFY(!hasWater(content->grid) || inGame.water.isValid());
    }

    static void aMapLiesNextToTheWorld()
    {
        const WorldSession world;
        QCOMPARE(world.mapFile(0), u":/content/training_ground.json"_s);
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
