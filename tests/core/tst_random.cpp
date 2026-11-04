#include "core/random.h"

#include <QTest>

#include <array>

using qtd::core::Random;

class TestRandom : public QObject
{
    Q_OBJECT

private slots:
    // The published output of pcg32-demo from the PCG reference code, seed 42, sequence 54.
    static void matchesThePublishedReferenceOutput()
    {
        Random random(42, 54);
        const std::array<quint32, 6> expected{
            0xa15c02b7, 0x7b47f409, 0xba1d3330, 0x83d2f293, 0xbfa4784b, 0xcbed606e};
        for (const quint32 number : expected) {
            QCOMPARE(random.next(), number);
        }
    }

    // Worked out with a separate Python copy of the reference code.
    static void seedingFollowsTheReference()
    {
        const Random random(1, 54);
        QCOMPARE(random.state(), 0xf336eb76e83ca5c3ULL);
        QCOMPARE(random.increment(), 0x6dULL);
    }

    // The reference output above modulo 6: 0xa15c02b7 = 2707161783 gives 3, and so on.
    static void boundedNumbersFollowTheReference()
    {
        Random random(42, 54);
        const std::array<quint32, 6> expected{3, 3, 2, 1, 1, 4};
        for (const quint32 number : expected) {
            QCOMPARE(random.nextBelow(6), number);
        }
    }

    // For bound 2^31 + 1 all numbers below 2^31 - 1 are rejected. The second number of the
    // reference output, 2068313097, is one of them.
    static void biasedNumbersAreSkipped()
    {
        Random random(42, 54);
        constexpr quint32 bound = 0x80000001;
        QCOMPARE(random.nextBelow(bound), 2707161783U - bound);
        QCOMPARE(random.nextBelow(bound), 3122475824U - bound);
    }

    static void boundOfOneOrZeroGivesZero()
    {
        Random random(42, 54);
        QCOMPARE(random.nextBelow(1), 0U);
        QCOMPARE(random.nextBelow(0), 0U);
        QCOMPARE(random.next(), 0x7b47f409U); // a bound of 1 draws once, a bound of 0 does not
    }

    static void streamsAndSeedsDiffer()
    {
        QVERIFY(Random(1, 54) != Random(2, 54));
        QVERIFY(Random(1, 54) != Random(1, 55));
        QVERIFY(Random(1, 54) == Random(1, 54));
    }
};

QTEST_APPLESS_MAIN(TestRandom)
#include "tst_random.moc"
