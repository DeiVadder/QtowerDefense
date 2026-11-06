import QtQuick
import QtQuick.Shapes
import QtowerDefense.Client

// The portrait of the hero with its speech bubble. The bubble lies over the map and takes no taps.
Item {
    id: root

    required property GameSession game
    required property real unit

    property real size: unit * Theme.heroSize
    property int spell: 0
    property point target: Qt.point(0.5, 0.5)

    implicitWidth: size
    implicitHeight: size

    HeroVoice {
        id: voice

        objectName: "heroVoice"
        game: root.game
    }

    Connections {
        target: root.game

        function onSpellCast(x: int, y: int, radius: real, spell: int) {
            root.spell = spell;
            root.target = Qt.point(x / root.game.gridSize.width, y / root.game.gridSize.height);
        }

        function onSelectionChanged() {
            if (root.game.selectedSpell >= 0) {
                root.spell = root.game.selectedSpell;
            }
        }
    }

    FramedPanel {
        anchors.fill: parent
        unit: root.unit

        HeroFace {
            objectName: "heroFace"
            anchors.fill: parent
            anchors.margins: root.unit * Theme.inset
            mood: voice.mood
            spellColor: Theme.spellColor(root.spell)
            focused: voice.line.length > 0 && voice.moment === HeroVoice.Moment.Spell
            lookAt: root.target
        }
    }

    Rectangle {
        id: bubble

        objectName: "heroBubble"
        anchors.left: parent.right
        anchors.leftMargin: root.unit * Theme.gap
        anchors.verticalCenter: parent.verticalCenter
        visible: voice.line.length > 0
        width: Math.min(words.implicitWidth, root.unit * Theme.bubbleWidth) + 2 * padding
        height: words.height + 2 * padding
        radius: root.unit * Theme.corner
        color: Theme.text
        border.color: Theme.woodDark

        readonly property real padding: root.unit * Theme.inset
        readonly property real tail: root.unit * Theme.gap

        Shape {
            anchors.right: parent.left
            anchors.rightMargin: -bubble.border.width
            anchors.verticalCenter: parent.verticalCenter
            width: bubble.tail
            height: bubble.tail
            preferredRendererType: Shape.CurveRenderer

            ShapePath {
                fillColor: Theme.text
                strokeColor: Theme.woodDark
                startX: bubble.tail
                startY: 0

                PathLine {
                    x: 0
                    y: bubble.tail / 2
                }
                PathLine {
                    x: bubble.tail
                    y: bubble.tail
                }
            }
        }

        Text {
            id: words

            objectName: "heroLine"
            x: bubble.padding
            y: bubble.padding
            width: Math.min(implicitWidth, root.unit * Theme.bubbleWidth)
            wrapMode: Text.Wrap
            text: voice.line
            color: Theme.woodDark
            font.family: Theme.face
            font.pixelSize: root.unit * Theme.smallText
            Accessible.role: Accessible.StaticText
            Accessible.name: text
        }
    }
}
