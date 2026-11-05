#pragma once

#include <QByteArray>
#include <QObject>

#include <memory>

class QQuickWindow;

namespace qtd::automation {
class ControlSession;
}

namespace qtd::client {

class GameSession;

/// The control interface in the real game: requests play the game on the screen. Time stands still
/// until a request lets it run, so that a controlled game stays a matter of ticks.
class ScreenControl : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ScreenControl)

public:
    ScreenControl(GameSession *game, QQuickWindow *window);
    ~ScreenControl() override;

    /// Handles one line of the protocol.
    QByteArray handle(const QByteArray &request);

    /// Reads requests from standard input and writes the answers to standard output. The end of the
    /// input ends the program.
    void listenOnStandardInput();

private:
    class ScreenGame;
    struct Mailbox;

    std::unique_ptr<ScreenGame> game_;
    std::unique_ptr<automation::ControlSession> session_;
    std::shared_ptr<Mailbox> mailbox_;
};

} // namespace qtd::client
