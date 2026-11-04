#pragma once

#include "core/content.h"
#include "core/simulation.h"

#include <QByteArrayView>
#include <QtTypes>

namespace qtd::core {

/// FNV-1a with 64 bits. Numbers go in byte by byte, so the result is the same on every platform.
class Fnv1a64
{
public:
    void addBytes(QByteArrayView bytes);

    /// Always eight bytes, lowest first.
    void addNumber(quint64 number);

    quint64 value() const
    {
        return value_;
    }

private:
    quint64 value_ = 14'695'981'039'346'656'037ULL;
};

/// The hash of everything in the content that can change the course of a game. The ids are left
/// out, the simulation only sees indexes.
quint64 contentHash(const Content &content);

/// The hash of all that decides how a game goes on. It goes through the fields one by one, so
/// padding, addresses and capacities have no effect (E-18).
quint64 stateHash(const SimulationState &state);

} // namespace qtd::core
