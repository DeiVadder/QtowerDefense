#include "core/simulation.h"
#include "core/world.h"
#include "scenario.h"

#include <QByteArray>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QTest>

#include <algorithm>
#include <cstddef>
#include <vector>

using namespace Qt::StringLiterals;
using qtd::core::BuildTowerCommand;
using qtd::core::CampaignProgress;
using qtd::core::GameStatus;
using qtd::core::Simulation;
using qtd::core::World;
using qtd::tests::contentOf;
using qtd::tests::editedJson;
using qtd::tests::required;

namespace {

// A start with two ways that meet again: a - b - d and a - c - d.
QByteArray worldJson()
{
    return R"json({
    "formatVersion": 1,
    "start": "a",
    "sections": [
        { "id": "a", "name": "A", "landscape": "meadow", "map": "a.json", "x": 0, "y": 500 },
        { "id": "b", "name": "B", "landscape": "swamp", "x": 500, "y": 0 },
        { "id": "c", "name": "C", "landscape": "frost", "x": 500, "y": 1000 },
        { "id": "d", "name": "D", "landscape": "frost", "x": 1000, "y": 500 }
    ],
    "links": [
        { "from": "a", "to": "b" },
        { "from": "c", "to": "a" },
        { "from": "b", "to": "d" },
        { "from": "c", "to": "d" }
    ]
}
)json";
}

QByteArray edited(const QByteArray &from, const QByteArray &to)
{
    const QByteArray json = worldJson();
    return json.contains(from) ? QByteArray(json).replace(from, to) : QByteArray();
}

// Towers, enemies and spells of a shipped map.
QJsonArray catalogOf(const QString &map)
{
    QFile file(QStringLiteral(QTD_CONTENT_DIR "/") + map);
    const QJsonObject root = file.open(QIODevice::ReadOnly)
                                 ? QJsonDocument::fromJson(file.readAll()).object()
                                 : QJsonObject();
    return QJsonArray{root[u"towers"_s], root[u"enemies"_s], root[u"spells"_s]};
}

World testWorld()
{
    return required(qtd::core::parseWorld(worldJson()).world);
}

std::vector<bool> openSections(const World &world, const CampaignProgress &progress)
{
    std::vector<bool> open;
    open.reserve(world.sections.size());
    for (std::size_t section = 0; section < world.sections.size(); ++section) {
        open.push_back(qtd::core::isOpen(world, progress, section));
    }
    return open;
}

// The scenario sends two enemies that each cost a life when they get through.
qint64 starsAfter(int startLives, bool towers)
{
    const QByteArray json =
        editedJson("\"startLives\": 3", "\"startLives\": " + QByteArray::number(startLives));
    Simulation game(contentOf(json), 1);
    if (towers) {
        game.execute(BuildTowerCommand{{2, 1}, 0});
        game.execute(BuildTowerCommand{{4, 1}, 0});
    }
    while (game.state().status == GameStatus::Running) {
        game.tick();
    }
    const qint64 lost = towers ? 0 : 2;
    if (game.state().lives != std::max<qint64>(startLives - lost, 0)) {
        return -1; // the scenario no longer does what the test expects
    }
    return qtd::core::starsOfGame(game);
}

} // namespace

class TestWorld : public QObject
{
    Q_OBJECT

private slots:
    static void readsSectionsAndLinksBothWays()
    {
        const World world = testWorld();
        QCOMPARE(world.sections.size(), std::size_t{4});
        QCOMPARE(world.start, std::size_t{0});
        QCOMPARE(world.sections[0].map, QStringLiteral("a.json"));
        QVERIFY(world.sections[1].map.isEmpty());
        QCOMPARE(world.sections[2].landscape, QStringLiteral("frost"));
        QCOMPARE(world.sections[2].y, qint64{1000});
        QCOMPARE(world.sections[0].neighbors, (std::vector<std::size_t>{1, 2}));
        QCOMPARE(world.sections[3].neighbors, (std::vector<std::size_t>{1, 2}));
    }

    static void theShippedWorldIsValid()
    {
        const World world =
            required(qtd::core::loadWorldFile(QStringLiteral(QTD_CONTENT_DIR "/world.json")).world);
        QCOMPARE(world.sections.size(), std::size_t{9});
    }

