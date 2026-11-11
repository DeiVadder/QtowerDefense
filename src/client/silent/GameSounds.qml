import QtQuick
import QtowerDefense.Client

// The sounds of a build without Qt Multimedia: Qt 6.12 for WebAssembly ships none, so the game in
// the browser is silent (E-47). The same properties as ../GameSounds.qml.
Item {
    required property GameSession game
    property real volume: 1
}
