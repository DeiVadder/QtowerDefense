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
    property bool speaks: true // the player may turn the words off

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
        talking: root.speaks && voice.line.length > 0
        spell: root.game.spellNames[root.spell] ?? ""
        spellColor: Theme.spellColor(root.spell)
        focused: voice.line.length > 0 && voice.moment === HeroVoice.Moment.Spell
        lookAt: root.target
    }

    // It lies over the map, a tap on it means nothing there.
    MouseArea {
        anchors.fill: parent
    }

    WordSign {
        objectName: "heroBubble"
        x: root.bubbleLeft
        y: root.bubbleTop
        unit: root.unit
        wordsName: "heroLine"
        line: root.speaks ? voice.line : ""
    }
}
