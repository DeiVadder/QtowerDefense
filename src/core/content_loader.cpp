#include "core/content_loader.h"

#include "core/json_reader.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLatin1StringView>

#include <algorithm>
#include <array>
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
constexpr qint64 kMaxPercent = 100;

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

DamageType readDamageType(ObjectReader &item)
{
    const QString name = item.text("damageType"_L1);
    const std::optional<DamageType> type = damageTypeNamed(name);
    if (!type) {
        item.fail(u"damageType"_s, u"unknown damage type \"%1\""_s.arg(name));
    }
    return type.value_or(DamageType::Physical);
}

/// An upgrade that names effects replaces those of the level before.
void readEffects(ObjectReader &tower, std::vector<StatusEffect> *effects)
{
    if (!tower.has("effects"_L1)) {
        return;
    }
    effects->clear();
    const auto read = [effects](ObjectReader &item) {
        const QString name = item.text("kind"_L1);
        const std::optional<StatusKind> kind = statusKindNamed(name);
        if (!kind) {
            item.fail(u"kind"_s, u"unknown status effect \"%1\""_s.arg(name));
            return;
        }
        if (std::ranges::contains(*effects, *kind, &StatusEffect::kind)) {
            item.fail(u"kind"_s, u"repeats a status effect"_s);
        }
        const Fixed strongest = *kind == StatusKind::Slow ? maxSlow() : Fixed::fromInt(kMaxDamage);
        const Fixed strength = item.fixed("strength"_L1, Fixed::fromRaw(1), strongest);
        effects->push_back({*kind, strength, item.integer("durationTicks"_L1, 1, kMaxInteger)});
    };
    tower.forEach("effects"_L1, read, ObjectReader::Items::MayBeEmpty);
}

void readResistances(ObjectReader &enemy, EnemyType *type)
{
    if (!enemy.has("resistances"_L1)) {
        return;
    }
    std::set<DamageType> seen;
    const auto read = [&](ObjectReader &item) {
        const DamageType damage = readDamageType(item);
        if (!seen.insert(damage).second) {
            item.fail(u"damageType"_s, u"repeats a resistance of this enemy"_s);
        }
        type->resistances[static_cast<std::size_t>(damage)] =
            item.fixed("share"_L1, Fixed::fromInt(-1), Fixed::fromInt(1));
    };
    enemy.forEach("resistances"_L1, read, ObjectReader::Items::MayBeEmpty);
}

/// The first level names all its values, an upgrade only what it changes.
void readLevel(ObjectReader &item, TowerType *type)
{
    const bool first = type->levels.empty();
    TowerLevel level = first ? TowerLevel{} : type->levels.back();
    const Fixed smallest = Fixed::fromRaw(1);
    const Fixed maxRange = Fixed::fromInt(kMaxRange);
    const Fixed maxDamage = Fixed::fromInt(kMaxDamage);
    const auto readFixed = [&](QLatin1StringView key, Fixed highest, Fixed *value) {
        if (first || item.has(key)) {
            *value = item.fixed(key, smallest, highest);
        }
    };
    level.cost = item.integer("cost"_L1, 0, kMaxInteger);
    readFixed("range"_L1, maxRange, &level.range);
    if (first || item.has("cooldownTicks"_L1)) {
        level.cooldownTicks = item.integer("cooldownTicks"_L1, 1, kMaxInteger);
    }
    readFixed("damageMin"_L1, maxDamage, &level.damageMin);
    readFixed("damageMax"_L1, maxDamage, &level.damageMax);
    if (level.damageMin > level.damageMax) {
        item.fail(u"damageMax"_s, u"must not be below damageMin"_s);
    }
    if (item.has("splashRadius"_L1)) {
        level.splashRadius = item.fixed("splashRadius"_L1, smallest, maxRange);
    }
    readEffects(item, &level.effects);
    type->levels.push_back(std::move(level));
}

void readUpgrades(ObjectReader &tower, TowerType *type)
{
    if (tower.has("upgrades"_L1)) {
        tower.forEach(
            "upgrades"_L1,
            [type](ObjectReader &item) { readLevel(item, type); },
            ObjectReader::Items::MayBeEmpty);
    }
}

