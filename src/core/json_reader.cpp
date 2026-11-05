#include "core/json_reader.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QStringList>

#include <algorithm>
#include <utility>

using namespace Qt::StringLiterals;

namespace qtd::core {

namespace {

constexpr qsizetype kMaxIntegerDigits = 18; // see kMaxExactInteger

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

} // namespace

std::optional<QJsonObject> parseExactJsonObject(QByteArrayView json, QString *error)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json.toByteArray(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        *error =
            u"invalid JSON at offset %1: %2"_s.arg(parseError.offset).arg(parseError.errorString());
        return std::nullopt;
    }
    if (!document.isObject()) {
        *error = u"the top level must be an object"_s;
        return std::nullopt;
    }
    *error = findInexactNumber(json);
    if (!error->isEmpty()) {
        return std::nullopt;
    }
    return document.object();
}

ObjectReader::ObjectReader(QJsonObject object, QString place, QString *error)
    : object_(std::move(object)), place_(std::move(place)), error_(error)
{
}

void ObjectReader::fail(const QString &key, const QString &message) const
{
    if (error_->isEmpty()) {
        *error_ = (place_.isEmpty() ? key : place_ + u'.' + key) + u": "_s + message;
    }
}

bool ObjectReader::has(QLatin1StringView key)
{
    known_.insert(QString(key));
    return object_.contains(key);
}

qint64 ObjectReader::integer(QLatin1StringView key, qint64 lowest, qint64 highest)
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

Fixed ObjectReader::fixed(QLatin1StringView key, Fixed lowest, Fixed highest)
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

QString ObjectReader::text(QLatin1StringView key)
{
    const QJsonValue value = take(key);
    if (!value.isUndefined() && value.toString().isEmpty()) {
        fail(key, u"must be a text that is not empty"_s);
    }
    return value.toString();
}

bool ObjectReader::flag(QLatin1StringView key)
{
    const QJsonValue value = take(key);
    if (!value.isUndefined() && !value.isBool()) {
        fail(key, u"must be true or false"_s);
    }
    return value.toBool();
}

void ObjectReader::forEach(QLatin1StringView key,
                           const std::function<void(ObjectReader &)> &read,
                           Items items)
{
    const QJsonValue value = take(key);
    if (value.isUndefined()) {
        return;
    }
    const QJsonArray array = value.toArray();
    if (!value.isArray() || (items == Items::AtLeastOne && array.isEmpty())) {
        fail(key,
             items == Items::AtLeastOne ? u"must be an array that is not empty"_s
                                        : u"must be an array"_s);
        return;
    }
    for (qsizetype index = 0; index < array.size() && !failed(); ++index) {
        const QString name = u"%1[%2]"_s.arg(key).arg(index);
        if (!array[index].isObject()) {
            fail(name, u"must be an object"_s);
            return;
        }
        ObjectReader item(
            array[index].toObject(), place_.isEmpty() ? name : place_ + u'.' + name, error_);
        read(item);
        item.rejectUnknownKeys();
    }
}

void ObjectReader::rejectUnknownKeys() const
{
    const QStringList keys = object_.keys();
    for (const QString &key : keys) {
        if (!known_.contains(key)) {
            fail(key, u"unknown key"_s);
        }
    }
}

QJsonValue ObjectReader::take(QLatin1StringView key)
{
    known_.insert(QString(key));
    const QJsonValue value = object_.value(key);
    if (value.isUndefined()) {
        fail(key, u"is missing"_s);
    }
    return value;
}

} // namespace qtd::core
