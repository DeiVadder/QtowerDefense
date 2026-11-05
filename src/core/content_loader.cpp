#include "core/content_loader.h"

#include "core/json_reader.h"

#include <QFile>
#include <QJsonObject>
#include <QLatin1StringView>

#include <algorithm>
#include <iterator>
#include <optional>
#include <set>
#include <utility>

using namespace Qt::StringLiterals;

namespace qtd::core {

namespace {

constexpr qint64 kFormatVersion = 1;
// These limits keep every later computation inside the range of Fixed: squared distances stay near
// 1e9, and the spread of a damage roll fits the 32 bits of the generator in raw units.
constexpr qint64 kMaxGridSize = 256;
constexpr qint64 kMaxFootprintReach = 8;
constexpr qint64 kMaxRange = 30'000;
constexpr qint64 kMaxSpeed = 1'000;
constexpr qint64 kMaxDamage = 100'000;

/// A cell on the grid. The grid has at least one cell here, so a failed read still gives one.
Cell readCell(ObjectReader &item, const Grid &grid)
{
    const qint64 x = item.integer("x"_L1, 0, grid.width() - 1);
    return {x, item.integer("y"_L1, 0, grid.height() - 1)};
}

std::optional<Terrain> obstacleNamed(const QString &name)
{
    if (name == "rock"_L1) {
        return Terrain::Rock;
    }
    if (name == "tree"_L1) {
        return Terrain::Tree;
    }
    if (name == "water"_L1) {
        return Terrain::Water;
    }
    return std::nullopt;
}

template <typename Type>
void rejectDuplicateIds(const ObjectReader &reader,
                        const QString &key,
                        const std::vector<Type> &types)
{
    std::set<QString> seen;
    for (const Type &type : types) {
        if (!seen.insert(type.id).second) {
            reader.fail(key, u"id \"%1\" is used twice"_s.arg(type.id));
        }
    }
}

void readGrid(ObjectReader &root, Content *content)
{
    const qint64 width = root.integer("gridWidth"_L1, 1, kMaxGridSize);
    content->grid = Grid(width, root.integer("gridHeight"_L1, 1, kMaxGridSize));
}

/// Marks the cells of the straight lines through the corners.
void markPath(const std::vector<Cell> &corners, Grid *grid)
{
    Cell cell = corners.front();
    grid->setTerrain(cell, Terrain::Path);
    for (const Cell corner : corners) {
        while (cell != corner) {
            cell.x += (corner.x > cell.x) - (corner.x < cell.x);
            cell.y += (corner.y > cell.y) - (corner.y < cell.y);
            grid->setTerrain(cell, Terrain::Path);
        }
    }
}

void readPath(ObjectReader &root, Content *content)
{
    std::vector<Cell> corners;
    root.forEach("path"_L1, [&](ObjectReader &item) {
        const Cell corner = readCell(item, content->grid);
        if (!corners.empty() && corner == corners.back()) {
            item.fail(u"x"_s, u"repeats the point before it"_s);
        } else if (!corners.empty() && corner.x != corners.back().x &&
                   corner.y != corners.back().y) {
            item.fail(u"x"_s, u"must share a row or column with the point before it"_s);
        }
        corners.push_back(corner);
    });
    if (corners.size() < 2) {
        root.fail(u"path"_s, u"needs at least two points"_s);
    }
    if (root.failed()) {
        return;
    }
    markPath(corners, &content->grid);
    std::vector<Point> points;
    std::ranges::transform(corners, std::back_inserter(points), centreOf);
    content->path = Path(std::move(points));
}

void readObstacles(ObjectReader &root, Content *content)
{
    const auto read = [content](ObjectReader &item) {
        const Cell cell = readCell(item, content->grid);
        const QString name = item.text("terrain"_L1);
        const std::optional<Terrain> terrain = obstacleNamed(name);
        if (!terrain) {
            item.fail(u"terrain"_s, u"unknown obstacle \"%1\""_s.arg(name));
        } else if (content->grid.terrainAt(cell) != Terrain::Open) {
            item.fail(u"x"_s, u"the cell is not free"_s);
        } else {
            content->grid.setTerrain(cell, *terrain);
        }
    };
    root.forEach("obstacles"_L1, read, ObjectReader::Items::MayBeEmpty);
}

void readFootprint(ObjectReader &tower, TowerType *type)
{
    tower.forEach("footprint"_L1, [type](ObjectReader &item) {
        const qint64 x = item.integer("x"_L1, -kMaxFootprintReach, kMaxFootprintReach);
        const Cell cell{x, item.integer("y"_L1, -kMaxFootprintReach, kMaxFootprintReach)};
        if (std::ranges::contains(type->footprint, cell)) {
            item.fail(u"x"_s, u"repeats a cell of the footprint"_s);
        }
        type->footprint.push_back(cell);
    });
    if (!std::ranges::contains(type->footprint, Cell{})) {
        tower.fail(u"footprint"_s, u"needs the cell 0, 0 that the tower stands on"_s);
    }
}

void readTowers(ObjectReader &root, Content *content)
{
    const Fixed smallest = Fixed::fromRaw(1);
    const Fixed maxRange = Fixed::fromInt(kMaxRange);
    const Fixed maxDamage = Fixed::fromInt(kMaxDamage);
    root.forEach("towers"_L1, [&](ObjectReader &item) {
        TowerType tower;
        tower.id = item.text("id"_L1);
        tower.cost = item.integer("cost"_L1, 0, kMaxInteger);
        tower.range = item.fixed("range"_L1, smallest, maxRange);
        tower.cooldownTicks = item.integer("cooldownTicks"_L1, 1, kMaxInteger);
        tower.damageMin = item.fixed("damageMin"_L1, smallest, maxDamage);
        tower.damageMax = item.fixed("damageMax"_L1, smallest, maxDamage);
        if (tower.damageMin > tower.damageMax) {
            item.fail(u"damageMax"_s, u"must not be below damageMin"_s);
        }
        readFootprint(item, &tower);
        content->towers.push_back(std::move(tower));
    });
    rejectDuplicateIds(root, u"towers"_s, content->towers);
}

void readEnemies(ObjectReader &root, Content *content)
{
    const Fixed smallest = Fixed::fromRaw(1);
    const Fixed maxHealth = Fixed::fromInt(kMaxInteger);
    const Fixed maxSpeed = Fixed::fromInt(kMaxSpeed);
    root.forEach("enemies"_L1, [&](ObjectReader &item) {
        EnemyType enemy;
        enemy.id = item.text("id"_L1);
        enemy.health = item.fixed("health"_L1, smallest, maxHealth);
        enemy.speedPerTick = item.fixed("speedPerTick"_L1, smallest, maxSpeed);
        enemy.bounty = item.integer("bounty"_L1, 0, kMaxInteger);
        enemy.lifeCost = item.integer("lifeCost"_L1, 1, kMaxInteger);
        content->enemies.push_back(std::move(enemy));
    });
    rejectDuplicateIds(root, u"enemies"_s, content->enemies);
}

void readWave(ObjectReader &root, Content *content)
{
    root.forEach("wave"_L1, [content](ObjectReader &item) {
        SpawnGroup group;
        const QString enemy = item.text("enemy"_L1);
        const std::vector<EnemyType>::const_iterator found =
            std::ranges::find(content->enemies, enemy, &EnemyType::id);
        if (found == content->enemies.end()) {
            item.fail(u"enemy"_s, u"unknown enemy \"%1\""_s.arg(enemy));
        } else {
            group.enemyType = static_cast<std::size_t>(found - content->enemies.begin());
        }
        group.count = item.integer("count"_L1, 1, kMaxInteger);
        group.delayTicks = item.integer("delayTicks"_L1, 0, kMaxInteger);
        group.intervalTicks = item.integer("intervalTicks"_L1, 1, kMaxInteger);
        content->wave.push_back(group);
    });
}

ContentLoadResult failure(QString error)
{
    return {std::nullopt, std::move(error)};
}

} // namespace

ContentLoadResult parseContent(QByteArrayView json)
{
    QString error;
    const std::optional<QJsonObject> object = parseExactJsonObject(json, &error);
    if (!object) {
        return failure(std::move(error));
    }
    ObjectReader root(*object, QString(), &error);
    root.integer("formatVersion"_L1, kFormatVersion, kFormatVersion);
    Content content;
    content.startGold = root.integer("startGold"_L1, 0, kMaxInteger);
    content.startLives = root.integer("startLives"_L1, 1, kMaxInteger);
    readGrid(root, &content);
    if (!root.failed()) {
        readPath(root, &content);
    }
    if (!root.failed()) {
        readObstacles(root, &content);
    }
    readTowers(root, &content);
    readEnemies(root, &content);
    readWave(root, &content);
    root.rejectUnknownKeys();
    if (!error.isEmpty()) {
        return failure(std::move(error));
    }
    return {std::move(content), QString()};
}

ContentLoadResult loadContentFile(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        return failure(u"%1: %2"_s.arg(fileName, file.errorString()));
    }
    ContentLoadResult result = parseContent(file.readAll());
    if (!result.content) {
        result.error.prepend(fileName + u": "_s);
    }
    return result;
}

} // namespace qtd::core
