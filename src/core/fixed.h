#pragma once

#include <QString>
#include <QStringView>
#include <QtNumeric>
#include <QtTypes>

#include <compare>
#include <limits>
#include <optional>
#include <stdexcept>

namespace qtd::core {

/// Thrown in Debug builds when a Fixed operation has no valid result. See E-14.
class FixedRangeError : public std::range_error
{
public:
    using std::range_error::range_error;
};

/// Decimal fixed point number: the raw integer counts ten-thousandths, so 1.5 is raw 15000.
///
/// Multiplication, division and square root round to the nearest raw value, ties away from zero.
/// A result out of range, a division by zero or the root of a negative number throws
/// FixedRangeError in Debug builds. Other builds give the limit of the range with the right sign,
/// or 0 for the root. The raw value is never the lowest qint64, so negation is always safe.
class Fixed
{
public:
    static constexpr qint64 kScale = 10'000;
    static constexpr int kFractionDigits = 4;

    constexpr Fixed() = default;

    static Fixed fromRaw(qint64 raw);
    static Fixed fromInt(qint64 value);

    /// Parses "-12.3456": an optional minus, digits, and optionally a dot with one to four digits.
    /// Anything else gives std::nullopt, also a value that would need rounding or does not fit.
    static std::optional<Fixed> parse(QStringView text);

    static constexpr Fixed max()
    {
        return Fixed(kMaxRaw);
    }
    static constexpr Fixed min()
    {
        return Fixed(-kMaxRaw);
    }

    constexpr qint64 raw() const
    {
        return raw_;
    }

    constexpr auto operator<=>(const Fixed &) const = default;

    constexpr Fixed operator-() const
    {
        return Fixed(-raw_);
    }

    friend Fixed operator+(Fixed left, Fixed right)
    {
        qint64 sum = 0;
        if (qAddOverflow(left.raw_, right.raw_, &sum) || sum < -kMaxRaw) {
            return rangeError("Fixed addition out of range", right.raw_ < 0 ? min() : max());
        }
        return Fixed(sum);
    }

    friend Fixed operator-(Fixed left, Fixed right)
    {
        return left + (-right);
    }

    /// The exact product of the raw values must fit a qint64: |left * right| below about 9.2e10.
    friend Fixed operator*(Fixed left, Fixed right);

    /// left.raw() * kScale must fit a qint64: |left| below about 9.2e10.
    friend Fixed operator/(Fixed left, Fixed right);

    /// The value must be at least 0 and below about 9.2e10.
    friend Fixed sqrt(Fixed value);

private:
    static constexpr qint64 kMaxRaw = std::numeric_limits<qint64>::max();

    explicit constexpr Fixed(qint64 raw) : raw_(raw) {}

    /// Throws in Debug builds. Elsewhere the result is the given replacement.
    static Fixed rangeError(const char *message, Fixed replacement);

    qint64 raw_ = 0;
};

/// The shortest decimal text of the value, as Fixed::parse reads it, such as "-1.25".
QString toString(Fixed value);

/// A point on the map, in tiles.
struct Point
{
    Fixed x;
    Fixed y;

    constexpr auto operator<=>(const Point &) const = default;
};

Fixed distanceSquared(Point from, Point to);
Fixed distance(Point from, Point to);

} // namespace qtd::core
