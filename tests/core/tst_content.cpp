#include "core/content_loader.h"
#include "scenario.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>
#include <set>
#include <vector>

using qtd::core::Fixed;
using qtd::core::loadContentFile;
using qtd::core::parseContent;
using qtd::core::Path;
using qtd::core::Point;
using qtd::core::Terrain;
using qtd::tests::editedJson;
using qtd::tests::required;
using qtd::tests::scenarioJson;

namespace {

Point at(qint64 x, qint64 y)
{
    return {Fixed::fromInt(x), Fixed::fromInt(y)};
}

void addBrokenContentRows()
{
    QTest::addColumn<QByteArray>("from");
    QTest::addColumn<QByteArray>("to");
    QTest::addColumn<QString>("error");
    const auto row = [](const char *name, const char *from, const char *to, const char *error) {
        QTest::newRow(name) << QByteArray(from) << QByteArray(to) << QString::fromUtf8(error);
    };
    row("no version", "\"formatVersion\": 1,", "", "formatVersion: is missing");
    row("wrong version",
        R"("formatVersion": 1)",
        R"("formatVersion": 2)",
        "formatVersion: must be an integer from 1 to 1");
    row("missing key", "\"startGold\": 100,", "", "startGold: is missing");
    row("missing nested key", "\"cost\": 40,", "", "towers[0].cost: is missing");
    row("unknown key",
        R"("startLives": 3)",
        R"("startLives": 3, "golld": 1)",
        "golld: unknown key");
    row("unknown nested key",
        R"("cost": 40)",
        R"("cost": 40, "costs": 1)",
        "towers[0].costs: unknown key");
    row("no lives",
        R"("startLives": 3)",
        R"("startLives": 0)",
        "startLives: must be an integer from 1 to 1000000000");
    row("negative cost",
        R"("cost": 40)",
        R"("cost": -1)",
        "towers[0].cost: must be an integer from 0 to 1000000000");
    row("cost over the limit",
        R"("cost": 40)",
        R"("cost": 1000000001)",
        "towers[0].cost: must be an integer from 0 to 1000000000");
    row("text as integer",
        R"("cost": 40)",
        R"("cost": "40")",
        "towers[0].cost: must be an integer");
    row("zero cooldown",
        R"("cooldownTicks": 4)",
        R"("cooldownTicks": 0)",
        "towers[0].cooldownTicks: must be");
    row("too many decimals",
        R"("range": "3")",
        R"("range": "3.12345")",
        "towers[0].range: must be a number");
}

void addBrokenFixedRows()
{
    const auto row = [](const char *name, const char *from, const char *to, const char *error) {
        QTest::newRow(name) << QByteArray(from) << QByteArray(to) << QString::fromUtf8(error);
    };
    row("text as fixed",
        R"("range": "3")",
        R"("range": "far")",
        "towers[0].range: must be a number");
    row("bool as fixed",
        R"("range": "3")",
        R"("range": true)",
        "towers[0].range: must be a number");
    row("zero range", R"("range": "3")", R"("range": "0")", "towers[0].range: must be a number");
    row("huge integer as fixed",
        R"("health": "4")",
        R"("health": 1000000001)",
        "enemies[0].health: must be a number");
    row("swapped damage",
        R"("damageMin": "2")",
        R"("damageMin": "3")",
        "towers[0].damageMax: must not be below damageMin");
    row("no speed",
        R"("speedPerTick": "0.5")",
        R"("speedPerTick": "0")",
        "enemies[0].speedPerTick: must be a number");
    row("empty id",
        R"("id": "grunt")",
        R"("id": "")",
        "enemies[0].id: must be a text that is not empty");
    row("number as id",
        R"("id": "grunt")",
        R"("id": 5)",
        "enemies[0].id: must be a text that is not empty");
}

void addBrokenMapRows()
{
    const auto row = [](const char *name, const char *from, const char *to, const char *error) {
        QTest::newRow(name) << QByteArray(from) << QByteArray(to) << QString::fromUtf8(error);
    };
    row("diagonal path",
        R"({ "x": 10, "y": 0 })",
        R"({ "x": 10, "y": 1 })",
        "path[1].x: must share a row or column with the point before it");
    row("grid too large",
        R"("gridWidth": 11)",
        R"("gridWidth": 257)",
        "gridWidth: must be an integer from 1 to 256");
    row("obstacle outside a small grid",
        R"("gridHeight": 11)",
        R"("gridHeight": 1)",
        "obstacles[0].y: must be an integer from 0 to 0");
    row("unknown obstacle",
        R"("terrain": "rock")",
        R"("terrain": "lava")",
        "obstacles[0].terrain: unknown obstacle \"lava\"");
    row("obstacle on the path",
        R"("terrain": "rock", "x": 0, "y": 2)",
        R"("terrain": "rock", "x": 4, "y": 0)",
        "obstacles[0].x: the cell is not free");
    row("same cell twice",
        R"("obstacles": [{)",
        R"("obstacles": [{ "terrain": "tree", "x": 0, "y": 2 }, {)",
        "obstacles[1].x: the cell is not free");
    row("footprint without its cell",
        R"("footprint": [{"x": 0, "y": 0}])",
        R"("footprint": [{"x": 1, "y": 0}])",
        "towers[0].footprint: needs the cell 0, 0 that the tower stands on");
    row("footprint cell twice",
        R"("footprint": [{"x": 0, "y": 0}])",
        R"("footprint": [{"x": 0, "y": 0}, {"x": 0, "y": 0}])",
        "towers[0].footprint[1].x: repeats a cell of the footprint");
    row("footprint too wide",
        R"("footprint": [{"x": 0, "y": 0}])",
        R"("footprint": [{"x": 0, "y": 0}, {"x": 9, "y": 0}])",
        "towers[0].footprint[1].x: must be an integer from -8 to 8");
}

void addBrokenCombatRows()
{
    const auto row = [](const char *name, const char *from, const char *to, const char *error) {
        QTest::newRow(name) << QByteArray(from) << QByteArray(to) << QString::fromUtf8(error);
    };
    row("unknown damage type",
        R"("damageType": "physical")",
        R"("damageType": "holy")",
        "towers[0].damageType: unknown damage type \"holy\"");
    row("no damage type", R"("damageType": "physical",)", "", "towers[0].damageType: is missing");
    row("unknown effect",
        R"("damageType": "physical")",
        R"("damageType": "physical", "effects": [{"kind": "curse"}])",
        "towers[0].effects[0].kind: unknown status effect \"curse\"");
    row("effect twice",
        R"("damageType": "physical")",
        R"("damageType": "physical", "effects": [
            {"kind": "poison", "strength": "1", "durationTicks": 1},
            {"kind": "poison", "strength": "1", "durationTicks": 1}])",
        "towers[0].effects[1].kind: repeats a status effect of this tower");
    row("slow too strong",
        R"("damageType": "physical")",
        R"("damageType": "physical", "effects": [
            {"kind": "slow", "strength": "0.9001", "durationTicks": 1}])",
        "towers[0].effects[0].strength: must be");
    row("effect without time",
        R"("damageType": "physical")",
        R"("damageType": "physical", "effects": [
            {"kind": "slow", "strength": "0.5", "durationTicks": 0}])",
        "towers[0].effects[0].durationTicks: must be");
    row("resistance too high",
        R"("lifeCost": 1)",
        R"("lifeCost": 1, "resistances": [{"damageType": "ice", "share": "1.0001"}])",
        "enemies[0].resistances[0].share: must be");
    row("resistance too low",
        R"("lifeCost": 1)",
        R"("lifeCost": 1, "resistances": [{"damageType": "ice", "share": "-1.0001"}])",
        "enemies[0].resistances[0].share: must be");
    row("resistance twice",
        R"("lifeCost": 1)",
        R"("lifeCost": 1, "resistances": [{"damageType": "ice", "share": "1"},
                                          {"damageType": "ice", "share": "1"}])",
        "enemies[0].resistances[1].damageType: repeats a resistance of this enemy");
}