    static void everyMapOfTheWorldHasTheSameCatalog_data()
    {
        QTest::addColumn<QString>("map");
        const World world =
            required(qtd::core::loadWorldFile(QStringLiteral(QTD_CONTENT_DIR "/world.json")).world);
        for (const qtd::core::WorldSection &section : world.sections) {
            if (section.map != u"training_ground.json"_s) { // the short tutorial has fewer enemies
                QTest::newRow(qPrintable(section.id)) << section.map;
            }
        }
    }

    // Until there is one catalog for all maps, their towers, enemies and spells must not drift
    // apart.
    static void everyMapOfTheWorldHasTheSameCatalog()
    {
        QFETCH(const QString, map);
        const QJsonArray catalog = catalogOf(u"mire_siege.json"_s);
        QVERIFY(!catalog[0].toArray().isEmpty());
        QCOMPARE(catalogOf(map), catalog);
    }

    static void aMissingFileNamesItself()
    {
        const qtd::core::WorldLoadResult result =
            qtd::core::loadWorldFile(QStringLiteral("no/such/world.json"));
        QVERIFY(!result.world);
        QVERIFY(result.error.startsWith(QStringLiteral("no/such/world.json: ")));
    }

    static void rejectsBrokenWorlds_data()
    {
        QTest::addColumn<QByteArray>("json");
        QTest::addColumn<QString>("error");
        QTest::newRow("no json") << QByteArray("{") << QString();
        QTest::newRow("twice the same id")
            << edited(R"("id": "b")", R"("id": "a")") << QStringLiteral("used twice");
        QTest::newRow("start unknown")
            << edited(R"("start": "a")", R"("start": "x")") << QStringLiteral("start");
        QTest::newRow("link from nowhere") << edited(R"("from": "b")", R"("from": "x")")
                                           << QStringLiteral("from: names no section");
        QTest::newRow("link to nowhere")
            << edited(R"("to": "b")", R"("to": "x")") << QStringLiteral("to: names no section");
        QTest::newRow("link to itself")
            << edited(R"("to": "b")", R"("to": "a")") << QStringLiteral("itself");
        QTest::newRow("link twice")
            << edited(R"("from": "c", "to": "d")", R"("from": "d", "to": "b")")
            << QStringLiteral("second time");
        QTest::newRow("unreachable") << edited(R"(,
        { "from": "c", "to": "d" })",
                                               "")
                                            .replace(R"({ "from": "c", "to": "a" },)", "")
                                     << QStringLiteral("no way leads to \"c\"");
        QTest::newRow("place outside")
            << edited(R"("x": 1000)", R"("x": 1001)") << QStringLiteral("x");
        QTest::newRow("empty map name")
            << edited(R"("map": "a.json")", R"("map": "")") << QStringLiteral("map");
        QTest::newRow("unknown key in a section")
            << edited(R"("name": "A")", R"("name": "A", "music": "x")") << QStringLiteral("music");
        QTest::newRow("unknown key in a link")
            << edited(R"("from": "a", "to": "b")", R"("from": "a", "to": "b", "x": 1)")
            << QStringLiteral("x");
        QTest::newRow("unknown key at the top")
            << edited(R"("start": "a")", R"("start": "a", "music": 1)") << QStringLiteral("music");
        QTest::newRow("wrong version") << edited(R"("formatVersion": 1)", R"("formatVersion": 2)")
                                       << QStringLiteral("formatVersion");
    }

    static void rejectsBrokenWorlds()
    {
        QFETCH(const QByteArray, json);
        QFETCH(const QString, error);
        QVERIFY(!json.isEmpty()); // the edit found its place
        const qtd::core::WorldLoadResult result = qtd::core::parseWorld(json);
        QVERIFY(!result.world);
        QVERIFY2(result.error.contains(error), qPrintable(result.error));
    }

    static void onlyTheStartIsOpenAtFirst()
    {
        const World world = testWorld();
        QCOMPARE(openSections(world, {}), (std::vector<bool>{true, false, false, false}));
        QVERIFY(!qtd::core::isOpen(world, {}, 4));
    }

    static void aWonSectionOpensItsNeighbors()
    {
        const World world = testWorld();
        CampaignProgress progress;
        qtd::core::recordStars(&progress, 2, 1);
        QCOMPARE(openSections(world, progress), (std::vector<bool>{true, false, true, true}));
    }

    static void starsKeepTheBest()
    {
        CampaignProgress progress;
        qtd::core::recordStars(&progress, 1, 2);
        qtd::core::recordStars(&progress, 1, 1);
        QCOMPARE(progress.stars, (std::vector<qint64>{0, 2}));
        qtd::core::recordStars(&progress, 1, 9);
        QCOMPARE(progress.stars[1], qtd::core::kMaxStars);
        qtd::core::recordStars(&progress, 0, -4);
        QCOMPARE(progress.stars[0], qint64{0});
    }

    static void progressGoesThereAndBack()
    {
        const World world = testWorld();
        CampaignProgress progress;
        qtd::core::recordStars(&progress, 0, 3);
        qtd::core::recordStars(&progress, 2, 1);
        const QByteArray json = qtd::core::progressToJson(world, progress);
        QVERIFY(!json.contains("\"b\""));
        QCOMPARE(qtd::core::parseProgress(world, json).stars, (std::vector<qint64>{3, 0, 1, 0}));
    }

    static void progressOfAPlayerIsReadKindly_data()
    {
        QTest::addColumn<QByteArray>("json");
        QTest::addColumn<std::vector<qint64>>("stars");
        QTest::newRow("broken") << QByteArray("{\"stars\": ") << std::vector<qint64>{0, 0, 0, 0};
        QTest::newRow("a section of another world")
            << QByteArray(R"({"stars": {"x": 3, "b": 2}})") << std::vector<qint64>{0, 2, 0, 0};
        QTest::newRow("too many stars")
            << QByteArray(R"({"stars": {"a": 4, "c": 3}})") << std::vector<qint64>{0, 0, 3, 0};
        QTest::newRow("negative") << QByteArray(R"({"stars": {"a": -1, "d": 1}})")
                                  << std::vector<qint64>{0, 0, 0, 1};
    }

    static void progressOfAPlayerIsReadKindly()
    {
        QFETCH(const QByteArray, json);
        QFETCH(const std::vector<qint64>, stars);
        QCOMPARE(qtd::core::parseProgress(testWorld(), json).stars, stars);
    }

    // Stars of a section this world does not know, from an older or newer world, stay in the file.
    static void starsOfOtherSectionsAreKept()
    {
        const World world = testWorld();
        CampaignProgress progress =
            qtd::core::parseProgress(world, R"({"stars": {"x": 3, "y": 7, "b": 1}})");
        QCOMPARE(progress.others, (QMap<QString, qint64>{{QStringLiteral("x"), 3}}));
        qtd::core::recordStars(&progress, 0, 2);
        const CampaignProgress again =
            qtd::core::parseProgress(world, qtd::core::progressToJson(world, progress));
        QCOMPARE(again.stars, (std::vector<qint64>{2, 1, 0, 0}));
        QCOMPARE(again.others, progress.others);
    }

    static void starsCountTheLivesKept_data()
    {
        QTest::addColumn<int>("startLives");
        QTest::addColumn<bool>("towers");
        QTest::addColumn<qint64>("stars");
        QTest::newRow("all lives") << 3 << true << qint64{3};
        QTest::newRow("exactly half") << 4 << false << qint64{2};
        QTest::newRow("less than half") << 3 << false << qint64{1};
        QTest::newRow("lost") << 2 << false << qint64{0};
    }

    static void starsCountTheLivesKept()
    {
        QFETCH(const int, startLives);
        QFETCH(const bool, towers);
        QFETCH(const qint64, stars);
        QCOMPARE(starsAfter(startLives, towers), stars);
    }

    static void aRunningGameHasNoStars()
    {
        const Simulation game(contentOf(qtd::tests::scenarioJson()), 1);
        QCOMPARE(qtd::core::starsOfGame(game), qint64{0});
    }
};

QTEST_APPLESS_MAIN(TestWorld)
#include "tst_world.moc"
