pragma ComponentBehavior: Bound

import QtQuick
import QtowerDefense.Client

// The dice: a reel of cards runs behind an iron window, slows down and stops on the card. The game
// goes on behind it, the card is in the hand already and only waits for the show to end.
Item {
    id: root

    required property GameSession game
    required property real unit

    readonly property bool rolling: stepsLeft > 0 || leave.running
    property int result: 0
    property int shownTower: 0
    property int nextTower: 0
    property real offset: 0
    property int stepsLeft: 0
    property real spin: 0

    readonly property real window: unit * Theme.cardHeight + 2 * gap
    readonly property real gap: unit * Theme.gap

    function roll(tower: int) {
        result = tower;
        stepsLeft = Theme.diceSteps;
        spinning.restart();
        step();
    }

    function step() {
        const towers = game.towerTypes.length;
        shownTower = nextTower;
        nextTower = stepsLeft === 1 ? result : (nextTower + 1) % towers;
        offset = 0;
        const done = Theme.diceSteps - stepsLeft;
        reel.duration = Theme.diceFirstStep + done * done * Theme.diceSlowing;
        reelRun.restart();
    }

    implicitWidth: unit * Theme.cardWidth + 6 * gap
    implicitHeight: window + 4 * gap + unit
    opacity: rolling ? 1 : 0
    visible: opacity > 0

    Connections {
        target: root.game

        function onCardRolled(tower: int) {
            root.roll(tower);
        }

        function onMapChanged() {
            reelRun.stop();
            leave.stop();
            root.stepsLeft = 0;
            root.shownTower = 0;
            root.nextTower = 0;
        }
    }

    FramedPanel {
        anchors.fill: parent
        unit: root.unit
        seed: Theme.diceSeed
    }

    GameIcon {
        anchors.horizontalCenter: parent.horizontalCenter
        y: root.gap
        width: root.unit
        height: width
        rotation: root.spin
        name: "dice"
        fill: Theme.text
    }

    Rectangle {
        id: frame

        objectName: "diceWindow"
        anchors.horizontalCenter: parent.horizontalCenter
        y: root.unit + 2 * root.gap
        width: root.unit * Theme.cardWidth + 2 * root.gap
        height: root.window
        clip: true
        color: Theme.ironDark
        border.color: Theme.iron
        border.width: root.gap / 2

        // Only while it rolls: a map that did not load has no towers to show.
        Repeater {
            model: root.rolling ? 2 : 0

            delegate: TowerCard {
                required property int index

                x: root.gap
                y: root.gap + index * root.window - root.offset
                info: root.game.towerTypes[index === 0 ? root.shownTower : root.nextTower]
                unit: root.unit
            }
        }
    }

    NumberAnimation {
        id: spinning

        target: root
        property: "spin"
        from: 0
        to: Theme.diceSpin
        duration: Theme.diceMillis
        easing.type: Easing.OutCubic
    }

    SequentialAnimation {
        id: reelRun

        NumberAnimation {
            id: reel

            target: root
            property: "offset"
            from: 0
            to: root.window
        }

        ScriptAction {
            script: {
                if (root.stepsLeft > 1) {
                    root.stepsLeft -= 1;
                    root.step();
                } else {
                    // First the rest: rolling must not stop for a moment in between.
                    leave.restart();
                    root.stepsLeft = 0;
                    root.shownTower = root.result;
                    root.offset = 0;
                }
            }
        }
    }

    Timer {
        id: leave

        interval: Theme.diceRest
    }
}