void addBrokenStructureRows()
{
    QTest::addColumn<QByteArray>("from");
    QTest::addColumn<QByteArray>("to");
    QTest::addColumn<QString>("error");
    const auto row = [](const char *name, const char *from, const char *to, const char *error) {
        QTest::newRow(name) << QByteArray(from) << QByteArray(to) << QString::fromUtf8(error);
    };
    const char *path = R"("path": [{ "x": 0, "y": 0 }, { "x": 10, "y": 0 }])";
    row("one point", path, R"("path": [{ "x": 0, "y": 0 }])", "path: needs at least two points");
    row("no points", path, R"("path": [])", "path: must be an array");
    row("path no array", path, R"("path": 5)", "path: must be an array");
    row("repeated point",
        R"({ "x": 10, "y": 0 })",
        R"({ "x": 0, "y": 0 })",
        "path[1].x: repeats the point before it");
    row("point no object", R"({ "x": 10, "y": 0 })", "7", "path[1]: must be an object");
    row("point off the map",
        R"({ "x": 10, "y": 0 })",
        R"({ "x": 11, "y": 0 })",
        "path[1].x: must be an integer from 0 to 10");
    row("tower twice",
        R"("towers": [{)",
        R"("towers": [{"id": "arrow", "cost": 1, "range": 1, "cooldownTicks": 1,
            "damageMin": 1, "damageMax": 1, "damageType": "ice",
            "footprint": [{"x": 0, "y": 0}]}, {)",
        "towers: id \"arrow\" is used twice");
    row("enemy twice",
        R"("enemies": [{)",
        R"("enemies": [{"id": "grunt", "health": 1, "speedPerTick": 1, "bounty": 1,
            "lifeCost": 1}, {)",
        "enemies: id \"grunt\" is used twice");
    row("unknown enemy",
        R"("enemy": "grunt")",
        R"("enemy": "ghost")",
        "wave[0].enemy: unknown enemy \"ghost\"");
    row("empty wave",
        R"("wave": [{ "enemy": "grunt", "count": 2, "delayTicks": 0, "intervalTicks": 4 }])",
        R"("wave": [])",
        "wave: must be an array");
    row("no enemies in group", R"("count": 2)", R"("count": 0)", "wave[0].count: must be");
}

} // namespace

