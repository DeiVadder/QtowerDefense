import QtQuick
import QtowerDefense.Client

// Plays what the game asks for in the browser, through Web Audio: Qt 6.12 for WebAssembly ships
// no Qt Multimedia (E-47). The same properties and sounds as ../GameSounds.qml.
Item {
    id: root

    required property GameSession game
    property real volume: 1 // from 0 to 1, as the settings say, as the ear hears it

    Connections {
        target: root.game

        function onCue(what: int) {
            WebAudio.play(Theme.cueSounds[what], Theme.loudness(root.volume));
        }
    }
}
