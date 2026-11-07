pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Shapes
import QtowerDefense.Client

// The hero on its iron shield, drawn in a box of 184 by 184 that is scaled to the size of this item.
// It blinks, breathes, talks and watches the board, unless told where to look.
Item {
    id: root

    property int mood: HeroVoice.Mood.Calm
    property bool talking: false
    property string spell: "" // the icon in its hand, if any
    property color spellColor: Theme.fire
    property bool focused: false // looks at lookAt, else it lets its eyes wander
    property point lookAt: Qt.point(0.5, 0.5) // on the board, from 0 to 1

    property real gaze
    property real blink
    property real breath
    property real flap

    readonly property point look: focused ? lookAt : Qt.point(gaze, 1 - gaze)
    // The board lies to the right and below: from there the eyes go from straight on to the edge.
    readonly property real gazeX: Math.min(1, look.x * 1.4 - 0.2)
    readonly property real gazeY: Math.min(1, look.y * 1.4 - 0.2)

    readonly property real lid: mood === HeroVoice.Mood.Worried ? 0 : mood === HeroVoice.Mood.Casting
                                                                  ? 0.4 : 0.2
    readonly property real mouth: mood === HeroVoice.Mood.Worried ? 1 : mood
                                                                    === HeroVoice.Mood.Glad ? 0.5 :
                                                                                              0.1
    readonly property int browTilt: mood === HeroVoice.Mood.Casting ? 16 : mood === HeroVoice.Mood.Worried ?
                                                                          -10 : 4

    component Part: ShapePath {
        id: part

        required property string path

        strokeWidth: 2

        PathSvg {
            path: part.path
        }
    }

    component Eye: Rectangle {
        width: 24
        height: 19
        radius: 9
        color: Theme.heroEye
        border.color: Theme.heroLine
        clip: true

        Rectangle {
            x: 7 + root.gazeX * 6
            y: 5 + root.gazeY * 3
            width: 10
            height: 10
            radius: 5
            color: root.mood === HeroVoice.Mood.Casting ? root.spellColor : Theme.heroPupil
        }

        Rectangle {
            width: parent.width
            height: parent.height * Math.max(root.lid, root.blink)
            color: Theme.heroSkinDark
        }
    }

    component Brow: Rectangle {
        y: root.mood === HeroVoice.Mood.Worried ? 63 : 69
        width: 32
        height: 9
        radius: 3
        color: Theme.heroBrow

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
            from: 0.3
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

    SequentialAnimation on flap {
        running: root.talking
        loops: Animation.Infinite
        onStopped: root.flap = 0

        NumberAnimation {
            to: 0.8
            duration: 110
        }
        NumberAnimation {
            to: 0.1
            duration: 130
        }
    }

    Item {
        width: 184
        height: 184
        scale: root.width / 184
        transformOrigin: Item.TopLeft

        Rectangle {
            anchors.fill: parent
            radius: width / 2
            border.color: Theme.ironDark
            border.width: 3
            gradient: Gradient {
                GradientStop {
                    position: 0
                    color: Theme.ironLight
                }
                GradientStop {
                    position: 1
                    color: Theme.ironDark
                }
            }
        }

        Repeater {
            model: 12

            delegate: Rectangle {
                required property int index
                readonly property real angle: index * Math.PI / 6

                x: 92 + Math.cos(angle) * 83 - 4
                y: 92 + Math.sin(angle) * 83 - 4
                width: 8
                height: 8
                radius: 4
                color: Theme.ironLight
                border.color: Theme.ironDark
            }
        }

        Rectangle {
            anchors.centerIn: parent
            width: 152
            height: 152
            radius: 76
            color: Theme.heroShield
            border.color: Theme.ironDark
            border.width: 3
        }

        Item {
            width: 184
            height: 184
            x: root.gazeX * 5
            y: root.gazeY * 3 + root.breath
            rotation: root.gazeX * 4
            transformOrigin: Item.Bottom

            Shape {
                anchors.fill: parent
                preferredRendererType: Shape.CurveRenderer

                Part {
                    path: "M36 162c6-22 28-30 56-30s50 8 56 30c-16 12-38 16-56 16s-40-4-56-16Z"
                    fillColor: Theme.heroArmor
                    strokeColor: Theme.ironDark
                }
                Part {
                    path: "M52 70c-4 30 0 56 14 70 8 7 18 9 26 9s18-2 26-9c14-14 18-40 14-70Z"
                    fillColor: Theme.heroSkin
                    strokeColor: Theme.heroLine
                }
                Part {
                    path: "M84 92c-3 8-6 16-5 22 4 5 21 5 26 0 1-6-2-14-5-22Z"
                    fillColor: Theme.heroSkinDark
                    strokeColor: Theme.heroLine
                }
            }

            Eye {
                x: 58
                y: 80
            }

            Eye {
                x: 102
                y: 80
            }

            Brow {
                x: 54
                rotation: root.browTilt
            }

            Brow {
                x: 98
                rotation: -root.browTilt
            }

            Rectangle {
                objectName: "heroMouth"
                anchors.horizontalCenter: parent.horizontalCenter
                y: 124
                width: 34
                height: 5 + 18 * Math.max(root.mouth, root.flap)
                radius: 8
                color: Theme.heroMouth
            }

            Shape {
                anchors.fill: parent
                preferredRendererType: Shape.CurveRenderer

                Part {
                    path: "M92 112c-10-4-30 2-44 22 14-4 22 2 28 0 8-2 12-8 16-12 4 4 8 10 16 12 6 2 14-4 28 0-14-20-34-26-44-22Z"
                    fillColor: Theme.heroMoustache
                    strokeColor: Theme.heroMoustacheLine
                }
                ShapePath {
                    strokeColor: Theme.ironDark
                    strokeWidth: 3
                    fillGradient: LinearGradient {
                        x1: 30
                        x2: 150

                        GradientStop {
                            position: 0
                            color: Theme.helmetLight
                        }
                        GradientStop {
                            position: 0.5
                            color: Theme.helmetMiddle
                        }
                        GradientStop {
                            position: 1
                            color: Theme.helmetDark
                        }
                    }

                    PathSvg {
                        path: "M46 58c0-22 20-36 46-36s46 14 46 36c8 2 12 4 14 8-14 6-36 8-60 8s-46-2-60-8c2-4 6-6 14-8Z"
                    }
                }
                ShapePath {
                    fillColor: "transparent"
                    strokeColor: Theme.ironDark
                    strokeWidth: 3

                    PathSvg {
                        path: "M112 30c6 4 8 10 6 16 M70 20c8-14 36-14 44 0"
                    }
                }
            }
        }

        // The spell in the hand of the hero, as long as it is ready or under way.
        GameIcon {
            objectName: "heroSpell"
            x: 120
            y: 112
            width: 62
            height: 62
            visible: root.mood === HeroVoice.Mood.Casting
            name: root.spell
            fill: root.spellColor
        }
    }
}
