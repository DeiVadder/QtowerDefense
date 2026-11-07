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
    property MainScene scene: null // who knows the map under a dropped card

    property var dropPoint: null // while a dropped card builds
    property real density: Screen.pixelDensity
    property bool heroSpeaks: true
    property int best: 0 // the most rounds of this map's endless game before this game
    readonly property real unit: Math.min(Math.max(Theme.smallestUnit, density * Theme.fingerSize), width
                                          / Theme.unitsAcross)
    readonly property real cardUnit: Math.min(unit, height / Theme.handShare)
    readonly property bool gameOver: game.won || game.lost
    readonly property bool failed: game.error.length > 0

    signal leave // back to the world map

    Binding {
        when: root.scene !== null
        target: root.scene
        property: "gridStrength"
        value: towerList.dragging ? Theme.gridHeld : Theme.gridFaint
    }

    Binding {
        when: root.scene !== null
        target: root.scene
        property: "cardInHand"
        value: towerList.dragging
    }

    Connections {
        target: root.game

        function onTowerBuilt(x: int, y: int) {
            if (root.scene !== null) {
                const spot = root.scene.cellOnScreen(x, y);
                flight.fly(root.game.towerTypes[root.game.selectedTower], root.dropPoint
                           ?? towerList.chosenCenter(), Qt.point(spot.x, spot.y));
            }
        }
    }

    // In the corner, as large as the buttons at the left leave room for.
    Hero {
        id: hero

        speaks: root.heroSpeaks

        readonly property real ring: root.unit * (Theme.dialGap + Theme.dialButton / 2)
        readonly property real room: root.height - root.cardUnit * Theme.cardPeek - root.unit * (Theme.leftRows
                                                                                                 + (Theme.leftRows
                                                                                                    + 1) * Theme.gap
                                                                                                 + Theme.leftInsets
                                                                                                 * Theme.inset)
        // A low window has no room for the bow, the clock then stands in a row beside the portrait.
        readonly property bool compact: room - ring < root.unit * Theme.heroSize

        objectName: "hero"
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: root.unit * Theme.inset
        size: Math.max(root.unit, Math.min(root.unit * Theme.heroSize, room))
        bubbleLeft: size + (compact ? 0 : ring) + root.unit * Theme.gap
        bubbleTop: Math.max(stats.y + stats.height - y, compact ? size : 0)
        game: root.game
        unit: root.unit
    }

    FramedPanel {
        id: stats

        objectName: "stats"
        anchors.left: hero.compact ? topButtons.right : hero.right
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

            Item {
                visible: root.game.endless
                width: root.unit * Theme.gap
                height: 1
            }

            GameIcon {
                visible: root.game.endless
                anchors.verticalCenter: parent.verticalCenter
                width: root.unit * Theme.iconSize
                height: width
                name: "flag"
                fill: Theme.text
            }

            Text {
                objectName: "roundLabel"
                visible: root.game.endless
                anchors.verticalCenter: parent.verticalCenter
                text: root.game.round + 1
                color: Theme.text
                font.family: Theme.face
                font.bold: true
                font.pixelSize: root.unit * Theme.statText
                Accessible.name: qsTr("Round: %1").arg(root.game.round + 1)
            }
        }
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
        around: cardButtons
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

    // The clock stands around the portrait, along its lower right.
    Item {
        id: topButtons

        readonly property real reach: hero.size / 2 + root.unit * Theme.dialGap
        readonly property real button: root.unit * Theme.dialButton
        readonly property int places: root.game.maxSpeed + 1
        readonly property real gap: root.unit * Theme.inset

        function placed(place: int): point {
            if (hero.compact) {
                return Qt.point(hero.size + gap + place * (button + gap), 0);
            }
            const angle = place * Theme.dialTurn / (places - 1) * Math.PI / 180;
            return Qt.point(hero.size / 2 + Math.cos(angle) * reach - button / 2, hero.size / 2 + Math.sin(
                                angle) * reach - button / 2);
        }

        // As large as the buttons: the HUD asks what lies under a dropped card.
        objectName: "buttons"
        anchors.left: hero.left
        anchors.top: hero.top
        anchors.topMargin: hero.compact ? (hero.size - button) / 2 : 0
        width: hero.size + (hero.compact ? places * (button + gap) + gap : hero.ring)
        height: hero.compact ? button : hero.size + hero.ring

        HudButton {
            objectName: "pauseButton"
            x: topButtons.placed(0).x
            y: topButtons.placed(0).y
            width: topButtons.button
            height: topButtons.button
            radius: width / 2
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

                x: topButtons.placed(speed).x
                y: topButtons.placed(speed).y
                width: topButtons.button
                height: topButtons.button
                radius: width / 2
                unit: root.unit
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

    // What a tap on the map builds, or what gets dragged there.
    Hand {
        id: towerList

        objectName: "towerList"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: root.cardUnit * Theme.cardLift
        game: root.game
        unit: root.cardUnit
        heldBack: dice.rolling ? 1 : 0
        maxWidth: parent.width - 2 * (Math.max(placement.width, cardButtons.width) + root.unit
                                      * Theme.gap * 2)
        lowered: root.game.chosenSellValue >= 0
        onDropped: (x, y) => {
            // A card over the HUD goes back into the hand.
            if (root.scene !== null && root.childAt(x, y) === null) {
                root.dropPoint = Qt.point(x, y);
                root.scene.dropCardAt(x, y);
                root.dropPoint = null;
            }
        }
    }

    // A paused game can be put away, or be replaced by the one that was put away.
    Column {
        id: savePanel

        objectName: "savePanel"
        visible: root.game.paused
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: Math.max(stats.y + stats.height, hero.compact ? hero.y + hero.size : 0)
                           + root.unit * Theme.gap
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
        anchors.top: parent.top
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

    // Where cards come from, and where they go.
    Row {
        id: cardButtons

        objectName: "cardButtons"
        anchors.right: parent.right
        anchors.bottom: placement.top
        anchors.margins: root.unit * Theme.gap
        spacing: root.unit * Theme.gap

        HudButton {
            objectName: "rollButton"
            unit: root.unit
            icon: "dice"
            caption: root.game.rollCost
            captionColor: Theme.gold
            text: qsTr("Roll for a card")
            available: !dice.rolling
            onClicked: root.game.rollCard()
        }

        HudButton {
            objectName: "shopButton"
            unit: root.unit
            icon: "bag"
            text: qsTr("Buy a card")
            available: !dice.rolling // the hand holds back the last card until the dice stop
            onClicked: shop.open = true
        }

        HudButton {
            objectName: "discardButton"
            unit: root.unit
            icon: "trash"
            text: qsTr("Throw the chosen card away")
            highlighted: root.game.hand.length >= root.game.handLimit
            available: root.game.chosenCard >= 0
            onClicked: root.game.discardCard()
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

    CardFlight {
        id: flight

        unit: root.cardUnit
        holding: towerList.dragging
        held: towerList.held
        heldInfo: root.game.towerTypes[root.game.selectedTower] ?? noTower
    }

    DiceRoll {
        id: dice

        objectName: "dice"
        anchors.centerIn: parent
        game: root.game
        unit: root.cardUnit
    }

    CardShop {
        id: shop

        objectName: "shop"
        anchors.fill: parent
        game: root.game
        unit: root.unit
        cardUnit: root.cardUnit
    }

    EndPanel {
        objectName: "endPanel"
        anchors.centerIn: parent
        visible: root.gameOver || root.failed
        game: root.game
        unit: root.unit
        failed: root.failed
        best: root.best
        onLeave: root.leave()
    }
}
