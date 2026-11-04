#include "core/random.h"

namespace qtd::core {

namespace {

constexpr quint64 kMultiplier = 6'364'136'223'846'793'005ULL;

} // namespace

Random::Random(quint64 seed, quint64 stream) : increment_((stream << 1U) | 1U)
{
    next();
    state_ += seed;
    next();
}

quint32 Random::next()
{
    const quint64 old = state_;
    state_ = old * kMultiplier + increment_;
    const auto shifted = static_cast<quint32>(((old >> 18U) ^ old) >> 27U);
    const auto rotation = static_cast<quint32>(old >> 59U);
    return (shifted >> rotation) | (shifted << ((0U - rotation) & 31U));
}

quint32 Random::nextBelow(quint32 bound)
{
    if (bound == 0) {
        return 0;
    }
    const quint32 threshold = (0U - bound) % bound; // 2^32 mod bound: the values that would bias
    quint32 value = next();
    while (value < threshold) {
        value = next();
    }
    return value % bound;
}

} // namespace qtd::core
