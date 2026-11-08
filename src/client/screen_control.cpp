#include "client/screen_control.h"

#include "automation/control_session.h"
#include "automation/control_stdio.h"
#include "automation/report.h"
#include "client/game_session.h"
#include "core/content_loader.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QImage>
#include <QJsonObject>
#include <QLatin1StringView>
#include <QMetaObject>
#include <QPointer>
#include <QQuickWindow>
#include <rhi/qrhi.h>

#include <atomic>
#include <cstdio>
#include <exception>
#include <memory>
#include <mutex>
#include <set>
#include <thread>

using namespace Qt::StringLiterals;

namespace qtd::client {

namespace {

constexpr double kNanosecondsPerSecond = 1e9;
constexpr qint64 kNanosecondsPerMicrosecond = 1000;

/// What frames cost, apart from how often the window draws them. The render thread writes, the
/// control reads. The GPU time needs timestamps, see QSG_RHI_PROFILE in
/// docs/Performance-Verlauf.md.
class FrameCosts
{
public:
    /// From the start of a frame to its end, in the render thread.
    static void watch(const std::shared_ptr<FrameCosts> &costs, QQuickWindow *window)
    {
        constexpr Qt::ConnectionType direct = Qt::DirectConnection;
        QObject::connect(
            window, &QQuickWindow::beforeFrameBegin, window, [costs] { costs->begin(); }, direct);
        QObject::connect(
            window,
            &QQuickWindow::afterRendering,
            window,
            [costs, window] { costs->noteGpuTime(window->swapChain()); },
            direct);
        QObject::connect(
            window, &QQuickWindow::afterFrameEnd, window, [costs] { costs->end(); }, direct);
    }

    [[nodiscard]] QJsonObject report() const
    {
        return {{"frames"_L1, frames_.load()},
                {"cpuMicroseconds"_L1, cpuNanoseconds_.load() / kNanosecondsPerMicrosecond},
                {"gpuMicroseconds"_L1, gpuNanoseconds_.load() / kNanosecondsPerMicrosecond}};
    }

private:
    void begin()
    {
        running_.start();
    }

    /// The time of a frame that the GPU has finished, one or two frames ago.
    void noteGpuTime(QRhiSwapChain *swapChain)
    {
        if (swapChain != nullptr) {
            const double seconds = swapChain->currentFrameCommandBuffer()->lastCompletedGpuTime();
            gpuNanoseconds_ += static_cast<qint64>(seconds * kNanosecondsPerSecond);
        }
    }

    void end()
    {
        cpuNanoseconds_ += running_.nsecsElapsed();
        ++frames_;
    }

    QElapsedTimer running_;
    std::atomic<qint64> frames_ = 0;
    std::atomic<qint64> cpuNanoseconds_ = 0;
    std::atomic<qint64> gpuNanoseconds_ = 0;
};

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
    ScreenGame(GameSession *game, QQuickWindow *window) : game_(game), window_(window)
    {
        FrameCosts::watch(frameCosts_, window);
    }

    QString start(const QString &map, quint64 seed, core::GameMode mode) override
    {
        const core::ContentLoadResult loaded = core::loadContentFile(map);
        if (!loaded.content) {
            return loaded.error;
        }
        game_->startGame(map, seed, mode);
        game_->setPaused(true);
        started_ = true;
        return {};
    }

    QString loadSaved(const QString &map, const QString &file) override
    {
        const QString error = game_->resumeGame(map, file);
        started_ = started_ || error.isEmpty();
        return error;
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
        answer->insert("frameCosts"_L1, frameCosts_->report());
    }

    QString saveScreenshot(const QString &file, QJsonObject *answer) override
    {
        // Before its window is on the screen, Qt draws the picture with a renderer of its own,
        // and the next map crashed in the textures of the real one (E-43).
        if (!window_->isExposed()) {
            return u"the window is not on the screen yet"_s;
        }
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
    std::shared_ptr<FrameCosts> frameCosts_ = std::make_shared<FrameCosts>();
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
