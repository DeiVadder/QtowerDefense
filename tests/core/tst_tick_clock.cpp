#include "core/tick_clock.h"

#include <QTest>

using qtd::core::TickClock;

class TickClockTest : public QObject
{
    Q_OBJECT

private slots:
    void oneSecondGivesThirtyTicks()
    {
        TickClock clock;
        QCOMPARE(clock.ticksFor(1'000'000), 30);
    }

    void fractionsOfATickAddUp()
    {
        TickClock clock;
        // A tick takes 33333 1/3 microseconds: the third frame completes the first whole tick
        // only because nothing of the first two was lost.
        QCOMPARE(clock.ticksFor(11'112), 0);
        QCOMPARE(clock.ticksFor(11'111), 0);
        QCOMPARE(clock.ticksFor(11'111), 1);
    }

    void aSecondInSmallFramesGivesThirtyTicksToo()
    {
        TickClock clock;
        qint64 ticks = 0;
        for (int frame = 0; frame < 1000; ++frame) {
            ticks += clock.ticksFor(1000);
        }
        QCOMPARE(ticks, 30);
    }

    void thePartOfATickIsKnown()
    {
        qtd::core::TickClock clock;
        QCOMPARE(clock.pendingMillionths(), 0);
        QCOMPARE(clock.ticksFor(10'000), 0);
        QCOMPARE(clock.pendingMillionths(), 300'000);
        QCOMPARE(clock.ticksFor(30'000), 1);
        QCOMPARE(clock.pendingMillionths(), 200'000);
    }

    void speedMultipliesTheTicks()
    {
        TickClock clock;
        clock.setSpeed(3);
        QCOMPARE(clock.ticksFor(1'000'000), 90);
    }

    void aSpeedBelowOneCountsAsOne()
    {
        TickClock clock;
        clock.setSpeed(0);
        QCOMPARE(clock.ticksFor(1'000'000), 30);
    }

    void pauseStopsTicksAndKeepsTheFraction()
    {
        TickClock clock;
        QCOMPARE(clock.ticksFor(20'000), 0); // 0.6 ticks
        clock.setPaused(true);
        QCOMPARE(clock.ticksFor(1'000'000), 0);
        clock.setPaused(false);
        QCOMPARE(clock.ticksFor(20'000), 1); // 1.2 ticks
    }

    void timeGoingBackwardsGivesNoTicks()
    {
        TickClock clock;
        QCOMPARE(clock.ticksFor(-1'000'000), 0);
        QCOMPARE(clock.ticksFor(1'000'000), 30);
    }
};

QTEST_APPLESS_MAIN(TickClockTest)
#include "tst_tick_clock.moc"
