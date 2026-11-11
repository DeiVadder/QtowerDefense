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
                name: Theme.cueSounds[GameSession.Build]
                loudness: box.loudness
            },
            Sound {
                name: Theme.cueSounds[GameSession.Upgrade]
                loudness: box.loudness
            },
            Sound {
                name: Theme.cueSounds[GameSession.Sell]
                loudness: box.loudness
            },
            Sound {
                name: Theme.cueSounds[GameSession.Spell]
                loudness: box.loudness
            },
            Sound {
                name: Theme.cueSounds[GameSession.Wave]
                loudness: box.loudness
            },
            Sound {
                name: Theme.cueSounds[GameSession.Won]
                loudness: box.loudness
            },
            Sound {
                name: Theme.cueSounds[GameSession.Lost]
                loudness: box.loudness
            },
            Sound {
                name: Theme.cueSounds[GameSession.Roll]
                loudness: box.loudness
            },
            Sound {
                name: Theme.cueSounds[GameSession.Coin]
                loudness: box.loudness
            }
        ]
    }

    required property GameSession game
    property real volume: 1 // from 0 to 1, as the settings say, as the ear hears it
    readonly property real loudness: Theme.loudness(volume) // what the speakers get

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
