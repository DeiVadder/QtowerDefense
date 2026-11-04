#include "core/fixed.h"

namespace qtd::core {

namespace {

/// left * right, or nothing if it does not fit. Neither value is the lowest qint64. This is not
/// qMulOverflow: for qint64, its header gives the MSVC warning C4702 in every program that uses it.
std::optional<qint64> product(qint64 left, qint64 right)
{
    if (right != 0 && qAbs(left) > std::numeric_limits<qint64>::max() / qAbs(right)) {
        return std::nullopt;
    }
    return left * right;
}

/// numerator / denominator, rounded to nearest, ties away from zero. The denominator is not 0 and
/// neither value is the lowest qint64.
qint64 roundedQuotient(qint64 numerator, qint64 denominator)
{
    const qint64 quotient = numerator / denominator;
    const qint64 remainder = qAbs(numerator % denominator);
    if (remainder < qAbs(denominator) - remainder) {
        return quotient;
    }
    return (numerator < 0) == (denominator < 0) ? quotient + 1 : quotient - 1;
}

/// The integer square root by the binary digit method, rounded to nearest.
quint64 roundedRoot(quint64 value)
{
    quint64 rest = value;
    quint64 root = 0;
    quint64 bit = quint64(1) << 62U;
    while (bit > rest) {
        bit >>= 2U;
    }
    while (bit != 0) {
        if (rest >= root + bit) {
            rest -= root + bit;
            root = (root >> 1U) + bit;
        } else {
            root >>= 1U;
        }
        bit >>= 2U;
    }
    return rest > root ? root + 1 : root; // (root + 0.5)^2 = root^2 + root + 0.25
}

bool appendDigits(QStringView digits, qint64 *value)
{
    for (const QChar digit : digits) {
        const std::optional<qint64> shifted = product(*value, 10);
        if (digit < QLatin1Char('0') || digit > QLatin1Char('9') || !shifted ||
            qAddOverflow(*shifted, qint64(digit.unicode() - u'0'), value)) {
            return false;
        }
    }
    return true;
}

} // namespace

Fixed Fixed::rangeError(const char *message, Fixed replacement)
{
#ifdef QT_NO_DEBUG
    Q_UNUSED(message)
    return replacement;
#else
    Q_UNUSED(replacement)
    throw FixedRangeError(message);
#endif
}

Fixed Fixed::fromRaw(qint64 raw)
{
    if (raw < -kMaxRaw) {
        return rangeError("Fixed raw value out of range", min());
    }
    return Fixed(raw);
}

Fixed Fixed::fromInt(qint64 value)
{
    const std::optional<qint64> scaled = value < -kMaxRaw ? std::nullopt : product(value, kScale);
    if (!scaled) {
        return rangeError("Fixed integer out of range", value < 0 ? min() : max());
    }
    return Fixed(*scaled);
}

std::optional<Fixed> Fixed::parse(QStringView text)
{
    const bool negative = text.startsWith(QLatin1Char('-'));
    const QStringView digits = negative ? text.sliced(1) : text;
    const qsizetype dot = digits.indexOf(QLatin1Char('.'));
    const QStringView whole = dot < 0 ? digits : digits.first(dot);
    const QStringView fraction = dot < 0 ? QStringView() : digits.sliced(dot + 1);
    if (whole.isEmpty() || (dot >= 0 && fraction.isEmpty()) || fraction.size() > kFractionDigits) {
        return std::nullopt;
    }
    const QStringView padding = QStringView(u"0000").first(kFractionDigits - fraction.size());
    qint64 number = 0;
    if (!appendDigits(whole, &number) || !appendDigits(fraction, &number) ||
        !appendDigits(padding, &number)) {
        return std::nullopt;
    }
    return Fixed(negative ? -number : number);
}

Fixed operator*(Fixed left, Fixed right)
{
    const std::optional<qint64> exact = product(left.raw_, right.raw_);
    if (!exact) {
        const bool negative = (left.raw_ < 0) != (right.raw_ < 0);
        return Fixed::rangeError("Fixed product out of range",
                                 negative ? Fixed::min() : Fixed::max());
    }
    return Fixed(roundedQuotient(*exact, Fixed::kScale));
}

Fixed operator/(Fixed left, Fixed right)
{
    const bool negative = (left.raw_ < 0) != (right.raw_ < 0);
    const Fixed limit = negative ? Fixed::min() : Fixed::max();
    if (right.raw_ == 0) {
        return Fixed::rangeError("Fixed division by zero", limit);
    }
    const std::optional<qint64> numerator = product(left.raw_, Fixed::kScale);
    if (!numerator) {
        return Fixed::rangeError("Fixed quotient out of range", limit);
    }
    return Fixed(roundedQuotient(*numerator, right.raw_));
}

Fixed sqrt(Fixed value)
{
    if (value.raw_ < 0) {
        return Fixed::rangeError("Fixed root of a negative number", Fixed());
    }
    const std::optional<qint64> scaled = product(value.raw_, Fixed::kScale);
    if (!scaled) { // sqrt(raw / scale) * scale = sqrt(raw * scale)
        return Fixed::rangeError("Fixed root out of range", Fixed::max());
    }
    return Fixed(static_cast<qint64>(roundedRoot(static_cast<quint64>(*scaled))));
}

QString toString(Fixed value)
{
    const qint64 magnitude = qAbs(value.raw());
    QString text = QString::number(magnitude / Fixed::kScale);
    if (value.raw() < 0) {
        text.prepend(QLatin1Char('-'));
    }
    const qint64 fraction = magnitude % Fixed::kScale;
    if (fraction == 0) {
        return text;
    }
    QString digits =
        QString::number(fraction).rightJustified(Fixed::kFractionDigits, QLatin1Char('0'));
    while (digits.endsWith(QLatin1Char('0'))) {
        digits.chop(1);
    }
    return text + QLatin1Char('.') + digits;
}

Fixed distanceSquared(Point from, Point to)
{
    const Fixed x = to.x - from.x;
    const Fixed y = to.y - from.y;
    return x * x + y * y;
}

Fixed distance(Point from, Point to)
{
    return sqrt(distanceSquared(from, to));
}

} // namespace qtd::core
