#include "client/screen_control.h"

#include "automation/control_session.h"
#include "automation/control_stdio.h"
#include "automation/report.h"
#include "client/game_session.h"
#include "core/content_loader.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QImage>
#include <QJsonObject>
#include <QLatin1StringView>
#include <QMetaObject>
#include <QPointer>
#include <QQuickWindow>

#include <cstdio>
#include <exception>
#include <mutex>
#include <set>
#include <thread>

using namespace Qt::StringLiterals;

namespace qtd::client {

namespace {

/// Enough points to tell a picture from an empty window.
constexpr int kColorGridSteps = 32;

qint64 distinctColors(const QImage &image)
{
    std::set<QRgb> colors;
    for (int y = 0; y < kColorGridSteps; ++y) {
        for (int x = 0; x < kColorGridSteps; ++x) {
            colors.insert(image.pixel(x * image.width() / kColorGridSteps,
                                      y * image.height() / kColorGridSteps));
        }
    }
    return static_cast<qint64>(colors.size());
}

} // namespace

/// The game on the screen, as the control interface sees it.
class ScreenControl::ScreenGame : public automation::ControlledGame
{
public:
    ScreenGame(GameSession *game, QQuickWindow *window) : game_(game), window_(window) {}

    QString start(const QString &map, quint64 seed) override
    {
        const core::ContentLoadResult loaded = core::loadContentFile(map);
        if (!loaded.content) {
            return loaded.error;
        }
        game_->startGame(map, seed);
        game_->setPaused(true);
        started_ = true;
        return {};
    }

    core::ReplayRecorder *recorder() override
    {
        return started_ ? game_->recorder() : nullptr;
    }

    void changed() override
    {
        game_->refresh();
    }

    void observe(QJsonObject *answer) const override
    {
        answer->insert("paused"_L1, game_->paused());
        answer->insert("frames"_L1, frames_);
    }

    QString saveScreenshot(const QString &file, QJsonObject *answer) override
    {
        const QImage image = window_->grabWindow();
        if (image.isNull()) {
            return u"the window gave no picture"_s;
        }
        QByteArray data;
        QBuffer buffer(&data);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "PNG");
        QString error;
        if (!automation::writeFile(file, data, &error)) {
            return error;
        }
        answer->insert("width"_L1, image.width());
        answer->insert("height"_L1, image.height());
        answer->insert("colors"_L1, distinctColors(image));
        return {};
    }

    QString setPaused(bool paused) override
    {
        game_->setPaused(paused);
        return {};
    }

    QString quit() override
    {
        QMetaObject::invokeMethod(
            QCoreApplication::instance(), &QCoreApplication::quit, Qt::QueuedConnection);
        return {};
    }

    void countFrame()
    {
        ++frames_;
    }

private:
    QPointer<GameSession> game_;
    QPointer<QQuickWindow> window_;
    qint64 frames_ = 0;
    bool started_ = false; // the game that the window opened with has a seed of its own
};

/// The way from the thread that reads to the control. The thread may outlive the control: it blocks
/// in the input, and nothing can wake it.
struct ScreenControl::Mailbox
{
    std::mutex mutex;
    ScreenControl *control = nullptr;

    void deliver(const QByteArray &line)
    {
        const std::scoped_lock lock(mutex);
        if (control != nullptr) {
            ScreenControl *target = control;
            QMetaObject::invokeMethod(
                target,
                [target, line] { automation::writeControlAnswer(target->handle(line)); },
                Qt::QueuedConnection);
        }
    }

    void endOfInput()
    {
        const std::scoped_lock lock(mutex);
        if (control != nullptr) {
            QMetaObject::invokeMethod(
                QCoreApplication::instance(), &QCoreApplication::quit, Qt::QueuedConnection);
        }
    }

    void readAll()
    {
        try {
            automation::readControlLines([this](const QByteArray &line) { deliver(line); });
        } catch (const std::exception &error) {
            std::fputs(error.what(), stderr);
        }
        endOfInput();
    }
};

ScreenControl::ScreenControl(GameSession *game, QQuickWindow *window)
    : game_(std::make_unique<ScreenGame>(game, window)),
      session_(std::make_unique<automation::ControlSession>(game_.get())),
      mailbox_(std::make_shared<Mailbox>())
{
    mailbox_->control = this;
    game->setPaused(true);
    connect(window, &QQuickWindow::frameSwapped, this, [this] { game_->countFrame(); });
}

ScreenControl::~ScreenControl()
{
    const std::scoped_lock lock(mailbox_->mutex);
    mailbox_->control = nullptr;
}

QByteArray ScreenControl::handle(const QByteArray &request)
{
    return session_->handle(request);
}

void ScreenControl::listenOnStandardInput()
{
    std::thread([mailbox = mailbox_] { mailbox->readAll(); }).detach();
}

} // namespace qtd::client
