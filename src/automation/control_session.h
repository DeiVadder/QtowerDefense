#pragma once

#include "core/json_reader.h"
#include "core/replay.h"

#include <QByteArray>
#include <QByteArrayView>
#include <QJsonObject>
#include <QString>

#include <functional>
#include <memory>

namespace qtd::automation {

/// Where a controlled game lives: on its own in qtd_headless, on the screen in the real game. The
/// optional parts answer with an error text, and an empty text means done.
class ControlledGame
{
public:
    ControlledGame() = default;
    virtual ~ControlledGame() = default;

    ControlledGame(const ControlledGame &) = delete;
    ControlledGame &operator=(const ControlledGame &) = delete;

    /// Starts a game. A failure gives its text and keeps the current game.
    virtual QString start(const QString &map, quint64 seed) = 0;
    /// The current game, nothing before the first start.
    virtual core::ReplayRecorder *recorder() = 0;
    /// A request has changed the game.
    virtual void changed();
    /// Adds what only this place knows to the observation.
    virtual void observe(QJsonObject *answer) const;

    /// Saves the picture of the game. Facts about the picture go to answer.
    virtual QString saveScreenshot(const QString &file, QJsonObject *answer);
    /// Lets the game run in real time or stops it.
    virtual QString setPaused(bool paused);
    /// Ends the program.
    virtual QString quit();
};

/// The control interface without its transport: one JSON request in, one JSON answer out. See
/// docs/Steuerschnittstelle.md. A session plays one game at a time and records it, and it goes
/// through the same commands as a human.
class ControlSession
{
public:
    /// A session with a game of its own, without a window.
    ControlSession();
    /// A session on a game that lives elsewhere and outlives the session.
    explicit ControlSession(ControlledGame *game);
    ~ControlSession();

    ControlSession(const ControlSession &) = delete;
    ControlSession &operator=(const ControlSession &) = delete;

    /// Handles one line of the protocol. Never fails: problems come back as an answer.
    QByteArray handle(QByteArrayView request);

private:
    QByteArray startMap(core::ObjectReader &request);
    QByteArray buildTower(core::ObjectReader &request);
    QByteArray advance(core::ObjectReader &request);
    QByteArray saveReplay(core::ObjectReader &request);
    QByteArray saveScreenshot(core::ObjectReader &request);
    /// Runs a request without fields, once it is known to have none.
    QByteArray finish(const core::ObjectReader &request, const std::function<QString()> &act);
    QJsonObject observation() const;

    std::unique_ptr<ControlledGame> ownGame_;
    ControlledGame *game_;
};

} // namespace qtd::automation
