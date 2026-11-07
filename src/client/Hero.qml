pragma ComponentBehavior: Bound

import QtQuick
import QtowerDefense.Client

// The portrait of the hero, with its words on a sign that hangs at bubbleTop. The sign lies over the
// map and takes no taps.
Item {
    id: root

    required property GameSession game
    required property real unit

    property real size: unit * Theme.heroSize
    property real bubbleLeft: size + unit * Theme.gap
    property real bubbleTop: 0
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

    HeroFace {
        objectName: "heroFace"
        anchors.fill: parent
        mood: voice.mood
        talking: voice.line.length > 0
        spell: root.game.spellNames[root.spell] ?? ""
        spellColor: Theme.spellColor(root.spell)
        focused: voice.line.length > 0 && voice.moment === HeroVoice.Moment.Spell
        lookAt: root.target
    }

    // It lies over the map, a tap on it means nothing there.
    MouseArea {
        anchors.fill: parent
    }

    Item {
        id: bubble

        readonly property real chain: root.unit * Theme.signChain
        readonly property real padding: root.unit * Theme.gap

        objectName: "heroBubble"
        x: root.bubbleLeft
        y: root.bubbleTop
        visible: voice.line.length > 0
        width: root.unit * Theme.bubbleWidth
        height: words.height + 2 * padding + chain
        transformOrigin: Item.Top

        Repeater {
            model: 2

            delegate: Rectangle {
                required property int index

                x: (index === 0 ? Theme.chainInset : 1 - Theme.chainInset) * bubble.width - width
                   / 2
                width: root.unit * Theme.bracketBar
                height: bubble.chain + width
                color: Theme.ironDark
                border.color: Theme.ironLight
            }
        }

        FramedPanel {
            y: bubble.chain
            width: parent.width
            height: parent.height - bubble.chain
            unit: root.unit * Theme.signBracket
            tone: Theme.signWood
            seed: Theme.signSeed
            takesTaps: false
        }

        Text {
            id: words

            objectName: "heroLine"
            x: bubble.padding
            y: bubble.chain + bubble.padding
            width: bubble.width - 2 * bubble.padding
            wrapMode: Text.Wrap
            text: voice.line
            color: Theme.burnt
            font.family: Theme.face
            font.bold: true
            font.pixelSize: root.unit * Theme.signText
            Accessible.role: Accessible.StaticText
            Accessible.name: text
        }

        // A new line drops in and swings out.
        SequentialAnimation {
            id: swing

            NumberAnimation {
                target: bubble
                property: "rotation"
                from: Theme.swingStart
                to: Theme.swingOut
                duration: Theme.swingTime
                easing.type: Easing.OutQuad
            }
            NumberAnimation {
                target: bubble
                property: "rotation"
                to: Theme.swingBack
                duration: Theme.swingTime
                easing.type: Easing.InOutQuad
            }
            NumberAnimation {
                target: bubble
                property: "rotation"
                to: 0
                duration: Theme.swingRest
            }
        }

        Connections {
            target: voice

            function onLineChanged() {
                if (voice.line.length > 0) {
                    swing.restart();
                }
            }
        }
    }
}