void readTowers(ObjectReader &root, Content *content)
{
    root.forEach("towers"_L1, [&](ObjectReader &item) {
        TowerType tower;
        tower.id = item.text("id"_L1);
        readLevel(item, &tower);
        tower.damageType = readDamageType(item);
        readFootprint(item, &tower);
        readUpgrades(item, &tower);
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
        if (item.has("regeneration"_L1)) {
            enemy.regeneration = item.fixed("regeneration"_L1, smallest, maxHealth);
        }
        enemy.boss = item.has("boss"_L1) && item.flag("boss"_L1);
        readResistances(item, &enemy);
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

void readSpells(ObjectReader &root, Content *content)
{
    if (!root.has("spells"_L1)) {
        return;
    }
    const Fixed maxDamage = Fixed::fromInt(kMaxDamage);
    const auto read = [&](ObjectReader &item) {
        SpellType spell;
        spell.id = item.text("id"_L1);
        spell.cooldownTicks = item.integer("cooldownTicks"_L1, 1, kMaxInteger);
        spell.radius = item.fixed("radius"_L1, Fixed::fromRaw(1), Fixed::fromInt(kMaxRange));
        spell.damageMin = item.fixed("damageMin"_L1, Fixed(), maxDamage);
        spell.damageMax = item.fixed("damageMax"_L1, Fixed(), maxDamage);
        if (spell.damageMin > spell.damageMax) {
            item.fail(u"damageMax"_s, u"must not be below damageMin"_s);
        }
        spell.damageType = readDamageType(item);
        readEffects(item, &spell.effects);
        content->spells.push_back(std::move(spell));
    };
    root.forEach("spells"_L1, read, ObjectReader::Items::MayBeEmpty);
    rejectDuplicateIds(root, u"spells"_s, content->spells);
}

ContentLoadResult failure(QString error)
{
    return {std::nullopt, std::move(error)};
}

/// What a map takes from a catalog (E-37): what can be built, what comes and what is cast.
constexpr std::array kCatalogKeys{"towers"_L1, "enemies"_L1, "spells"_L1};

/// Reads a whole file, or gives an error text that starts with its name.
std::optional<QByteArray> readWhole(const QString &fileName, QString *error)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = u"%1: %2"_s.arg(fileName, file.errorString());
        return std::nullopt;
    }
    return file.readAll();
}

/// Validates a catalog on its own, so that its errors name its own file.
QString catalogError(const QJsonObject &catalog)
{
    QString error;
    ObjectReader root(catalog, QString(), &error);
    root.integer("formatVersion"_L1, kFormatVersion, kFormatVersion);
    Content content;
    readTowers(root, &content);
    readEnemies(root, &content);
    readSpells(root, &content);
    root.rejectUnknownKeys();
    return error;
}

/// Puts the catalog a map names into its JSON. A map without one stays as it is, an invalid map
/// is left to parseContent. Gives an error text that names the file at fault.
QString addCatalog(const QString &fileName, QByteArray *json)
{
    QString error;
    std::optional<QJsonObject> map = parseExactJsonObject(*json, &error);
    if (!map || !map->contains("catalog"_L1)) {
        return {};
    }
    if (!map->value("catalog"_L1).isString()) {
        return u"%1: catalog must be a file name"_s.arg(fileName);
    }
    const std::array<QLatin1StringView, 3>::const_iterator own = std::ranges::find_if(
        kCatalogKeys, [&map](QLatin1StringView key) { return map->contains(key); });
    if (own != kCatalogKeys.end()) {
        return u"%1: %2 come from the catalog"_s.arg(fileName, *own);
    }
    const QString catalogFile =
        QFileInfo(fileName).dir().filePath(map->take("catalog"_L1).toString());
    const std::optional<QByteArray> text = readWhole(catalogFile, &error);
    if (!text) {
        return u"%1: %2"_s.arg(fileName, error);
    }
    const std::optional<QJsonObject> catalog = parseExactJsonObject(*text, &error);
    if (!catalog) {
        return u"%1: %2"_s.arg(catalogFile, error);
    }
    error = catalogError(*catalog);
    if (!error.isEmpty()) {
        return u"%1: %2"_s.arg(catalogFile, error);
    }
    for (const QLatin1StringView key : kCatalogKeys) {
        if (catalog->contains(key)) {
            map->insert(key, catalog->value(key));
        }
    }
    *json = QJsonDocument(*map).toJson(QJsonDocument::Compact);
    return {};
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
    if (root.has("sellPercent"_L1)) {
        content.sellPercent = root.integer("sellPercent"_L1, 0, kMaxPercent);
    }
    if (root.has("earlyCallTicksPerGold"_L1)) {
        content.earlyCallTicksPerGold = root.integer("earlyCallTicksPerGold"_L1, 1, kMaxInteger);
    }
    if (root.has("rollPercent"_L1)) {
        // Below all: a roll is always cheaper than the card it may give.
        content.rollPercent = root.integer("rollPercent"_L1, 1, kMaxPercent - 1);
    }
    if (root.has("coinPercent"_L1)) {
        content.coinPercent = root.integer("coinPercent"_L1, 0, kMaxPercent);
    }
    if (root.has("coinTicks"_L1)) {
        content.coinTicks = root.integer("coinTicks"_L1, 1, kMaxInteger);
    }
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
    readSpells(root, &content);
    root.rejectUnknownKeys();
    if (!error.isEmpty()) {
        return failure(std::move(error));
    }
    return {std::move(content), QString()};
}

ContentLoadResult loadContentFile(const QString &fileName)
{
    QString error;
    std::optional<QByteArray> json = readWhole(fileName, &error);
    if (!json) {
        return failure(std::move(error));
    }
    error = addCatalog(fileName, &*json);
    if (!error.isEmpty()) {
        return failure(std::move(error));
    }
    ContentLoadResult result = parseContent(*json);
    if (!result.content) {
        result.error.prepend(fileName + u": "_s);
    }
    return result;
}

} // namespace qtd::core
