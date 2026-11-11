#include "web_storage.h"

// Only the browser build compiles this file. The guard leaves the code analysis of the other builds
// with an empty file.
#ifdef __EMSCRIPTEN__

#include <QCoreApplication>
#include <QTimer>

#include <emscripten.h>

namespace qtd::app {

namespace {

constexpr int pollMillis = 20;
constexpr int keepMillis = 5000;

/// Waits without blocking: the browser runs the sync only when the program gives it back control.
void callWhenLoaded(std::function<void()> ready)
{
    auto *poll = new QTimer(QCoreApplication::instance());
    poll->setInterval(pollMillis);
    QObject::connect(poll, &QTimer::timeout, poll, [poll, ready = std::move(ready)] {
        if (EM_ASM_INT({ return globalThis.qtdStorageReady ? 1 : 0; }) == 0) {
            return;
        }
        poll->stop();
        poll->deleteLater();
        ready();
    });
    poll->start();
}

} // namespace

// Writes go to memory first. They reach IndexedDB every few seconds and when the page is left.
// One sync at a time: Emscripten warns about two in flight. After a failed load nothing is
// written back: the sync would make IndexedDB as empty as the memory and lose the saves.
void openWebStorage(const QString &folder, std::function<void()> ready)
{
    const QByteArray path = folder.toUtf8();
    EM_ASM(
        {
            const mountPoint = UTF8ToString($0);
            globalThis.qtdStorageReady = false;
            let syncing = false;
            let loaded = false;
            FS.mkdirTree(mountPoint);
            FS.mount(IDBFS, {}, mountPoint);
            FS.syncfs(
                true, function(error) {
                    if (error) {
                        console.warn("QtowerDefense: the saves of the last visit did not load, " +
                                         "this visit keeps nothing",
                                     error);
                    }
                    loaded = !error;
                    globalThis.qtdStorageReady = true;
                });
            // A wish to keep during a sync runs after it: that sync may miss the last writes.
            let again = false;
            const keep = function()
            {
                if (!loaded) {
                    return;
                }
                if (syncing) {
                    again = true;
                    return;
                }
                syncing = true;
                again = false;
                FS.syncfs(
                    false, function(error) {
                        syncing = false;
                        if (error) {
                            console.warn("QtowerDefense: the saves were not kept", error);
                        }
                        if (again) {
                            keep();
                        }
                    });
            };
            setInterval(keep, $1);
            document.addEventListener("visibilitychange", keep);
            window.addEventListener("pagehide", keep);
        },
        path.constData(),
        keepMillis);
    callWhenLoaded(std::move(ready));
}

} // namespace qtd::app

#endif
