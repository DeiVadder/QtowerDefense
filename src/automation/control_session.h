#pragma once

#include "core/json_reader.h"
#include "core/replay.h"

#include <QByteArray>
#include <QByteArrayView>

#include <memory>

namespace qtd::automation {

/// The control interface without its transport: one JSON request in, one JSON answer out. See
/// docs/Steuerschnittstelle.md. A session plays one game at a time and records it, and it goes
/// through the same commands as a human.
class ControlSession
{
public:
    ControlSession();
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

    std::unique_ptr<core::ReplayRecorder> game_;
};

} // namespace qtd::automation
