#include "client/game_session.h"
#include "client/ground_field.h"
#include "client/landscape.h"
#include "client/landscape_view.h"
#include "client/scenery_instances.h"
#include "client/terrain_mesh.h"
#include "client/terrain_picture.h"

#include <QFile>
#include <QSignalSpy>
#include <QTest>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

using namespace qtd::client;
using namespace Qt::StringLiterals;

namespace {

QString meadowMap()
{
    return QStringLiteral(QTD_CONTENT_DIR "/training_ground.json");
}

QString swampMap()
{
    return QStringLiteral(QTD_CONTENT_DIR "/mire_siege.json");
}

bool hasFiles(const QString &model)
{
    return QFile::exists(u":/assets/%1.mesh"_s.arg(model)) &&
           QFile::exists(u":/assets/%1.png"_s.arg(model));
}

QStringList modelsOf(const Landscape &landscape)
{
    QStringList models{landscape.treeModel, landscape.rockModel};
    for (const SceneryItem &item : landscape.scenery) {
        models.append(item.model);
    }
    return models;
}

/// Opens the table of a kind of scenery. Its real user is the renderer.
class OpenScenery : public SceneryInstances
{
public:
    OpenScenery(LandscapeView *view, int item)
    {
        setFieldSize(1);
        setItem(item);
        setView(view);
    }

