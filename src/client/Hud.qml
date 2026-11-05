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

    // What a tap on the map builds.
    Flow {
        objectName: "towerButtons"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: root.unit / 4
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
