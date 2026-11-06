pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Window
import QtowerDefense.Client

// Shows the state of the game and offers everything that is no tap on the map. Sizes follow the
// pixel density, so that a finger can hit every button, and shrink when a window is too narrow for
// everything to stand side by side.
Item {
    id: root

    required property GameSession game

    property real density: Screen.pixelDensity
    readonly property real unit: Math.min(Math.max(40, density * 9), width / 14)
    readonly property bool gameOver: game.won || game.lost
    readonly property bool failed: game.error.length > 0

    Column {
        objectName: "stats"
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: root.unit / 4

        Text {
            objectName: "goldLabel"
            text: qsTr("Gold: %1").arg(root.game.gold)
            color: "#f0d97a"
            font.pixelSize: root.unit * 0.45
        }

        Text {
            objectName: "livesLabel"
            text: qsTr("Lives: %1").arg(root.game.lives)
            color: "#f0f4f8"
            font.pixelSize: root.unit * 0.45
        }
    }

    Row {
        id: topButtons

        objectName: "buttons"
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: root.unit / 4
        spacing: root.unit / 4

        HudButton {
            objectName: "pauseButton"
            unit: root.unit
            text: root.game.paused ? qsTr("Resume") : qsTr("Pause")
            highlighted: root.game.paused
            onClicked: root.game.paused = !root.game.paused
        }

        Repeater {
            model: root.game.maxSpeed

            delegate: HudButton {
                required property int index
                readonly property int speed: index + 1

                unit: root.unit
                text: qsTr("%1x").arg(speed)
                highlighted: root.game.speed === speed
                onClicked: root.game.speed = speed
            }
        }
    }

    // What can be done with the chosen tower.
    Row {
        objectName: "towerActions"
        anchors.left: parent.left
        anchors.bottom: priorityButtons.top
        anchors.margins: root.unit / 4
        spacing: root.unit / 4
        visible: root.game.chosenSellValue >= 0

        // First in the row: at the highest level the upgrade goes away, and one tap too many on it
        // must not hit this button.
        HudButton {
            objectName: "sellButton"
            unit: root.unit
            text: qsTr("Sell: %1 gold").arg(root.game.chosenSellValue)
            onClicked: root.game.sellChosenTower()
        }

        HudButton {
            objectName: "upgradeButton"
            unit: root.unit
            visible: root.game.chosenUpgradeCost >= 0
            text: qsTr("Upgrade: %1 gold").arg(root.game.chosenUpgradeCost)
            onClicked: root.game.upgradeChosenTower()
        }
    }

    // What the chosen tower aims at, the order is that of core::TargetPriority.
    Row {
        id: priorityButtons

        objectName: "priorityButtons"
        anchors.left: parent.left
        anchors.bottom: towerList.top
        anchors.margins: root.unit / 4
        spacing: root.unit / 4
        visible: root.game.chosenPriority >= 0

        Repeater {
            model: [qsTr("First"), qsTr("Last"), qsTr("Strongest"), qsTr("Nearest")]

            delegate: HudButton {
                required property int index
                required property string modelData

                unit: root.unit
                text: modelData
                highlighted: root.game.chosenPriority === index
                onClicked: root.game.setChosenPriority(index)
            }
        }
    }

    // What a tap on the map builds. The row scrolls, so that the list never covers more of the map.
    Flickable {
        id: towerList

        objectName: "towerList"
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: root.unit / 4
        width: Math.min(towerButtons.width, parent.width - placement.width - root.unit * 3 / 4)
        height: root.unit
        contentWidth: towerButtons.width
        flickableDirection: Flickable.HorizontalFlick
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        Row {
            id: towerButtons

            objectName: "towerButtons"
            spacing: root.unit / 4

            Repeater {
                model: root.game.towerTypes

                delegate: HudButton {
                    required property int index
                    required property var modelData

                    unit: root.unit
                    text: qsTr("%1: %2 gold").arg(modelData.name).arg(modelData.cost)
                    highlighted: root.game.selectedTower === index
                    onClicked: root.game.selectedTower = index
                }
            }
        }
    }

    // What the next tap casts, and when each spell is ready again.
    Column {
        objectName: "spellButtons"
        anchors.right: parent.right
        anchors.top: topButtons.bottom
        anchors.margins: root.unit / 4
        spacing: root.unit / 4

        Repeater {
            model: root.game.spellNames

            delegate: HudButton {
                required property int index
                required property string modelData
                readonly property int seconds: root.game.spellSeconds[index] ?? 0

                unit: root.unit
                text: seconds > 0 ? qsTr("%1: %2 s").arg(modelData).arg(seconds) : modelData
                highlighted: root.game.selectedSpell === index
                onClicked: root.game.selectSpell(index)
            }
        }
    }

    // How it will stand: the dark cell is the one that is tapped.
    Row {
        id: placement

        objectName: "rotation"
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: root.unit / 4
        spacing: root.unit / 4

        Item {
            id: shape

            readonly property int extent: root.game.selectedShape.reduce((most, cell) => Math.max(most,
                                                                                                  cell.x + 1,
                                                                                                  cell.y + 1),
            1)
            readonly property real cellSize: root.unit / extent

            objectName: "shape"
            width: root.unit
            height: root.unit

            // The shape lies over the map, a tap on it builds nothing there.
            MouseArea {
                anchors.fill: parent
            }

            Repeater {
                model: root.game.selectedShape

                delegate: Rectangle {
                    required property var modelData

                    x: modelData.x * shape.cellSize
                    y: modelData.y * shape.cellSize
                    width: shape.cellSize - 1
                    height: shape.cellSize - 1
                    color: modelData.anchor ? "#8a6d12" : "#c9a227"
                }
            }
        }

        HudButton {
            objectName: "rotateButton"
            unit: root.unit
            text: qsTr("Rotate")
            onClicked: root.game.rotateSelection()
        }
    }

    Rectangle {
        objectName: "endPanel"
        anchors.centerIn: parent
        visible: root.gameOver || root.failed
        width: Math.min(parent.width - root.unit, Math.max(root.unit * 8, message.implicitWidth
                                                           + root.unit))
        height: root.unit * 3.5
        radius: root.unit / 4
        color: "#dd1d2733"

        // The panel lies over the map, a click on it means nothing there.
        MouseArea {
            anchors.fill: parent
        }

        Text {
            id: message

            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: root.unit / 2
            width: parent.width - root.unit
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            text: root.failed ? root.game.error : root.game.won ? qsTr("Victory") : qsTr("Defeat")
            color: "#f0f4f8"
            font.pixelSize: root.unit * 0.6
        }

        HudButton {
            objectName: "restartButton"
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: root.unit / 2
            visible: !root.failed
            unit: root.unit
            text: qsTr("Play again")
            onClicked: root.game.restart()
        }
    }
}
