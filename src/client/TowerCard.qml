pragma ComponentBehavior: Bound

import QtQuick
import QtowerDefense.Client

// A tower as a playing card, a board of planks: drawn in a box of 134 by 190 that is scaled to the
// size of this item. The numbers stand on iron tags down the left edge, where the fan leaves them
// visible, the icon is burnt into the wood.
Item {
    id: root

    required property var info // one entry of GameSession.towerTypes
    required property real unit
    property bool highlighted: false

    // Content may bring more towers than this client has words for.
    readonly property var titles: ({
                                       ballista: qsTr("Ballista"),
                                       twin_ballista: qsTr("Twin Ballista"),
                                       bastion: qsTr("Bastion"),
                                       horseshoe: qsTr("Horseshoe"),
                                       frost_spire: qsTr("Frost Spire"),
                                       venom_spire: qsTr("Venom Spire"),
                                       catapult: qsTr("Catapult")
                                   })
    readonly property string title: titles[info.name] ?? info.name
    readonly property string text: qsTr("%1: %2 gold").arg(title).arg(info.cost)

    signal picked

    width: unit * Theme.cardWidth
    height: unit * Theme.cardHeight

    Accessible.role: Accessible.Button
    Accessible.name: text
    Accessible.onPressAction: picked()

    Item {
        width: 134
        height: 190
        scale: root.width / 134
        transformOrigin: Item.TopLeft

        FramedPanel {
            anchors.fill: parent
            unit: 45
            tone: Theme.cardWood
            seed: root.info.name.length + 3
            plankUnits: Theme.cardPlank
        }

        Rectangle {
            x: 54
            y: 40
            width: 72
            height: 98
            radius: 4
            color: Theme.cardWoodDark
            border.color: Theme.woodDark
            border.width: 2
        }

        GameIcon {
            x: 58
            y: 59
            width: 64
            height: 64
            name: root.info.name
            fill: root.highlighted ? Theme.accent : Theme.burnt
        }

        Column {
            x: 8
            y: 42
            spacing: 4

            Repeater {
                model: [
                    {
                        icon: "sword",
                        value: Math.round(root.info.damage * 10) / 10
                    },
                    {
                        icon: "target",
                        value: Math.round(root.info.range * 10) / 10
                    },
                    {
                        icon: "fast",
                        value: Math.round(root.info.rate * 10) / 10
                    }
                ]

                delegate: Rectangle {
                    id: tag

                    required property var modelData

                    objectName: "card_" + modelData.icon
                    width: 44
                    height: 30
                    color: Theme.iron
                    border.color: Theme.ironDark
                    border.width: 2

                    GameIcon {
                        x: 3
                        y: 8
                        width: 13
                        height: 13
                        name: tag.modelData.icon
                    }

                    Text {
                        objectName: "value"
                        x: 17
                        width: 25
                        height: parent.height
                        verticalAlignment: Text.AlignVCenter
                        fontSizeMode: Text.HorizontalFit
                        minimumPixelSize: 9
                        text: tag.modelData.value
                        color: Theme.gold
                        font.family: Theme.face
                        font.pixelSize: 14
                        font.bold: true
                    }
                }
            }
        }

        Rectangle {
            x: 8
            y: 8
            width: 118
            height: 28
            color: Theme.ironDark
            border.color: Theme.iron
            border.width: 2

            Text {
                anchors.fill: parent
                anchors.margins: 3
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                text: root.title
                color: Theme.text
                font.family: Theme.face
                font.pixelSize: 15
                font.bold: true
                fontSizeMode: Text.Fit
                minimumPixelSize: 9
            }
        }

        Rectangle {
            x: 8
            y: 146
            width: costRow.implicitWidth + 12
            height: 34
            color: Theme.ironDark
            border.color: Theme.iron
            border.width: 2

            Row {
                id: costRow

                anchors.centerIn: parent
                spacing: 4

                GameIcon {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 24
                    height: 24
                    name: "coin"
                    fill: Theme.gold
                }

                Text {
                    objectName: "cardCost"
                    text: root.info.cost
                    color: Theme.gold
                    font.family: Theme.face
                    font.pixelSize: 22
                    font.bold: true
                }
            }
        }

        Rectangle {
            anchors.fill: parent
            visible: root.highlighted
            color: "transparent"
            border.color: Theme.accent
            border.width: 4
        }
    }
}
