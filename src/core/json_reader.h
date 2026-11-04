#pragma once

#include "core/fixed.h"

#include <QByteArrayView>
#include <QJsonObject>
#include <QJsonValue>
#include <QLatin1StringView>
#include <QString>
#include <QtTypes>

#include <functional>
#include <optional>
#include <set>

namespace qtd::core {

constexpr qint64 kMaxInteger = 1'000'000'000;

/// The largest integer in a data file: 18 digits always fit a qint64, so Qt keeps them exact.
constexpr qint64 kMaxExactInteger = 999'999'999'999'999'999;

/// The top-level object of a JSON text whose numbers Qt reads without rounding (E-16), or nothing
/// and an error text.
std::optional<QJsonObject> parseExactJsonObject(QByteArrayView json, QString *error);

/// Reads one JSON object. The first problem is kept and later reads give harmless values, so the
/// code that reads a file stays free of error branches.
class ObjectReader
{
public:
    enum class Items : quint8 {
        AtLeastOne,
        MayBeEmpty,
    };

    ObjectReader(QJsonObject object, QString place, QString *error);

    void fail(const QString &key, const QString &message) const;
    bool failed() const
    {
        return !error_->isEmpty();
    }
    const QString &error() const
    {
        return *error_;
    }

    /// Whether an optional key is there. It counts as known either way.
    bool has(QLatin1StringView key);

    qint64 integer(QLatin1StringView key, qint64 lowest, qint64 highest);
    Fixed fixed(QLatin1StringView key, Fixed lowest, Fixed highest);
    QString text(QLatin1StringView key);

    /// Calls read for each object of an array.
    void forEach(QLatin1StringView key,
                 const std::function<void(ObjectReader &)> &read,
                 Items items = Items::AtLeastOne);

    void rejectUnknownKeys() const;

private:
    QJsonValue take(QLatin1StringView key);

    QJsonObject object_;
    QString place_;
    QString *error_;
    std::set<QString> known_;
};

} // namespace qtd::core
