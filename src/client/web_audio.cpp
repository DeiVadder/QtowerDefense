#include "client/web_audio.h"

// Every build has the type, so that the QML of the browser passes qmllint on the desktop. Only the
// browser plays: elsewhere Qt Multimedia does.
#ifndef __EMSCRIPTEN__

namespace qtd::client {

WebAudio::WebAudio(QObject *parent) : QObject(parent) {}

void WebAudio::play(const QString & /*name*/, double /*loudness*/) {}

} // namespace qtd::client

#else

#include <QByteArray>
#include <QFile>

#include <emscripten.h>

using namespace Qt::StringLiterals;

namespace qtd::client {

// Browsers keep the sound off until an input, so the context starts with the first sound and every
// input wakes it again. Asleep, it plays only a sound that an input brings: the others would all
// sound at once on waking. A sound that plays again starts from the beginning, as with Qt
// Multimedia. The loudness holds for the whole sound, which is short. The first time, a sound plays
// once it is decoded; one that does not decode stays silent for the visit.
// clang-format does not know JavaScript inside EM_ASM: it would split "===", which the macro passes
// on as written. Commas only inside parentheses: the macro splits its arguments at the others.
// clang-format off
WebAudio::WebAudio(QObject *parent) : QObject(parent)
{
    EM_ASM({
        const audio = {};
        audio.context = null;
        audio.sounds = new Map();
        audio.wake = function() {
            if (audio.context == null) {
                audio.context = new AudioContext();
            }
            if (audio.context.state != "running") {
                audio.context.resume();
            }
            return audio.context;
        };
        audio.start = function(sound, loudness) {
            const context = audio.wake();
            const input = navigator.userActivation == null || navigator.userActivation.isActive;
            if (context.state != "running" && !input) {
                return;
            }
            if (sound.playing != null) {
                sound.playing.stop();
            }
            const source = context.createBufferSource();
            const gain = context.createGain();
            gain.gain.value = loudness;
            source.buffer = sound.buffer;
            source.connect(gain).connect(context.destination);
            source.start();
            sound.playing = source;
        };
        const wakeOnInput = function() {
            if (audio.context != null) {
                audio.wake();
            }
        };
        document.addEventListener("pointerup", wakeOnInput, true);
        document.addEventListener("touchend", wakeOnInput, true);
        document.addEventListener("keydown", wakeOnInput, true);
        globalThis.qtdAudio = audio;
    });
}

void WebAudio::play(const QString &name, double loudness)
{
    const QByteArray key = name.toUtf8();
    if (handedOver_.insert(name).second) {
        QFile file(u":/assets/%1.ogg"_s.arg(name));
        const QByteArray data = file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
        EM_ASM({
            const audio = globalThis.qtdAudio;
            const sound = {};
            sound.buffer = null;
            sound.playing = null;
            sound.waiting = -1;
            audio.sounds.set(UTF8ToString($0), sound);
            const bytes = HEAPU8.slice($1, $1 + $2).buffer;
            audio.wake().decodeAudioData(bytes).then(function(buffer) {
                sound.buffer = buffer;
                if (sound.waiting >= 0) {
                    audio.start(sound, sound.waiting);
                    sound.waiting = -1;
                }
            }, function(error) {
                console.warn("QtowerDefense: a sound did not decode", error);
            });
        }, key.constData(), data.constData(), data.size());
    }
    EM_ASM({
        const audio = globalThis.qtdAudio;
        const sound = audio.sounds.get(UTF8ToString($0));
        if (sound.buffer == null) {
            sound.waiting = $1;
        } else {
            audio.start(sound, $1);
        }
    }, key.constData(), loudness);
}
// clang-format on

} // namespace qtd::client

#endif
