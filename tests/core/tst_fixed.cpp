#include "core/fixed.h"

#include <QTest>

#include <limits>
#include <optional>
#include <stdexcept>

using qtd::core::distance;
using qtd::core::distanceSquared;
using qtd::core::Fixed;
using qtd::core::FixedRangeError;
using qtd::core::Point;

namespace {

constexpr qint64 kMaxRaw = std::numeric_limits<qint64>::max();

Fixed value(const QString &text)
{
    const std::optional<Fixed> parsed = Fixed::parse(text);
    if (!parsed) {
        throw std::logic_error("the test expected a valid number");
    }
    return *parsed;
}

Fixed value(const char *text)
{
    return value(QString::fromLatin1(text));
}

/// In Debug builds the operation must throw, elsewhere it gives the replacement.
template <typename Operation>
void expectRangeError(Operation operation, qint64 replacementRaw)
{
#ifdef QT_NO_DEBUG
    QCOMPARE(operation().raw(), replacementRaw);
#else
    Q_UNUSED(replacementRaw)
    QVERIFY_THROWS_EXCEPTION(FixedRangeError, operation());
#endif
}

} // namespace

class TestFixed : public QObject
{
    Q_OBJECT

private slots:
    // These raw values pin the scale: 4 decimal digits. Changing it invalidates all saved games.
    static void rawValuesAreTenThousandths()
    {
        QCOMPARE(Fixed::fromInt(1).raw(), 10'000);
        QCOMPARE(Fixed::fromInt(-3).raw(), -30'000);
        QCOMPARE(value("1.5").raw(), 15'000);
        QCOMPARE(Fixed().raw(), 0);
    }

    static void parsesDecimalText_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<qint64>("raw");
        QTest::newRow("integer") << QStringLiteral("12") << qint64(120'000);
        QTest::newRow("zero") << QStringLiteral("0") << qint64(0);
        QTest::newRow("minus zero") << QStringLiteral("-0") << qint64(0);
        QTest::newRow("one decimal") << QStringLiteral("1.5") << qint64(15'000);
        QTest::newRow("all decimals") << QStringLiteral("12.3456") << qint64(123'456);
        QTest::newRow("smallest") << QStringLiteral("0.0001") << qint64(1);
        QTest::newRow("negative") << QStringLiteral("-0.25") << qint64(-2'500);
        QTest::newRow("leading zero") << QStringLiteral("007") << qint64(70'000);
        QTest::newRow("largest") << QStringLiteral("922337203685477.5807") << kMaxRaw;
        QTest::newRow("lowest") << QStringLiteral("-922337203685477.5807") << -kMaxRaw;
    }

    static void parsesDecimalText()
    {
        QFETCH(const QString, text);
        QFETCH(const qint64, raw);
        QCOMPARE(value(text).raw(), raw);
    }

    static void rejectsWhatIsNotExact_data()
    {
        QTest::addColumn<QString>("text");
        QTest::newRow("empty") << QString();
        QTest::newRow("only minus") << QStringLiteral("-");
        QTest::newRow("five decimals") << QStringLiteral("0.00001");
        QTest::newRow("five decimals with zero") << QStringLiteral("1.00000");
        QTest::newRow("no fraction digits") << QStringLiteral("1.");
        QTest::newRow("no whole digits") << QStringLiteral(".5");
        QTest::newRow("plus") << QStringLiteral("+1");
        QTest::newRow("exponent") << QStringLiteral("1e3");
        QTest::newRow("letters") << QStringLiteral("abc");
        QTest::newRow("two dots") << QStringLiteral("1.2.3");
        QTest::newRow("second minus") << QStringLiteral("--1");
        QTest::newRow("space") << QStringLiteral(" 1");
        QTest::newRow("one over the largest") << QStringLiteral("922337203685477.5808");
        QTest::newRow("far too large") << QStringLiteral("99999999999999999999");
    }

    static void rejectsWhatIsNotExact()
    {
        QFETCH(const QString, text);
        QVERIFY(!Fixed::parse(text).has_value());
    }

    static void writesShortestText_data()
    {
        QTest::addColumn<qint64>("raw");
        QTest::addColumn<QString>("text");
        QTest::newRow("integer") << qint64(20'000) << QStringLiteral("2");
        QTest::newRow("fraction") << qint64(12'500) << QStringLiteral("1.25");
        QTest::newRow("inner zero") << qint64(10'005) << QStringLiteral("1.0005");
        QTest::newRow("negative fraction") << qint64(-5'000) << QStringLiteral("-0.5");
        QTest::newRow("zero") << qint64(0) << QStringLiteral("0");
    }

    static void writesShortestText()
    {
        QFETCH(const qint64, raw);
        QFETCH(const QString, text);
        QCOMPARE(toString(Fixed::fromRaw(raw)), text);
    }

    static void addsAndSubtracts()
    {
        QCOMPARE((value("1.25") + value("2.5")).raw(), 37'500);
        QCOMPARE((value("1.25") - value("2.5")).raw(), -12'500);
        QCOMPARE((-value("1.25")).raw(), -12'500);
    }

    static void multipliesAndRounds_data()
    {
        QTest::addColumn<QString>("left");
        QTest::addColumn<QString>("right");
        QTest::addColumn<qint64>("raw");
        QTest::newRow("exact") << QStringLiteral("1.5") << QStringLiteral("2.25") << qint64(33'750);
        QTest::newRow("signs") << QStringLiteral("-1.5") << QStringLiteral("2") << qint64(-30'000);
        QTest::newRow("both negative")
            << QStringLiteral("-1.5") << QStringLiteral("-2") << qint64(30'000);
        // 0.0003 * 0.3333 = 0.00009999 rounds up, 0.0001 * 0.4 = 0.00004 rounds down.
        QTest::newRow("up") << QStringLiteral("0.0003") << QStringLiteral("0.3333") << qint64(1);
        QTest::newRow("down") << QStringLiteral("0.0001") << QStringLiteral("0.4") << qint64(0);
        // 0.0001 * 0.5 = 0.00005 is a tie: away from zero on both sides.
        QTest::newRow("tie") << QStringLiteral("0.0001") << QStringLiteral("0.5") << qint64(1);
        QTest::newRow("negative tie")
            << QStringLiteral("-0.0001") << QStringLiteral("0.5") << qint64(-1);
    }

    static void multipliesAndRounds()
    {
        QFETCH(const QString, left);
        QFETCH(const QString, right);
        QFETCH(const qint64, raw);
        QCOMPARE((value(left) * value(right)).raw(), raw);
    }

    static void dividesAndRounds_data()
    {
        QTest::addColumn<QString>("left");
        QTest::addColumn<QString>("right");
        QTest::addColumn<qint64>("raw");
        QTest::newRow("exact") << QStringLiteral("7.5") << QStringLiteral("2.5") << qint64(30'000);
        QTest::newRow("down") << QStringLiteral("1") << QStringLiteral("3") << qint64(3'333);
        QTest::newRow("up") << QStringLiteral("2") << QStringLiteral("3") << qint64(6'667);
        QTest::newRow("negative up")
            << QStringLiteral("-2") << QStringLiteral("3") << qint64(-6'667);
        QTest::newRow("negative divisor")
            << QStringLiteral("2") << QStringLiteral("-3") << qint64(-6'667);
        QTest::newRow("both negative")
            << QStringLiteral("-1") << QStringLiteral("-3") << qint64(3'333);
        QTest::newRow("tie") << QStringLiteral("0.0001") << QStringLiteral("2") << qint64(1);
        QTest::newRow("negative tie")
            << QStringLiteral("-0.0001") << QStringLiteral("2") << qint64(-1);
    }

    static void dividesAndRounds()
    {
        QFETCH(const QString, left);
        QFETCH(const QString, right);
        QFETCH(const qint64, raw);
        QCOMPARE((value(left) / value(right)).raw(), raw);
    }

    static void takesRoots_data()
    {
        QTest::addColumn<QString>("input");
        QTest::addColumn<qint64>("raw");
        QTest::newRow("zero") << QStringLiteral("0") << qint64(0);
        QTest::newRow("one") << QStringLiteral("1") << qint64(10'000);
        QTest::newRow("square") << QStringLiteral("16") << qint64(40'000);
        QTest::newRow("smallest") << QStringLiteral("0.0001") << qint64(100);
        // sqrt(2) = 1.41421..., sqrt(0.0002) = 0.0141421..., sqrt(3) = 1.73205...
        QTest::newRow("two") << QStringLiteral("2") << qint64(14'142);
        QTest::newRow("small") << QStringLiteral("0.0002") << qint64(141);
        QTest::newRow("rounds up") << QStringLiteral("3") << qint64(17'321);
        QTest::newRow("large") << QStringLiteral("90000000000") << qint64(3'000'000'000);
    }

    static void takesRoots()
    {
        QFETCH(const QString, input);
        QFETCH(const qint64, raw);
        QCOMPARE(sqrt(value(input)).raw(), raw);
    }

    static void measuresDistances()
    {
        const Point from{Fixed::fromInt(1), Fixed::fromInt(1)};
        const Point to{Fixed::fromInt(4), Fixed::fromInt(5)};
        QCOMPARE(distanceSquared(from, to).raw(), 250'000);
        QCOMPARE(distance(from, to).raw(), 50'000);
        QCOMPARE(distance(to, from).raw(), 50'000);
        QCOMPARE(distance(from, from).raw(), 0);
        QCOMPARE(distance(Point{}, from).raw(), 14'142);
    }

    static void valuesAtTheLimitsStayExact()
    {
        QCOMPARE((Fixed::max() + Fixed()).raw(), kMaxRaw);
        QCOMPARE((Fixed::max() + Fixed::min()).raw(), 0);
        QCOMPARE((Fixed::min() - Fixed()).raw(), -kMaxRaw);
        QCOMPARE((-Fixed::min()).raw(), kMaxRaw);
        QCOMPARE(Fixed::fromRaw(-kMaxRaw).raw(), -kMaxRaw);
        QVERIFY(Fixed::min() < Fixed::max());
        // 303700^2 * 10^8 = 9.223369e18 still fits a qint64.
        QCOMPARE((Fixed::fromInt(303'700) * Fixed::fromInt(303'700)).raw(), 922'336'900'000'000);
    }

    static void rangeErrorsAreDetected()
    {
        const Fixed one = Fixed::fromRaw(1);
        expectRangeError([&] { return Fixed::max() + one; }, kMaxRaw);
        expectRangeError([&] { return Fixed::min() - one; }, -kMaxRaw);
        expectRangeError([&] { return Fixed::min() + (-one); }, -kMaxRaw);
        expectRangeError([] { return Fixed::fromRaw(std::numeric_limits<qint64>::min()); },
                         -kMaxRaw);
        expectRangeError([] { return Fixed::fromInt(kMaxRaw / Fixed::kScale + 1); }, kMaxRaw);
        expectRangeError([] { return Fixed::fromInt(-(kMaxRaw / Fixed::kScale) - 1); }, -kMaxRaw);
    }

    static void multiplicationOverflowIsDetected()
    {
        // 303701^2 * 10^8 = 9.2234297e18 is above 2^63 = 9.2233720e18.
        const Fixed large = Fixed::fromInt(303'701);
        expectRangeError([&] { return large * large; }, kMaxRaw);
        expectRangeError([&] { return large * -large; }, -kMaxRaw);
        expectRangeError([&] { return -large * -large; }, kMaxRaw);
    }

    static void divisionErrorsAreDetected()
    {
        const Fixed one = Fixed::fromInt(1);
        expectRangeError([&] { return one / Fixed(); }, kMaxRaw);
        expectRangeError([&] { return -one / Fixed(); }, -kMaxRaw);
        expectRangeError([&] { return Fixed::max() / one; }, kMaxRaw);
        expectRangeError([&] { return Fixed::max() / -one; }, -kMaxRaw);
    }

    static void rootErrorsAreDetected()
    {
        expectRangeError([] { return sqrt(Fixed::fromRaw(-1)); }, 0);
        expectRangeError([] { return sqrt(Fixed::max()); }, kMaxRaw);
    }
};

QTEST_APPLESS_MAIN(TestFixed)
#include "tst_fixed.moc"
