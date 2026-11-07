pragma ComponentBehavior: Bound

import QtQuick
import QtMultimedia
import QtowerDefense.Client

// Plays what the game asks for. The players load with the first sound, a controlled game has none.
Item {
    id: root

    component Sound: MediaPlayer {
        required property string name

        function start() {
            stop();
            play();
        }

        source: "qrc:/assets/" + name + ".ogg"
        audioOutput: AudioOutput {}
    }

    component Speakers: QtObject {
        // In the order of GameSession.Cue.
        readonly property list<Sound> sounds: [
            Sound {
                name: "build"
            },
            Sound {
                name: "upgrade"
            },
            Sound {
                name: "sell"
            },
            Sound {
                name: "spell"
            },
            Sound {
                name: "wave"
            },
            Sound {
                name: "won"
            },
            Sound {
                name: "lost"
            },
            Sound {
                name: "upgrade"
            },
            Sound {
                name: "sell"
            }
        ]
    }

    required property GameSession game

    Loader {
        id: speakers

        active: false
        sourceComponent: Component {
            Speakers {}
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
