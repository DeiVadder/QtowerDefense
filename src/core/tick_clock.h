#pragma once

#include <QtTypes>

namespace qtd::core {

/// Turns the time between two frames into ticks. Speed and pause only change how fast ticks come,
/// never what they do. The caller measures the time, so this class knows no clock. All arithmetic
/// is exact, no fraction of a tick gets lost between frames.
class TickClock
{
public:
    /// The speed is a whole factor of at least 1.
    void setSpeed(qint64 factor);
    void setPaused(bool paused)
    {
        paused_ = paused;
    }

    /// The ticks that are due after the given time.
    qint64 ticksFor(qint64 elapsedMicroseconds);

private:
    qint64 speed_ = 1;
    qint64 pending_ = 0; // the part of a tick left over, in millionths
    bool paused_ = false;
};

} // namespace qtd::core
