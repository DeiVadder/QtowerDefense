#pragma once

#include "core/content.h"
#include "core/replay.h"

#include <QJsonObject>

namespace qtd::automation {

/// How often the gold of a game is written down, ten seconds of play.
constexpr qint64 kGoldCurveTicks = 300;

/// What the balancing measures of a played game (E-38): by tower type the gold it got, built and
/// upgraded, of the towers that stand at the end, and the gold after the commands of tick 0, then
/// every kGoldCurveTicks and at the end. A replay that does not play to its end adds its `error`.
/// The game is played again from its replay, the player is not watched while it plays.
QJsonObject gameStatistics(const core::Content &content, const core::Replay &replay);

} // namespace qtd::automation
