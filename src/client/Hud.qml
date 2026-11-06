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
    readonly property real unit: Math.min(Math.max(Theme.smallestUnit, density * Theme.fingerSize), width
                                          / Theme.unitsAcross)
    readonly property bool gameOver: game.won || game.lost
    readonly property bool failed: game.error.length > 0

    FramedPanel {
        objectName: "stats"
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: root.unit * Theme.inset
        unit: root.unit
        width: statsRow.implicitWidth + root.unit * Theme.statsPadding
        height: root.unit * Theme.statsHeight

        Row {
            id: statsRow

            anchors.centerIn: parent
            spacing: root.unit * Theme.inset

            GameIcon {
                anchors.verticalCenter: parent.verticalCenter
                width: root.unit * Theme.iconSize
                height: width
                name: "coin"
                fill: Theme.gold
            }

            Text {
                objectName: "goldLabel"
                anchors.verticalCenter: parent.verticalCenter
                text: root.game.gold
                color: Theme.gold
                font.family: Theme.face
                font.bold: true
                font.pixelSize: root.unit * Theme.statText
                Accessible.name: qsTr("Gold: %1").arg(root.game.gold)
            }

            Item {
                width: root.unit * Theme.gap
                height: 1
            }

            GameIcon {
                anchors.verticalCenter: parent.verticalCenter
                width: root.unit * Theme.iconSize
                height: width
                name: "heart"
                fill: Theme.heart
            }

            Text {
                objectName: "livesLabel"
                anchors.verticalCenter: parent.verticalCenter
                text: root.game.lives
                color: Theme.text
                font.family: Theme.face
                font.bold: true
                font.pixelSize: root.unit * Theme.statText
                Accessible.name: qsTr("Lives: %1").arg(root.game.lives)
            }
        }
    }

    HudFrame {
        around: topButtons
        unit: root.unit
    }

    HudFrame {
        around: towerActions
        unit: root.unit
    }

    HudFrame {
        around: priorityButtons
        unit: root.unit
    }

    HudFrame {
        around: towerList
        unit: root.unit
    }

    HudFrame {
        around: savePanel
        unit: root.unit
    }

    HudFrame {
        around: waveButton
        unit: root.unit
    }

    HudFrame {
        around: spellButtons
        unit: root.unit
    }

    HudFrame {
        around: placement
        unit: root.unit
    }

    Row {
        id: topButtons

        objectName: "buttons"
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: root.unit * Theme.gap
        spacing: root.unit * Theme.gap

        HudButton {
            objectName: "pauseButton"
            unit: root.unit
            icon: root.game.paused ? "play" : "pause"
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
                icon: "fast"
                caption: speed
                text: qsTr("%1x").arg(speed)
                highlighted: root.game.speed === speed
                onClicked: root.game.speed = speed
            }
        }
    }

    // What can be done with the chosen tower.
    Row {
        id: towerActions

        objectName: "towerActions"
        anchors.left: parent.left
        anchors.bottom: priorityButtons.top
        anchors.margins: root.unit * Theme.gap
        spacing: root.unit * Theme.gap
        visible: root.game.chosenSellValue >= 0

        // First in the row: at the highest level the upgrade goes away, and one tap too many on it
        // must not hit this button.
        HudButton {
            objectName: "sellButton"
            unit: root.unit
            icon: "trash"
            caption: root.game.chosenSellValue
            captionColor: Theme.gold
            text: qsTr("Sell: %1 gold").arg(root.game.chosenSellValue)
            onClicked: root.game.sellChosenTower()
        }

        HudButton {
            objectName: "upgradeButton"
            unit: root.unit
            visible: root.game.chosenUpgradeCost >= 0
            icon: "up"
            caption: root.game.chosenUpgradeCost
            captionColor: Theme.gold
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
        anchors.margins: root.unit * Theme.gap
        spacing: root.unit * Theme.gap
        visible: root.game.chosenPriority >= 0

        Repeater {
            model: [qsTr("First"), qsTr("Last"), qsTr("Strongest"), qsTr("Nearest")]

            delegate: HudButton {
                required property int index
                required property string modelData

                unit: root.unit
                icon: ["flag", "hourglass", "skull", "target"][index]
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
        anchors.margins: root.unit * Theme.gap
        width: Math.min(towerButtons.width, parent.width - placement.width - root.unit * Theme.gap
                        * 3)
        height: root.unit
        contentWidth: towerButtons.width
        flickableDirection: Flickable.HorizontalFlick
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        Row {
            id: towerButtons

            objectName: "towerButtons"
            spacing: root.unit * Theme.gap

            Repeater {
                model: root.game.towerTypes

                delegate: HudButton {
                    required property int index
                    required property var modelData

                    unit: root.unit
                    icon: modelData.name
                    caption: modelData.cost
                    captionColor: Theme.gold
                    text: qsTr("%1: %2 gold").arg(modelData.name).arg(modelData.cost)
                    highlighted: root.game.selectedTower === index
                    onClicked: root.game.selectedTower = index
                }
            }
        }
    }

    // A paused game can be put away, or be replaced by the one that was put away.
    Column {
        id: savePanel

        objectName: "savePanel"
        visible: root.game.paused
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: topButtons.bottom
        anchors.margins: root.unit * Theme.gap
        spacing: root.unit * Theme.gap

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: root.unit * Theme.gap

            HudButton {
                objectName: "saveButton"
                unit: root.unit
                available: root.game.canSave
                icon: "save"
                text: qsTr("Save")
                onClicked: root.game.saveGame()
            }

            HudButton {
                objectName: "loadButton"
                unit: root.unit
                available: root.game.hasSave
                icon: "load"
                text: qsTr("Load")
                onClicked: root.game.loadGame()
            }
        }

        Text {
            objectName: "saveError"
            visible: text.length > 0
            width: root.width / 2
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            text: root.game.saveError
            color: Theme.danger
            font.family: Theme.face
            font.pixelSize: root.unit * Theme.smallText
        }
    }

    HudButton {
        id: waveButton

        objectName: "waveButton"
        anchors.right: parent.right
        anchors.top: topButtons.bottom
        anchors.margins: root.unit * Theme.gap
        unit: root.unit
        available: root.game.canCallWave
        icon: "flag"
        caption: "+" + root.game.waveCallBonus
        captionColor: Theme.gold
        text: qsTr("Next wave +%1").arg(root.game.waveCallBonus)
        onClicked: root.game.callWave()
    }

    // What the next tap casts, and when each spell is ready again.
    Column {
        id: spellButtons

        objectName: "spellButtons"
        anchors.right: parent.right
        anchors.top: waveButton.bottom
        anchors.margins: root.unit * Theme.gap
        spacing: root.unit * Theme.gap

        Repeater {
            model: root.game.spellNames

            delegate: HudButton {
                required property int index
                required property string modelData
                readonly property int seconds: root.game.spellSeconds[index] ?? 0

                unit: root.unit
                icon: modelData
                caption: seconds > 0 ? seconds : ""
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
        anchors.margins: root.unit * Theme.gap
        spacing: root.unit * Theme.gap

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

            Repeater {
                model: root.game.selectedShape

                delegate: Rectangle {
                    required property var modelData

                    x: modelData.x * shape.cellSize
                    y: modelData.y * shape.cellSize
                    width: shape.cellSize - 1
                    height: shape.cellSize - 1
                    color: modelData.anchor ? Theme.ironDark : Theme.accent
                    border.color: Theme.accent
                }
            }
        }

        HudButton {
            objectName: "rotateButton"
            unit: root.unit
            icon: "rotate"
            text: qsTr("Rotate")
            onClicked: root.game.rotateSelection()
        }
    }

    FramedPanel {
        objectName: "endPanel"
        anchors.centerIn: parent
        visible: root.gameOver || root.failed
        width: Math.min(parent.width - root.unit, Math.max(root.unit * Theme.endWidth, message.implicitWidth
                                                           + root.unit))
        height: root.unit * Theme.endHeight
        unit: root.unit

        Text {
            id: message

            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: root.unit * Theme.endMargin
            width: parent.width - root.unit
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            text: root.failed ? root.game.error : root.game.won ? qsTr("Victory") : qsTr("Defeat")
            color: Theme.text
            font.family: Theme.face
            font.bold: true
            font.pixelSize: root.unit * Theme.titleText
        }

        HudButton {
            objectName: "restartButton"
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: root.unit * Theme.endMargin
            visible: !root.failed
            unit: root.unit
            text: qsTr("Play again")
            onClicked: root.game.restart()
        }
    }
}
