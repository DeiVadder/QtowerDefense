#include "core/content_loader.h"
#include "scenario.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using qtd::core::Fixed;
using qtd::core::loadContentFile;
using qtd::core::parseContent;
using qtd::core::Path;
using qtd::core::Point;
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
        R"({ "x": 10001, "y": 0 })",
        "path[1].x: must be a number from -10000 to 10000");
    row("same slot twice",
        R"({ "x": 2, "y": 1 })",
        R"({ "x": 5, "y": 1 })",
        "buildSlots[1].x: another build slot has the same place");
    row("tower twice",
        R"("towers": [{)",
        R"("towers": [{"id": "arrow", "cost": 1, "range": 1, "cooldownTicks": 1,
            "damageMin": 1, "damageMax": 1}, {)",
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
        QCOMPARE(content.buildSlots.size(), 3U);
        QVERIFY(content.buildSlots[1] == at(2, 1));
        const qtd::core::TowerType &tower = content.towers.at(0);
        QCOMPARE(tower.id, QStringLiteral("arrow"));
        QCOMPARE(tower.cost, 40);
        QCOMPARE(tower.range.raw(), 30'000);
        QCOMPARE(tower.cooldownTicks, 4);
        QCOMPARE(tower.damageMin.raw(), 20'000);
        QCOMPARE(tower.damageMax.raw(), 20'000);
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
        QCOMPARE(required(result.content).towers.at(0).range.raw(), 1'000);
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
        QTest::newRow("negative coordinate")
            << QByteArray(R"("x": 5)") << QByteArray(R"("x": -10000)");
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
                 QStringLiteral("line 9: number %1 is no integer, write decimal "
                                "values as strings like \"1.5\"")
                     .arg(QString::fromLatin1(number)));
    }

    static void rejectsIntegersThatDoNotStayExact()
    {
        const qtd::core::ContentLoadResult result =
            parseContent(editedJson(R"("cost": 40)", R"("cost": 9007199254740993000)"));
        QVERIFY(!result.content.has_value());
        QCOMPARE(result.error, QStringLiteral("line 9: number 9007199254740993000 is too large"));
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