class TestContent : public QObject
{
    Q_OBJECT

private slots:
    static void readsEveryField()
    {
        const qtd::core::ContentLoadResult result = parseContent(scenarioJson());
        QCOMPARE(result.error, QString());
        const qtd::core::Content content = required(result.content);
        QCOMPARE(content.startGold, 100);
        QCOMPARE(content.startLives, 3);
        QCOMPARE(content.path.points().size(), 2U);
        QCOMPARE(content.path.length().raw(), 100'000);
        QCOMPARE(content.grid.width(), 11);
        QCOMPARE(content.grid.height(), 11);
        QVERIFY(content.grid.terrainAt({0, 0}) == Terrain::Path);
        QVERIFY(content.grid.terrainAt({10, 0}) == Terrain::Path);
        QVERIFY(content.grid.terrainAt({0, 1}) == Terrain::Open);
        QVERIFY(content.grid.terrainAt({0, 2}) == Terrain::Rock);
        QCOMPARE(content.towers.at(0).footprint.size(), 1U);
        const qtd::core::TowerType &tower = content.towers.at(0);
        QCOMPARE(tower.id, QStringLiteral("arrow"));
        QCOMPARE(tower.levels.at(0).cost, 40);
        QCOMPARE(tower.levels.at(0).range.raw(), 30'000);
        QCOMPARE(tower.levels.at(0).cooldownTicks, 4);
        QCOMPARE(tower.levels.at(0).damageMin.raw(), 20'000);
        QCOMPARE(tower.levels.at(0).damageMax.raw(), 20'000);
        const qtd::core::EnemyType &enemy = content.enemies.at(0);
        QCOMPARE(enemy.id, QStringLiteral("grunt"));
        QCOMPARE(enemy.health.raw(), 40'000);
        QCOMPARE(enemy.speedPerTick.raw(), 5'000);
        QCOMPARE(enemy.bounty, 7);
        QCOMPARE(enemy.lifeCost, 1);
        const qtd::core::SpawnGroup &group = content.wave.at(0);
        QCOMPARE(group.enemyType, 0U);
        QCOMPARE(group.count, 2);
        QCOMPARE(group.delayTicks, 0);
        QCOMPARE(group.intervalTicks, 4);
    }

    static void decimalsArriveExactly()
    {
        const qtd::core::ContentLoadResult result =
            parseContent(editedJson(R"("range": "3")", R"("range": "0.1")"));
        QCOMPARE(required(result.content).towers.at(0).levels.at(0).range.raw(), 1'000);
    }

    static void acceptsValuesAtTheLimits_data()
    {
        QTest::addColumn<QByteArray>("from");
        QTest::addColumn<QByteArray>("to");
        QTest::newRow("largest integer")
            << QByteArray(R"("cost": 40)") << QByteArray(R"("cost": 1000000000)");
        QTest::newRow("free tower") << QByteArray(R"("cost": 40)") << QByteArray(R"("cost": 0)");
        QTest::newRow("smallest fixed")
            << QByteArray(R"("range": "3")") << QByteArray(R"("range": "0.0001")");
        QTest::newRow("integer as fixed")
            << QByteArray(R"("range": "3")") << QByteArray(R"("range": 30000)");
        QTest::newRow("largest grid")
            << QByteArray(R"("gridWidth": 11)") << QByteArray(R"("gridWidth": 256)");
        QTest::newRow("widest footprint")
            << QByteArray(R"("footprint": [{"x": 0, "y": 0}])")
            << QByteArray(R"("footprint": [{"x": 0, "y": 0}, {"x": 8, "y": -8}])");
        QTest::newRow("number-like id")
            << QByteArray(R"("id": "arrow")") << QByteArray(R"("id": "1.5e3")");
        // No raw string here: moc loses its way at a backslash and a quote in one.
        // NOLINTNEXTLINE(modernize-raw-string-literal)
        const QByteArray escaped("\"id\": \"a\\\"1.5\"");
        QTest::newRow("escaped quote in id") << QByteArray(R"("id": "arrow")") << escaped;
    }

    static void acceptsValuesAtTheLimits()
    {
        QFETCH(const QByteArray, from);
        QFETCH(const QByteArray, to);
        const qtd::core::ContentLoadResult result = parseContent(editedJson(from, to));
        QCOMPARE(result.error, QString());
        QVERIFY(result.content.has_value());
    }

    static void rejectsBrokenContent_data()
    {
        addBrokenContentRows();
        addBrokenFixedRows();
    }

    static void rejectsBrokenContent()
    {
        QFETCH(const QByteArray, from);
        QFETCH(const QByteArray, to);
        QFETCH(const QString, error);
        const QByteArray json = editedJson(from, to);
        QVERIFY(!json.isEmpty());
        const qtd::core::ContentLoadResult result = parseContent(json);
        QVERIFY(!result.content.has_value());
        QVERIFY2(result.error.startsWith(error), qPrintable(result.error));
    }

    static void rejectsBrokenStructure_data()
    {
        addBrokenStructureRows();
        addBrokenCombatRows();
        addBrokenMapRows();
    }

    static void rejectsBrokenStructure()
    {
        rejectsBrokenContent();
    }

    // A floating point number in the file would be rounded by Qt, so none may get through. The
    // line in the error is the one of the number in scenarioJson().
    static void rejectsEveryNumberThatIsNotAnExactInteger_data()
    {
        QTest::addColumn<QByteArray>("number");
        QTest::newRow("fraction") << QByteArray("40.5");
        QTest::newRow("integral fraction") << QByteArray("40.0");
        QTest::newRow("exponent") << QByteArray("4e1");
        QTest::newRow("capital exponent") << QByteArray("4E+1");
        QTest::newRow("beyond double precision") << QByteArray("1.00000000000000000001");
        QTest::newRow("negative fraction") << QByteArray("-0.5");
        QTest::newRow("leading point") << QByteArray(".5");
        QTest::newRow("negative leading point") << QByteArray("-.5");
    }

    static void rejectsEveryNumberThatIsNotAnExactInteger()
    {
        QFETCH(const QByteArray, number);
        const qtd::core::ContentLoadResult result =
            parseContent(editedJson(R"("cost": 40)", R"("cost": )" + number));
        QVERIFY(!result.content.has_value());
        QCOMPARE(result.error,
                 QStringLiteral("line 11: number %1 is no integer, write decimal "
                                "values as strings like \"1.5\"")
                     .arg(QString::fromLatin1(number)));
    }

    static void rejectsIntegersThatDoNotStayExact()
    {
        const qtd::core::ContentLoadResult result =
            parseContent(editedJson(R"("cost": 40)", R"("cost": 9007199254740993000)"));
        QVERIFY(!result.content.has_value());
        QCOMPARE(result.error, QStringLiteral("line 11: number 9007199254740993000 is too large"));
    }

    static void rejectsInvalidJson_data()
    {
        QTest::addColumn<QByteArray>("json");
        QTest::newRow("empty") << QByteArray();
        QTest::newRow("cut off") << scenarioJson().first(50);
        QTest::newRow("trailing comma") << QByteArray(R"({"a": 1,})");
        QTest::newRow("no JSON at all") << QByteArray("tower defense");
        QTest::newRow("binary") << QByteArray("\xff\xfe\x00\x01", 4);
    }

    static void rejectsInvalidJson()
    {
        QFETCH(const QByteArray, json);
        const qtd::core::ContentLoadResult result = parseContent(json);
        QVERIFY(!result.content.has_value());
        QVERIFY2(result.error.startsWith(QStringLiteral("invalid JSON at offset")),
                 qPrintable(result.error));
    }

    static void topLevelMustBeAnObject()
    {
        const qtd::core::ContentLoadResult result = parseContent("[1, 2]");
        QVERIFY(!result.content.has_value());
        QCOMPARE(result.error, QStringLiteral("the top level must be an object"));
    }

    static void keepsTheFirstError()
    {
        QByteArray json = editedJson(R"("cost": 40)", R"("cost": -1)");
        json.replace(R"("bounty": 7)", R"("bounty": -1)");
        QVERIFY(parseContent(json).error.startsWith(QStringLiteral("towers[0].cost")));
    }

    // Every enemy of the map walks, and each tower has somewhere to grow.
    static void theSiegeHasTheWholeRoster()
    {
        const qtd::core::Content content =
            required(loadContentFile(QStringLiteral(QTD_CONTENT_DIR "/mire_siege.json")).content);
        QVERIFY(content.towers.size() >= 6);
        QVERIFY(std::ranges::all_of(content.towers, [](const qtd::core::TowerType &tower) {
            return tower.levels.size() >= 3;
        }));
        QVERIFY(std::ranges::any_of(content.towers, [](const qtd::core::TowerType &tower) {
            return tower.levels.front().splashRadius > qtd::core::Fixed();
        }));
        const std::ptrdiff_t bosses =
            std::ranges::count_if(content.enemies, &qtd::core::EnemyType::boss);
        QVERIFY(bosses >= 1);
        QVERIFY(std::ssize(content.enemies) - bosses >= 8);
        std::set<std::size_t> walking;
        for (const qtd::core::SpawnGroup &group : content.wave) {
            walking.insert(group.enemyType);
        }
        QCOMPARE(walking.size(), content.enemies.size());
    }

    static void loadsTheShippedContent()
    {
        const qtd::core::ContentLoadResult result =
            loadContentFile(QStringLiteral(QTD_CONTENT_DIR "/training_ground.json"));
        QCOMPARE(result.error, QString());
        QCOMPARE(required(result.content).path.length().raw(), 220'000); // 8 + 6 + 8 tiles
    }

    static void namesTheFileInErrors()
    {
        const QTemporaryDir folder;
        const QString name = folder.filePath(QStringLiteral("broken.json"));
        QFile file(name);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("{}");
        file.close();
        QCOMPARE(loadContentFile(name).error, name + QStringLiteral(": formatVersion: is missing"));
    }

    static void reportsAMissingFile()
    {
        const QTemporaryDir folder;
        const QString name = folder.filePath(QStringLiteral("nothing.json"));
        const qtd::core::ContentLoadResult result = loadContentFile(name);
        QVERIFY(!result.content.has_value());
        QVERIFY2(result.error.startsWith(name + QStringLiteral(": ")), qPrintable(result.error));
    }

    static void readsDamageEffectsAndResistances()
    {
        const qtd::core::Content content =
            required(parseContent(editedJson(R"("damageType": "physical")",
                                             R"("damageType": "fire", "effects": [
                {"kind": "poison", "strength": "0.5", "durationTicks": 9},
                {"kind": "slow", "strength": "0.9", "durationTicks": 3}])"))
                         .content);
        const qtd::core::TowerType &tower = content.towers.at(0);
        QVERIFY(tower.damageType == qtd::core::DamageType::Fire);
        QCOMPARE(tower.levels.at(0).effects.size(), 2U);
        QVERIFY(tower.levels.at(0).effects[0].kind == qtd::core::StatusKind::Poison);
        QCOMPARE(tower.levels.at(0).effects[0].strength.raw(), 5000);
        QCOMPARE(tower.levels.at(0).effects[0].durationTicks, 9);
        QVERIFY(tower.levels.at(0).effects[1].kind == qtd::core::StatusKind::Slow);
        QCOMPARE(tower.levels.at(0).effects[1].strength.raw(), 9000);
        const qtd::core::Content resistant =
            required(parseContent(editedJson(R"("lifeCost": 1)",
                                             R"("lifeCost": 1, "resistances": [
                {"damageType": "magic", "share": "-1"}, {"damageType": "ice", "share": "1"}])"))
                         .content);
        const qtd::core::EnemyType &enemy = resistant.enemies.at(0);
        QCOMPARE(qtd::core::hitShare(enemy, qtd::core::DamageType::Physical).raw(), 10000);
        QCOMPARE(qtd::core::hitShare(enemy, qtd::core::DamageType::Magic).raw(), 20000);
        QCOMPARE(qtd::core::hitShare(enemy, qtd::core::DamageType::Fire).raw(), 10000);
        QCOMPARE(qtd::core::hitShare(enemy, qtd::core::DamageType::Ice).raw(), 0);
    }

    static void namesOfPrioritiesGoBothWays()
    {
        using qtd::core::TargetPriority;
        for (const TargetPriority priority : {TargetPriority::First,
                                              TargetPriority::Last,
                                              TargetPriority::Strongest,
                                              TargetPriority::Nearest}) {
            QVERIFY(qtd::core::targetPriorityNamed(qtd::core::toString(priority)) == priority);
        }
        QVERIFY(!qtd::core::targetPriorityNamed(u"weakest").has_value());
    }

    static void aTurnedFootprintGoesRoundItsCell_data()
    {
        using qtd::core::Cell;
        using Cells = std::vector<Cell>;
        QTest::addColumn<qtd::core::Rotation>("rotation");
        QTest::addColumn<Cells>("cells");
        QTest::newRow("none") << qtd::core::Rotation::None
                              << Cells{Cell{5, 5}, Cell{7, 5}, Cell{5, 6}};
        QTest::newRow("quarter") << qtd::core::Rotation::Quarter
                                 << Cells{Cell{5, 5}, Cell{5, 7}, Cell{4, 5}};
        QTest::newRow("half") << qtd::core::Rotation::Half
                              << Cells{Cell{5, 5}, Cell{3, 5}, Cell{5, 4}};
        QTest::newRow("three quarters")
            << qtd::core::Rotation::ThreeQuarters << Cells{Cell{5, 5}, Cell{5, 3}, Cell{6, 5}};
    }

    static void aTurnedFootprintGoesRoundItsCell()
    {
        using Cells = std::vector<qtd::core::Cell>;
        QFETCH(const qtd::core::Rotation, rotation);
        QFETCH(const Cells, cells);
        qtd::core::TowerType type;
        type.footprint = {{0, 0}, {2, 0}, {0, 1}};
        QVERIFY(qtd::core::footprintAt(type, {5, 5}, rotation) == cells);
    }

    static void fourQuarterTurnsComeBack()
    {
        using qtd::core::nextRotation;
        using qtd::core::Rotation;
        QVERIFY(nextRotation(Rotation::None) == Rotation::Quarter);
        QVERIFY(nextRotation(Rotation::Quarter) == Rotation::Half);
        QVERIFY(nextRotation(Rotation::Half) == Rotation::ThreeQuarters);
        QVERIFY(nextRotation(Rotation::ThreeQuarters) == Rotation::None);
    }

    // 3-4-5 triangle, then six tiles straight on: all positions can be read off directly.
    static void pathsAreWalkedByDistance()
    {
        const Path path({at(0, 0), at(3, 4), at(3, 10)});
        QCOMPARE(path.length().raw(), 110'000);
        QVERIFY(path.positionAt(Fixed::fromRaw(25'000)) ==
                (Point{Fixed::fromRaw(15'000), at(0, 2).y}));
        QVERIFY(path.positionAt(Fixed::fromInt(5)) == at(3, 4));
        QVERIFY(path.positionAt(Fixed::fromInt(8)) == at(3, 7));
        QVERIFY(path.positionAt(Fixed()) == at(0, 0));
        QVERIFY(path.positionAt(Fixed::fromInt(-1)) == at(0, 0));
        QVERIFY(path.positionAt(Fixed::fromInt(11)) == at(3, 10));
        QVERIFY(path.positionAt(Fixed::fromInt(99)) == at(3, 10));
    }

    static void emptyPathStaysAtTheOrigin()
    {
        const Path path;
        QCOMPARE(path.length().raw(), 0);
        QVERIFY(path.positionAt(Fixed::fromInt(1)) == Point{});
    }
};

QTEST_APPLESS_MAIN(TestContent)
#include "tst_content.moc"
