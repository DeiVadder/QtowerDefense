#pragma once

#include <QtTypes>

#include <compare>

namespace qtd::core {

/// PCG32 (XSH-RR, 64 bit state), the generator of pcg-random.org. See E-15. Its state belongs to
/// the simulation state and goes into the state hash.
class Random
{
public:
    Random(quint64 seed, quint64 stream);

    quint32 next();

    /// A uniform number in [0, bound), without modulo bias. A bound of 0 gives 0.
    quint32 nextBelow(quint32 bound);

    quint64 state() const
    {
        return state_;
    }
    quint64 increment() const
    {
        return increment_;
    }

    auto operator<=>(const Random &) const = default;

private:
    quint64 state_ = 0;
    quint64 increment_ = 0;
};

} // namespace qtd::core
