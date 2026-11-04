#include "core/content_loader.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QLatin1StringView>
#include <QStringList>

#include <algorithm>
#include <functional>
#include <set>
#include <utility>

using namespace Qt::StringLiterals;

namespace qtd::core {

namespace {

constexpr qint64 kFormatVersion = 1;
constexpr qint64 kMaxInteger = 1'000'000'000;
constexpr qsizetype kMaxIntegerDigits = 18; // always fits a qint64, so Qt keeps it exact

// These limits keep every later computation inside the range of Fixed: squared distances stay near
// 1e9, and the spread of a damage roll fits the 32 bits of the generator in raw units.
constexpr qint64 kMaxCoordinate = 10'000;
constexpr qint64 kMaxRange = 30'000;
constexpr qint64 kMaxSpeed = 1'000;
constexpr qint64 kMaxDamage = 100'000;

bool isDigit(char c)
{
    return c >= '0' && c <= '9';
}

/// Includes the leading point: Qt accepts .5, and toInteger() would give 0.
bool startsNumber(char c)
{
    return isDigit(c) || c == '-' || c == '+' || c == '.';
}

bool isNumberPart(char c)
{
    return isDigit(c) || c == '-' || c == '+' || c == '.' || c == 'e' || c == 'E';
}

/// The error text for a number token of the JSON text, or nothing if Qt reads it exactly.
QString checkNumber(QByteArrayView number, qsizetype line)
{
    const QByteArrayView digits = number.sliced(number.front() == '-' ? 1 : 0);
    if (!std::ranges::all_of(digits, isDigit)) {
        return u"line %1: number %2 is no integer, write decimal values as strings like \"1.5\""_s
            .arg(line)
            .arg(QString::fromLatin1(number));
    }
    if (digits.size() > kMaxIntegerDigits) {
        return u"line %1: number %2 is too large"_s.arg(line).arg(QString::fromLatin1(number));
    }
    return {};
}

/// QJsonValue holds a fraction or a huge number as floating point, which rounds it before anyone
/// can look. Hence this pass over the text of a valid document. Returns an error text or nothing.
QString findInexactNumber(QByteArrayView json)
{
    qsizetype line = 1;
    bool inString = false;
    for (qsizetype i = 0; i < json.size(); ++i) {
        const char c = json[i];
        if (c == '\n') {
            ++line;
        } else if (inString) {
            i += c == '\\' ? 1 : 0;
            inString = c != '"';
        } else if (c == '"') {
            inString = true;
        } else if (startsNumber(c)) {
            const qsizetype start = i;
            while (i + 1 < json.size() && isNumberPart(json[i + 1])) {
                ++i;
            }
            QString error = checkNumber(json.sliced(start, i - start + 1), line);
            if (!error.isEmpty()) {
                return error;
            }
        }
    }
    return {};
}

/// Reads one JSON object. The first problem is kept and later reads give harmless values, so the
/// code that reads a file stays free of error branches.
class ObjectReader
{
public:
    ObjectReader(QJsonObject object, QString place, QString *error)
        : object_(std::move(object)), place_(std::move(place)), error_(error)
    {
    }

    void fail(const QString &key, const QString &message) const
    {
        if (error_->isEmpty()) {
            *error_ = (place_.isEmpty() ? key : place_ + u'.' + key) + u": "_s + message;
        }
    }

    bool failed() const
    {
        return !error_->isEmpty();
    }

    qint64 integer(QLatin1StringView key, qint64 lowest, qint64 highest)
    {
        const QJsonValue value = take(key);
        if (value.isUndefined()) {
            return lowest;
        }
        const qint64 number = value.toInteger();
        if (!value.isDouble() || number < lowest || number > highest) {
            fail(key, u"must be an integer from %1 to %2"_s.arg(lowest).arg(highest));
            return lowest;
        }
        return number;
    }

    Fixed fixed(QLatin1StringView key, Fixed lowest, Fixed highest)
    {
        const QJsonValue value = take(key);
        if (value.isUndefined()) {
            return lowest;
        }
        std::optional<Fixed> number;
        if (value.isString()) {
            number = Fixed::parse(value.toString());
        } else if (value.isDouble() && qAbs(value.toInteger()) <= kMaxInteger) {
            number = Fixed::fromInt(value.toInteger());
        }
        if (!number || *number < lowest || *number > highest) {
            fail(key,
                 u"must be a number from %1 to %2 with at most %3 decimal places"_s
                     .arg(toString(lowest), toString(highest))
                     .arg(Fixed::kFractionDigits));
            return lowest;
        }
        return *number;
    }

    Point point()
    {
        const Fixed limit = Fixed::fromInt(kMaxCoordinate);
        const Fixed x = fixed("x"_L1, -limit, limit);
        return {x, fixed("y"_L1, -limit, limit)};
    }

    QString text(QLatin1StringView key)
    {
        const QJsonValue value = take(key);
        if (!value.isUndefined() && value.toString().isEmpty()) {
            fail(key, u"must be a text that is not empty"_s);
        }
        return value.toString();
    }

    /// Calls read for each object of an array that must not be empty.
    void forEach(QLatin1StringView key, const std::function<void(ObjectReader &)> &read)
    {
        const QJsonValue value = take(key);
        if (value.isUndefined()) {
            return;
        }
        const QJsonArray items = value.toArray();
        if (items.isEmpty()) {
            fail(key, u"must be an array that is not empty"_s);
        }
        for (qsizetype index = 0; index < items.size() && !failed(); ++index) {
            const QString name = u"%1[%2]"_s.arg(key).arg(index);
            if (!items[index].isObject()) {
                fail(name, u"must be an object"_s);
                return;
            }
            ObjectReader item(items[index].toObject(), place_ + name, error_);
            read(item);
            item.rejectUnknownKeys();
        }
    }

    void rejectUnknownKeys() const
    {
        const QStringList keys = object_.keys();
        for (const QString &key : keys) {
            if (!known_.contains(key)) {
                fail(key, u"unknown key"_s);
            }
        }
    }

private:
    QJsonValue take(QLatin1StringView key)
    {
        known_.insert(QString(key));
        const QJsonValue value = object_.value(key);
        if (value.isUndefined()) {
            fail(key, u"is missing"_s);
        }
        return value;
    }

    QJsonObject object_;
    QString place_;
    QString *error_;
    std::set<QString> known_;
};

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
        points.push_back(item.point());
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
        const Point slot = item.point();
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
        const auto found = std::ranges::find(content->enemies, enemy, &EnemyType::id);
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
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json.toByteArray(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        return failure(u"invalid JSON at offset %1: %2"_s.arg(parseError.offset)
                           .arg(parseError.errorString()));
    }
    if (!document.isObject()) {
        return failure(u"the top level of the content must be an object"_s);
    }
    QString error = findInexactNumber(json);
    if (!error.isEmpty()) {
        return failure(std::move(error));
    }
    ObjectReader root(document.object(), QString(), &error);
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
