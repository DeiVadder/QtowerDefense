pragma ComponentBehavior: Bound

import QtQuick
import QtMultimedia
import QtowerDefense.Client

// Plays what the game asks for. The players load with the first sound, a controlled game has none.
Item {
    id: root

    component Sound: MediaPlayer {
        id: sound

        required property string name
        property real loudness: 1

        function start() {
            stop();
            play();
        }

        source: "qrc:/assets/" + name + ".ogg"
        audioOutput: AudioOutput {
            volume: sound.loudness
        }
    }

    component Speakers: QtObject {
        id: box

        property real loudness: 1
        // In the order of GameSession.Cue.
        readonly property list<Sound> sounds: [
            Sound {
                name: "build"
                loudness: box.loudness
            },
            Sound {
                name: "upgrade"
                loudness: box.loudness
            },
            Sound {
                name: "sell"
                loudness: box.loudness
            },
            Sound {
                name: "spell"
                loudness: box.loudness
            },
            Sound {
                name: "wave"
                loudness: box.loudness
            },
            Sound {
                name: "won"
                loudness: box.loudness
            },
            Sound {
                name: "lost"
                loudness: box.loudness
            },
            Sound {
                name: "upgrade"
                loudness: box.loudness
            },
            Sound {
                name: "sell"
                loudness: box.loudness
            }
        ]
    }

    required property GameSession game
    property real volume: 1 // from 0 to 1, as the settings say, as the ear hears it
    // What the speakers get: the ear hears loudness on a logarithmic scale, so half way on the
    // slider is a tenth of the power.
    readonly property real loudness: (Math.pow(Theme.loudnessRange, volume) - 1) / (
                                         Theme.loudnessRange - 1)

    Loader {
        id: speakers

        objectName: "speakers"
        active: false
        sourceComponent: Component {
            Speakers {
                loudness: root.loudness
            }
        }
    }

    Connections {
        target: root.game

        function onCue(what: int) {
            speakers.active = true;
            (speakers.item as Speakers).sounds[what].start();
        }
    }
}
