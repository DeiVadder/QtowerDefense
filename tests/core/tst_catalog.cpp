#include "core/content_loader.h"
#include "core/state_hash.h"
#include "scenario.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

#include <utility>

using namespace Qt::StringLiterals;
using qtd::core::loadContentFile;
using qtd::core::parseContent;
using qtd::tests::required;
using qtd::tests::scenarioJson;

namespace {

/// Writes the scenario as a map with its towers and enemies in a catalog next to it, changed by
/// the callback, and gives the file name of the map.
template <typename Change>
QString writeMapWithCatalog(const QTemporaryDir &folder, Change change)
{
    QJsonObject map = QJsonDocument::fromJson(scenarioJson()).object();
    QJsonObject catalog{{QStringLiteral("formatVersion"), 1}};
    for (const QString &key : {QStringLiteral("towers"), QStringLiteral("enemies")}) {
        catalog.insert(key, map.take(key));
    }
    map.insert(QStringLiteral("catalog"), QStringLiteral("catalog.json"));
    change(&map, &catalog);
    QString mapFile = folder.filePath(QStringLiteral("map.json"));
    for (const auto &[name, object] :
         {std::pair{mapFile, map},
          std::pair{folder.filePath(QStringLiteral("catalog.json")), catalog}}) {
        QFile file(name);
        if (!file.open(QIODevice::WriteOnly)) {
            return {};
        }
        file.write(QJsonDocument(object).toJson());
    }
    return mapFile;
}

} // namespace

// Towers, enemies and spells of the campaign in one file next to the maps (E-37).
class TestCatalog : public QObject
{
    Q_OBJECT

private slots:
    // A map with a catalog is the same content as one with its own towers and enemies (E-37).
    static void aCatalogGivesTheSameContent()
    {
        const QTemporaryDir folder;
        const QString map = writeMapWithCatalog(folder, [](QJsonObject *, QJsonObject *) {});
        const qtd::core::ContentLoadResult loaded = loadContentFile(map);
        QCOMPARE(loaded.error, QString());
        QCOMPARE(qtd::core::contentHash(required(loaded.content)),
                 qtd::core::contentHash(required(parseContent(scenarioJson()).content)));
    }

    // The value is JSON text.
    static void aWrongCatalogNamesTheFileAtFault_data()
    {
        QTest::addColumn<QString>("key");
        QTest::addColumn<QString>("value");
        QTest::addColumn<bool>("inCatalog");
        QTest::addColumn<QString>("error");
        QTest::newRow("no file name")
            << u"catalog"_s << u"3"_s << false << u"map.json: catalog must be a file name"_s;
        for (const QString &key : {u"towers"_s, u"enemies"_s, u"spells"_s}) {
            QTest::newRow(qPrintable(key + u" of its own"_s))
                << key << u"[]"_s << false << u"map.json: %1 come from the catalog"_s.arg(key);
        }
        QTest::newRow("no such catalog")
            << u"catalog"_s << u"\"none.json\""_s << false << u"none.json"_s;
        QTest::newRow("unknown key in the catalog")
            << u"wave"_s << u"[]"_s << true << u"catalog.json: wave: unknown key"_s;
        QTest::newRow("a catalog of the catalog") << u"catalog"_s << u"\"other.json\""_s << true
                                                  << u"catalog.json: catalog: unknown key"_s;
        QTest::newRow("another format")
            << u"formatVersion"_s << u"2"_s << true << u"catalog.json: formatVersion"_s;
    }

    static void aWrongCatalogNamesTheFileAtFault()
    {
        QFETCH(const QString, key);
        QFETCH(const QString, value);
        QFETCH(const bool, inCatalog);
        QFETCH(const QString, error);
        const QTemporaryDir folder;
        const QJsonValue json = QJsonDocument::fromJson(("[" + value + "]").toUtf8()).array().at(0);
        const QString map =
            writeMapWithCatalog(folder, [&](QJsonObject *mapObject, QJsonObject *catalog) {
                (inCatalog ? catalog : mapObject)->insert(key, json);
            });
        const qtd::core::ContentLoadResult loaded = loadContentFile(map);
        QVERIFY(!loaded.content);
        QVERIFY2(loaded.error.contains(error), qPrintable(loaded.error));
    }

    static void aBrokenCatalogNamesItself()
    {
        const QTemporaryDir folder;
        const QString map = writeMapWithCatalog(folder, [](QJsonObject *, QJsonObject *) {});
        QFile catalog(folder.filePath(u"catalog.json"_s));
        QVERIFY(catalog.open(QIODevice::WriteOnly));
        catalog.write("{ \"towers\": ");
        catalog.close();
        const qtd::core::ContentLoadResult loaded = loadContentFile(map);
        QVERIFY(!loaded.content);
        QVERIFY2(loaded.error.startsWith(catalog.fileName() + u": "_s), qPrintable(loaded.error));
    }

    static void theSpellsComeFromTheCatalog()
    {
        const QTemporaryDir folder;
        const QString map = writeMapWithCatalog(folder, [](QJsonObject *, QJsonObject *catalog) {
            catalog->insert(u"spells"_s,
                            QJsonArray{QJsonObject{{u"id"_s, u"meteor"_s},
                                                   {u"cooldownTicks"_s, 600},
                                                   {u"radius"_s, u"2"_s},
                                                   {u"damageMin"_s, u"15"_s},
                                                   {u"damageMax"_s, u"25"_s},
                                                   {u"damageType"_s, u"fire"_s}}});
        });
        const qtd::core::ContentLoadResult loaded = loadContentFile(map);
        QCOMPARE(loaded.error, QString());
        QCOMPARE(required(loaded.content).spells.size(), std::size_t{1});
    }

    // What belongs to the map is still the error of the map.
    static void anUnknownEnemyIsTheErrorOfTheMap()
    {
        const QTemporaryDir folder;
        const QString map = writeMapWithCatalog(folder, [](QJsonObject *, QJsonObject *catalog) {
            QJsonArray enemies = (*catalog)[QStringLiteral("enemies")].toArray();
            QJsonObject grunt = enemies[0].toObject();
            grunt[QStringLiteral("id")] = QStringLiteral("orc");
            enemies[0] = grunt;
            catalog->insert(QStringLiteral("enemies"), enemies);
        });
        const qtd::core::ContentLoadResult loaded = loadContentFile(map);
        QVERIFY2(loaded.error.startsWith(map + QStringLiteral(": wave")), qPrintable(loaded.error));
    }
};

QTEST_APPLESS_MAIN(TestCatalog)
#include "tst_catalog.moc"
