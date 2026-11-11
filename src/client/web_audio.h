#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include <set>

namespace qtd::client {

/// The sounds of the game in the browser, through Web Audio: Qt 6.12 for WebAssembly ships no
/// Qt Multimedia (E-47). Elsewhere it plays nothing.
class WebAudio : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit WebAudio(QObject *parent = nullptr);

    /// Plays :/assets/<name>.ogg from the start, at a loudness from 0 to 1. The first time a sound
    /// is decoded first and plays when it is ready.
    Q_INVOKABLE void play(const QString &name, double loudness);

private:
    std::set<QString> handedOver_; // the sounds whose file the browser already has
};

} // namespace qtd::client
