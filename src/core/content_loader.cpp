#include "core/content_loader.h"

#include "core/json_reader.h"

#include <QFile>
#include <QJsonObject>
#include <QLatin1StringView>

#include <algorithm>
#include <set>
#include <utility>

using namespace Qt::StringLiterals;

namespace qtd::core {

namespace {

constexpr qint64 kFormatVersion = 1;
// These limits keep every later computation inside the range of Fixed: squared distances stay near
// 1e9, and the spread of a damage roll fits the 32 bits of the generator in raw units.
constexpr qint64 kMaxCoordinate = 10'000;
constexpr qint64 kMaxRange = 30'000;
constexpr qint64 kMaxSpeed = 1'000;
constexpr qint64 kMaxDamage = 100'000;

Point readPoint(ObjectReader &item)
{
    const Fixed limit = Fixed::fromInt(kMaxCoordinate);
    const Fixed x = item.fixed("x"_L1, -limit, limit);
    return {x, item.fixed("y"_L1, -limit, limit)};
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

void readPath(ObjectReader &root, Content *content)
{
    std::vector<Point> points;
    root.forEach("path"_L1, [&points](ObjectReader &item) {
        points.push_back(readPoint(item));
        if (points.size() > 1 && points[points.size() - 2] == points.back()) {
            item.fail(u"x"_s, u"repeats the point before it"_s);
        }
    });
    if (points.size() < 2) {
        root.fail(u"path"_s, u"needs at least two points"_s);
    }
    content->path = Path(std::move(points));
}

void readBuildSlots(ObjectReader &root, Content *content)
{
    root.forEach("buildSlots"_L1, [content](ObjectReader &item) {
        const Point slot = readPoint(item);
        if (std::ranges::find(content->buildSlots, slot) != content->buildSlots.end()) {
            item.fail(u"x"_s, u"another build slot has the same place"_s);
        }
        content->buildSlots.push_back(slot);
    });
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
    readPath(root, &content);
    readBuildSlots(root, &content);
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
