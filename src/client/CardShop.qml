pragma ComponentBehavior: Bound

import QtQuick
import QtowerDefense.Client

// The armoury: exactly the tower you want, at its full price. A tap beside the board closes it.
Item {
    id: root

    required property GameSession game
    required property real unit
    required property real cardUnit

    property bool open: false

    readonly property int count: game.towerTypes.length
    readonly property real gap: unit * Theme.gap
    // As many as there are, side by side, as large as the window allows.
    readonly property real shopUnit: Math.min(cardUnit, (width - (count + 3) * gap) / (count
                                                                                       * Theme.cardWidth))

    visible: open && !game.won && !game.lost

    Connections {
        target: root.game

        function onMapChanged() {
            root.open = false;
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: root.open = false
    }

    FramedPanel {
        id: board

        objectName: "shopBoard"
        anchors.centerIn: parent
        width: cards.width + 4 * root.gap
        height: cards.height + 4 * root.gap
        unit: root.unit
        seed: Theme.shopSeed
    }

    Row {
        id: cards

        objectName: "shopCards"
        anchors.centerIn: board
        spacing: root.gap

        Repeater {
            model: root.game.towerTypes

            delegate: TowerCard {
                id: card

                required property int index
                required property var modelData
                readonly property bool affordable: modelData.cost <= root.game.gold

                objectName: "shopCard"
                info: modelData
                unit: root.shopUnit
                opacity: affordable ? 1 : Theme.unavailable

                onPicked: {
                    if (root.game.buyCard(index)) {
                        root.open = false;
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: card.picked()
                }
            }
        }
    }

    HudButton {
        objectName: "shopClose"
        x: board.x + board.width - width / 2 - root.gap
        y: board.y - height / 2 + root.gap
        unit: root.unit
        icon: "close"
        text: qsTr("Close")
        onClicked: root.open = false
    }
}
