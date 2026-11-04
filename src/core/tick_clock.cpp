#include "core/tick_clock.h"

#include "core/simulation.h"

#include <algorithm>

namespace qtd::core {

namespace {

constexpr qint64 kMicrosecondsPerSecond = 1'000'000;

} // namespace

void TickClock::setSpeed(qint64 factor)
{
    speed_ = std::max<qint64>(factor, 1);
}

qint64 TickClock::ticksFor(qint64 elapsedMicroseconds)
{
    if (paused_ || elapsedMicroseconds <= 0) {
        return 0;
    }
    pending_ += elapsedMicroseconds * speed_ * kTicksPerSecond;
    const qint64 ticks = pending_ / kMicrosecondsPerSecond;
    pending_ %= kMicrosecondsPerSecond;
    return ticks;
}

} // namespace qtd::core
