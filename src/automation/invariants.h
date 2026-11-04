#pragma once

#include "core/simulation.h"

#include <QString>

#include <functional>

namespace qtd::automation {

/// Judges a state: a text for the first problem, or nothing.
using StateCheck = std::function<QString(const core::Content &, const core::SimulationState &)>;

/// Looks for a state that no game may reach: negative gold or lives, an enemy off the path, ids out
/// of order, towers on unknown or shared slots. Gives a text for the first problem, or nothing.
QString checkInvariants(const core::Content &content, const core::SimulationState &state);

} // namespace qtd::automation
