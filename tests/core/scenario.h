#pragma once

#include "core/content_loader.h"

#include <QByteArray>

#include <optional>
#include <stdexcept>
#include <utility>

namespace qtd::tests {

/// A straight path of length 10. With 0.5 tiles per tick, an enemy spawned at tick s stands at
/// x = (t - s) / 2 at tick t, so all expected values of the tests can be worked out by hand.
inline QByteArray scenarioJson()
{
    return R"json({
    "formatVersion": 1,
    "startGold": 100,
    "startLives": 3,
    "gridWidth": 11,
    "gridHeight": 11,
    "path": [{ "x": 0, "y": 0 }, { "x": 10, "y": 0 }],
    "obstacles": [{ "terrain": "rock", "x": 0, "y": 2 }],
    "towers": [{
        "id": "arrow",
        "cost": 40,
        "range": "3",
        "cooldownTicks": 4,
        "damageMin": "2",
        "damageMax": "2",
        "damageType": "physical",
        "footprint": [{"x": 0, "y": 0}]
    }],
    "enemies": [{
        "id": "grunt",
        "health": "4",
        "speedPerTick": "0.5",
        "bounty": 7,
        "lifeCost": 1
    }],
    "wave": [{ "enemy": "grunt", "count": 2, "delayTicks": 0, "intervalTicks": 4 }]
}
)json";
}

/// The scenario with one piece of its text replaced. Gives an empty text if the piece is not there.
inline QByteArray editedJson(const QByteArray &from, const QByteArray &to)
{
    const QByteArray json = scenarioJson();
    return json.contains(from) ? QByteArray(json).replace(from, to) : QByteArray();
}

/// The value of something a test knows to be there. A mistake in the test ends the test.
template <typename Type>
Type required(std::optional<Type> optional)
{
    if (!optional) {
        throw std::logic_error("the test expected a value");
    }
    return *std::move(optional);
}

/// Content that the test knows to be valid.
inline core::Content contentOf(const QByteArray &json)
{
    return required(core::parseContent(json).content);
}

} // namespace qtd::tests