    std::vector<QVector3D> places()
    {
        int count = 0;
        const QByteArray buffer = getInstanceBuffer(&count);
        std::vector<QVector3D> found;
        for (int index = 0; index < count; ++index) {
            InstanceTableEntry entry{};
            std::memcpy(
                &entry, buffer.constData() + index * qsizetype{sizeof(entry)}, sizeof(entry));
            found.push_back(entry.getPosition());
        }
        return found;
    }
};

int firstItemIn(const Landscape &landscape, SceneryZone zone)
{
    for (std::size_t index = 0; index < landscape.scenery.size(); ++index) {
        if (landscape.scenery[index].zone == zone) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

} // namespace

class TstLandscape : public QObject
{
    Q_OBJECT

private slots:
    static void mapsHaveTheirLandscape()
    {
        const Landscape swamp = landscapeOfMap(swampMap());
        QVERIFY(swamp.water.isValid());
        QCOMPARE(swamp.treeModel, u"dark_tree"_s);
        QVERIFY(!landscapeOfMap(meadowMap()).lakes);
    }

    static void anUnknownMapGetsTheFallback()
    {
        QCOMPARE(landscapeOfMap(u"somewhere/else.json"_s).treeModel,
                 landscapeOfMap(meadowMap()).treeModel);
    }

    static void aBrokenFileGivesThePlainLook()
    {
        const Landscape landscape = landscapeOfMap(QByteArray("{ not json"), meadowMap());
        QCOMPARE(landscape.treeModel, u"tree"_s);
        QVERIFY(landscape.scenery.empty());
    }

    static void scenerySaysWhereItStands()
    {
        const QByteArray text = R"({"maps": {"a.json": "x"}, "landscapes": {"x": {"scenery": [
            {"model": "m", "zone": "water", "count": 3, "minSize": 0.5, "maxSize": 2},
            {"model": "n", "zone": "moon", "count": 3}]}}})";
        QTest::ignoreMessage(QtWarningMsg, "Scenery with an unknown zone: moon");
        const Landscape landscape = landscapeOfMap(text, u"a.json"_s);
        QCOMPARE(landscape.scenery.size(), std::size_t{1});
        QCOMPARE(landscape.scenery[0].zone, SceneryZone::Water);
        QCOMPARE(landscape.scenery[0].count, 3);
        QCOMPARE(landscape.scenery[0].maxSize, 2.0F);
    }

    static void everyModelOfALandscapeIsThere()
    {
        const QStringList models =
            modelsOf(landscapeOfMap(meadowMap())) + modelsOf(landscapeOfMap(swampMap()));
        QVERIFY(std::ranges::all_of(models, hasFiles));
    }

    static void theGroundIsFlatOnlyWhereThePlayerBuilds()
    {
        GameSession game;
        game.startGame(meadowMap(), 7);
        const GroundField field(*game.grid(), landscapeOfMap(meadowMap()));
        QCOMPARE(field.heightAt(5, 5), 0.0F);
        QCOMPARE(field.heightAt(16.4F, 10.4F), 0.0F);
        bool hilly = false;
        for (int step = -18; step < 28; ++step) {
            hilly = hilly || std::abs(field.heightAt(-14, static_cast<float>(step))) > 0.2F;
        }
        QVERIFY(hilly);
    }

    static void aWaterCellLiesBelowTheBoard()
    {
        GameSession game;
        game.startGame(meadowMap(), 7);
        const GroundField field(*game.grid(), landscapeOfMap(meadowMap()));
        QVERIFY(field.heightAt(2, 9) < 0);
        QVERIFY(field.isUnderWater(2, 9));
        QVERIFY(!field.isUnderWater(5, 5));
    }

    static void thePathLeavesTheBoard()
    {
        GameSession game;
        game.startGame(meadowMap(), 7);
        const GroundField field(*game.grid(), landscapeOfMap(meadowMap()));
        QCOMPARE(field.pathAt(3, 2), 1.0F);
        QCOMPARE(field.pathAt(3, 5), 0.0F);
        QCOMPARE(field.pathAt(-15, 2), 1.0F);
        QCOMPARE(field.pathAt(-15, 5), 0.0F);
        QCOMPARE(field.heightAt(-15, 2), 0.0F); // the road does not climb the hills
    }

    static void pathsLookDifferentFromGrass()
    {
        GameSession game;
        game.startGame(meadowMap(), 7);
        const Landscape landscape = landscapeOfMap(meadowMap());
        const GroundField field(*game.grid(), landscape);
        const QRgb path = field.colorAt(3, 2);
        const QRgb grass = field.colorAt(5, 5);
        QVERIFY(qRed(path) > qGreen(path));
        QVERIFY(qGreen(grass) > qRed(grass));
        QCOMPARE(GroundField(*game.grid(), landscape).colorAt(5, 5), grass);
    }

    static void thePlantsOfTheSurroundingsKeepOffTheBoardAndThePath()
    {
        GameSession game;
        game.startGame(meadowMap(), 7);
        LandscapeView view;
        view.setGame(&game);
        const int item = firstItemIn(view.landscape(), SceneryZone::Surroundings);
        OpenScenery scenery(&view, item);
        const std::vector<QVector3D> places = scenery.places();
        QCOMPARE(static_cast<int>(places.size()),
                 view.landscape().scenery[static_cast<std::size_t>(item)].count);
        for (const QVector3D &place : places) {
            QVERIFY(view.field()->distanceToGrid(place.x(), place.z()) > 3);
            QVERIFY(view.field()->pathAt(place.x(), place.z()) < 0.1F);
            QCOMPARE(place.y(), view.field()->heightAt(place.x(), place.z()));
        }
        QVERIFY(places == scenery.places());
    }

    static void aTowerTakesThePlaceOfThePlants()
    {
        GameSession game;
        game.startGame(meadowMap(), 7);
        LandscapeView view;
        view.setGame(&game);
        OpenScenery scenery(&view, firstItemIn(view.landscape(), SceneryZone::Board));
        const std::vector<QVector3D> before = scenery.places();
        QVERIFY(!before.empty());
        const QVector3D plant = before.back();
        const int cellX = qRound(plant.x());
        const int cellY = qRound(plant.z());
        QVERIFY(game.buildTower(cellX, cellY));
        const std::vector<QVector3D> after = scenery.places();
        QVERIFY(after.size() < before.size()); // and no new plant takes its place elsewhere
        for (const QVector3D &place : after) {
            QVERIFY(qRound(place.x()) != cellX || qRound(place.z()) != cellY);
        }
    }

    static void onlyPlantsOnTheBoardHearOfTowers()
    {
        GameSession game;
        game.startGame(meadowMap(), 7);
        LandscapeView view;
        view.setGame(&game);
        OpenScenery onBoard(&view, firstItemIn(view.landscape(), SceneryZone::Board));
        OpenScenery farAway(&view, firstItemIn(view.landscape(), SceneryZone::Surroundings));
        const QSignalSpy boardChanges(&onBoard, &QQuick3DInstancing::instanceTableChanged);
        const QSignalSpy farChanges(&farAway, &QQuick3DInstancing::instanceTableChanged);
        QVERIFY(game.buildTower(5, 5));
        QCOMPARE(boardChanges.count(), 1);
        QCOMPARE(farChanges.count(), 0);
    }

    static void thePlantsOfTheBorderStayOffTheBoard()
    {
        GameSession game;
        game.startGame(meadowMap(), 7);
        LandscapeView view;
        view.setGame(&game);
        OpenScenery scenery(&view, firstItemIn(view.landscape(), SceneryZone::Border));
        const std::vector<QVector3D> places = scenery.places();
        QVERIFY(!places.empty());
        for (const QVector3D &place : places) {
            const float distance = view.field()->distanceToGrid(place.x(), place.z());
            QVERIFY(distance > 0.5F && distance < 5);
        }
    }

    static void aPathAlongTheEdgeStaysOnTheBoard()
    {
        const QString map = QStringLiteral(QTD_CONTENT_DIR "/benchmark_arena.json");
        GameSession game;
        game.startGame(map, 7);
        const GroundField field(*game.grid(), landscapeOfMap(map));
        QCOMPARE(field.pathAt(12, 0), 1.0F);
        QCOMPARE(field.pathAt(12, -8), 0.0F);
        QCOMPARE(field.pathAt(12, 26), 0.0F);
        QCOMPARE(field.pathAt(-8, 0), 1.0F);  // but it leaves where it begins
        QCOMPARE(field.pathAt(-8, -8), 0.0F); // straight on, a corner is no wide road
        QCOMPARE(field.pathAt(0, -8), 0.0F);
    }

    static void aMapOfOneColumnHasGround()
    {
        const QString map = QStringLiteral(QTD_TEST_DIR "/narrow_map.json");
        GameSession game;
        game.startGame(map, 7);
        QCOMPARE(game.error(), QString());
        const GroundField field(*game.grid(), landscapeOfMap(map));
        QCOMPARE(field.pathAt(0, 3), 1.0F);
        QCOMPARE(field.pathAt(8, 3), 0.0F);
        QCOMPARE(field.pathAt(0, 15), 1.0F);
        QVERIFY(!field.picture(2).isEmpty());
    }

    static void theGroundFadesIntoThePlain()
    {
        GameSession game;
        game.startGame(meadowMap(), 7);
        const Landscape landscape = landscapeOfMap(meadowMap());
        const GroundField field(*game.grid(), landscape);
        const float edge = -static_cast<float>(kGroundMargin) - 0.5F;
        QCOMPARE(field.heightAt(edge, 20), 0.0F);
        QCOMPARE(QColor(field.colorAt(edge, 20)).rgb(), landscape.grassDark.rgb());
    }

    static void theMeadowHasNoLakes()
    {
        GameSession game;
        game.startGame(meadowMap(), 7);
        const GroundField field(*game.grid(), landscapeOfMap(meadowMap()));
        for (int step = -20; step < 30; ++step) {
            QVERIFY(!field.isUnderWater(static_cast<float>(step), -8));
        }
    }

    static void waterPlantsStandInTheWater()
    {
        GameSession game;
        game.startGame(swampMap(), 7);
        LandscapeView view;
        view.setGame(&game);
        OpenScenery scenery(&view, firstItemIn(view.landscape(), SceneryZone::Water));
        const std::vector<QVector3D> places = scenery.places();
        QVERIFY(!places.empty());
        for (const QVector3D &place : places) {
            QVERIFY(view.field()->isUnderWater(place.x(), place.z()));
            QCOMPARE(place.y(), view.waterLevel());
        }
    }

    static void aNewMapBringsItsLandscape()
    {
        GameSession game;
        game.startGame(meadowMap(), 7);
        LandscapeView view;
        view.setGame(&game);
        QVERIFY(!view.lakes());
        const int generation = view.generation();
        game.startGame(swampMap(), 7);
        QVERIFY(view.lakes());
        QCOMPARE(view.generation(), generation + 1);
        QCOMPARE(view.treeModel(), u"dark_tree"_s);
        QCOMPARE(view.sceneryModels().size(),
                 static_cast<qsizetype>(view.landscape().scenery.size()));
    }

    static void theMeshCoversTheBoardAndTheMargin()
    {
        GameSession game;
        game.startGame(meadowMap(), 7);
        LandscapeView view;
        view.setGame(&game);
        TerrainMesh mesh;
        mesh.setFieldSize(50);
        mesh.setView(&view);
        const int columns = (17 + 2 * kGroundMargin) * 2 + 1;
        const int rows = (11 + 2 * kGroundMargin) * 2 + 1;
        QCOMPARE(mesh.vertexData().size() / mesh.stride(), qsizetype{columns} * rows);
        QCOMPARE(mesh.indexData().size() / qsizetype{sizeof(quint32)},
                 qsizetype{columns - 1} * (rows - 1) * 6);
        QCOMPARE(mesh.boundsMin().x(), (-kGroundMargin - 0.5F) * 50);

        // Qt shows the front of a triangle, the ground has to face the sky.
        const QByteArray data = mesh.vertexData();
        const char *vertices = data.constData();
        const auto corner = [&](int index) {
            quint32 vertex = 0;
            std::memcpy(&vertex,
                        mesh.indexData().constData() + index * qsizetype{sizeof(vertex)},
                        sizeof(vertex));
            QVector3D place;
            std::memcpy(&place, vertices + qsizetype{vertex} * mesh.stride(), sizeof(place));
            return place;
        };
        QVERIFY(QVector3D::crossProduct(corner(1) - corner(0), corner(2) - corner(0)).y() > 0);
        QVector3D normal;
        std::memcpy(&normal, vertices + sizeof(QVector3D), sizeof(normal));
        QVERIFY(normal.y() > 0);
        QCOMPARE(mesh.boundsMax().z(), (10.5F + kGroundMargin) * 50);
    }

    static void thePictureHasEveryFieldOfTheGround()
    {
        GameSession game;
        game.startGame(meadowMap(), 7);
        LandscapeView view;
        view.setGame(&game);
        TerrainPicture picture;
        picture.setView(&view);
        const QSize size = picture.size();
        QCOMPARE(size.width() / (17 + 2 * kGroundMargin), size.height() / (11 + 2 * kGroundMargin));
        QCOMPARE(picture.textureData().size(), qsizetype{size.width()} * size.height() * 4);
    }

    static void thePictureShowsTheSameGroundAsTheField()
    {
        GameSession game;
        game.startGame(meadowMap(), 7);
        const GroundField field(*game.grid(), landscapeOfMap(meadowMap()));
        constexpr int pixels = 4;
        const QByteArray picture = field.picture(pixels);
        const int width = (17 + 2 * kGroundMargin) * pixels;
        const int height = (11 + 2 * kGroundMargin) * pixels;
        QCOMPARE(picture.size(), qsizetype{width} * height * 4);
        const auto redOf = [&](int x, int y) {
            const int column = (x + kGroundMargin) * pixels + pixels / 2;
            const int row = height - 1 - ((y + kGroundMargin) * pixels + pixels / 2);
            return static_cast<quint8>(picture[(qsizetype{row} * width + column) * 4]);
        };
        const auto greenOf = [&](int x, int y) {
            const int column = (x + kGroundMargin) * pixels + pixels / 2;
            const int row = height - 1 - ((y + kGroundMargin) * pixels + pixels / 2);
            return static_cast<quint8>(picture[(qsizetype{row} * width + column) * 4 + 1]);
        };
        QVERIFY(redOf(3, 2) > greenOf(3, 2)); // the path, and the north is not upside down
        QVERIFY(greenOf(5, 5) > redOf(5, 5)); // the grass
    }

    static void theCellOfTheGridIsAFrame()
    {
        const GridCellPicture picture;
        const QByteArray pixels = picture.textureData();
        const int width = picture.size().width();
        const auto alpha = [&](int x, int y) {
            return static_cast<quint8>(pixels[(y * width + x) * 4 + 3]);
        };
        QCOMPARE(alpha(0, width / 2), quint8{255});
        QCOMPARE(alpha(width / 2, width - 1), quint8{255});
        QCOMPARE(alpha(width / 2, width / 2), quint8{0});
    }
};

QTEST_GUILESS_MAIN(TstLandscape)
#include "tst_landscape.moc"
