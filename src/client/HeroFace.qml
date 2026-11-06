pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Shapes
import QtowerDefense.Client

// The hero, drawn in a box of 100 by 100 that is scaled to the size of this item. It blinks, breathes
// and watches the board, unless told where to look.
Item {
    id: root

    property int mood: HeroVoice.Mood.Calm
    property color spellColor: Theme.fire
    property bool focused: false // looks at lookAt, else it lets its eyes wander
    property point lookAt: Qt.point(0.5, 0.5) // on the board, from 0 to 1

    property real gaze
    property real blink
    property real breath

    readonly property point look: focused ? lookAt : Qt.point(gaze, 1 - gaze)
    readonly property string mouth: ["M40 73q10 3 20 0" // calm
        , "M37 69q13 13 26 0" // glad
        , "M40 77q10-8 20 0" // worried
        , "M44 72a6 6 0 1 0 12 0a6 6 0 1 0-12 0" // casting
    ][mood]
    // Tilted brows make the face. The left one is always up to something.
    readonly property int leftBrow: mood === HeroVoice.Mood.Worried ? -14 : -6
    readonly property int rightBrow: mood === HeroVoice.Mood.Worried ? 14 : 16

    component Part: Shape {
        id: part

        required property string path
        property color fill: "transparent"
        property color line: Theme.woodDark

        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            fillColor: part.fill
            strokeColor: part.line
            strokeWidth: 1.5
            capStyle: ShapePath.RoundCap

            PathSvg {
                path: part.path
            }
        }
    }

    component Eye: Rectangle {
        id: eye

        required property real centerX

        x: centerX - 6
        y: 50 - height / 2
        width: 12
        height: Math.max(1, 12 * (1 - root.blink))
        radius: 6
        color: Theme.text
        clip: true

        Rectangle {
            x: 2 + 4 * root.look.x
            y: eye.height / 2 - 3 + 3 * (root.look.y - 0.5)
            width: 5
            height: 5
            radius: 2.5
            color: Theme.ironDark
        }
    }

    component Brow: Rectangle {
        required property real centerX

        x: centerX - 8
        y: 38
        width: 16
        height: 3
        radius: 1.5
        color: Theme.heroBeard
        Behavior on rotation {
            NumberAnimation {
                duration: 150
            }
        }
    }

    SequentialAnimation on gaze {
        loops: Animation.Infinite

        PauseAnimation {
            duration: 2500
        }
        NumberAnimation {
            to: 1
            duration: 700
            easing.type: Easing.InOutQuad
        }
        PauseAnimation {
            duration: 3000
        }
        NumberAnimation {
            to: 0.3
            duration: 700
            easing.type: Easing.InOutQuad
        }
    }

    SequentialAnimation on blink {
        loops: Animation.Infinite

        PauseAnimation {
            duration: 3600
        }
        NumberAnimation {
            to: 1
            duration: 70
        }
        NumberAnimation {
            to: 0
            duration: 90
        }
    }

    SequentialAnimation on breath {
        loops: Animation.Infinite

        NumberAnimation {
            to: 1.5
            duration: 1600
            easing.type: Easing.InOutSine
        }
        NumberAnimation {
            to: 0
            duration: 1600
            easing.type: Easing.InOutSine
        }
    }

    Item {
        width: 100
        height: 100
        scale: root.width / 100
        transformOrigin: Item.TopLeft

        Part {
            path: "M8 100C8 80 30 74 50 74s42 6 42 26Z"
            fill: Theme.iron
        }

        Item {
            y: root.breath
            width: 100
            height: 100

            Part {
                path: "M24 50a26 30 0 1 0 52 0a26 30 0 1 0-52 0Z"
                fill: Theme.heroSkin
            }

            Part {
                path: "M24 58c2 26 14 36 26 36s24-10 26-36c-6 8-14 8-26 8s-20 0-26-8Z"
                fill: Theme.heroBeard
            }

            Part {
                objectName: "heroMouth"
                path: root.mouth
                fill: root.mood === HeroVoice.Mood.Casting ? Theme.woodDark : "transparent"
            }

            Eye {
                centerX: 37
            }

            Eye {
                centerX: 63
            }

            Brow {
                centerX: 37
                rotation: root.leftBrow
            }

            Brow {
                centerX: 63
                rotation: root.rightBrow
            }

            Part {
                path: "M21 40C22 18 36 8 50 8s28 10 29 32c-16-7-42-7-58 0Z"
                fill: Theme.iron
            }

            Part {
                path: "M46 34h8v22l-4 5-4-5Z"
                fill: Theme.iron
            }
        }

        // The spell in the hand of the hero, as long as it is ready or under way.
        Rectangle {
            objectName: "heroSpell"
            x: 76
            y: 72
            width: 20
            height: 20
            radius: 10
            visible: root.mood === HeroVoice.Mood.Casting
            color: root.spellColor
            border.color: Theme.text
        }
    }
}
