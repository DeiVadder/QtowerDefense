pragma ComponentBehavior: Bound

import QtQuick
import QtowerDefense.Client

// The cards at the bottom edge, a stiff fan on a shared pivot, as tight as it takes to fit. A tap chooses one, for building with
// a tap on the map. A chosen card can also be dragged there, also straight from the hand.
Item {
    id: root

    required property GameSession game
    required property real unit
    required property real maxWidth
    property bool lowered: false // the room above is needed for the buttons of a tower

    // One card at a time, also with two fingers on the hand.
    property int owner: -1
    property int games: 0
    readonly property bool dragging: owner >= 0
    property point held // the finger during a drag, in the coordinates of the parent

    readonly property real cardWidth: unit * Theme.cardWidth
    readonly property int count: game.towerTypes.length
    readonly property real step: count < 2 ? 0 : Math.min(cardWidth * Theme.cardStep, (maxWidth
                                                                                       - cardWidth)
                                                          / (count - 1))

    signal dropped(real x, real y)

    // The middle of the chosen card, in the coordinates of the parent.
    function chosenCenter(): point {
        const card = cards.itemAt(game.selectedTower);
        return card === null ? Qt.point(x, y) : card.mapToItem(parent, card.width / 2, card.height
                                                               / 2);
    }

    width: cardWidth + step * Math.max(0, count - 1)
    height: unit * (lowered ? Theme.cardPeek : Theme.cardHeight)

    Behavior on height {
        NumberAnimation {
            duration: Theme.cardMillis
        }
    }

    Connections {
        target: root.game

        function onMapChanged() {
            root.owner = -1;
            ++root.games;
        }
    }

    Item {
        objectName: "towerButtons"
        anchors.fill: parent

        Repeater {
            id: cards

            model: root.game.towerTypes

            delegate: TowerCard {
                id: card

                required property int index
                required property var modelData
                readonly property bool chosen: root.game.selectedTower === index
                readonly property bool raised: chosen && !root.lowered
                readonly property real fan: index - (root.count - 1) / 2
                readonly property real outerFan: (root.count - 1) / 2

                info: modelData
                unit: root.unit
                highlighted: chosen
                x: index * root.step
                y: (fan * fan - outerFan * outerFan) * root.unit * Theme.cardSag - (raised
                                                                                    ? root.unit
                                                                                      * Theme.cardRaise :
                                                                                      0)

                z: chosen ? root.count : index
                rotation: raised ? 0 : fan * Theme.cardTilt
                scale: raised ? Theme.cardChosenScale : 1
                // Not visible: a hidden item loses the finger.
                opacity: root.owner === index ? 0 : 1

                Behavior on y {
                    NumberAnimation {
                        duration: Theme.cardMillis
                        easing.type: Easing.OutBack
                    }
                }

                onPicked: root.game.selectedTower = index

                // Not a TapHandler: it only watches, and the map below would take the same click.
                MouseArea {
                    property point start
                    property int game: -1 // a new game ends what started in the old one

                    anchors.fill: parent
                    preventStealing: true
                    onPressed: mouse => {
                        start = mapToItem(root.parent, mouse.x, mouse.y);
                        game = root.games;
                    }
                    onClicked: {
                        if (root.owner < 0 && game === root.games) {
                            card.picked();
                        }
                    }
                    onPositionChanged: mouse => {
                        const place = mapToItem(root.parent, mouse.x, mouse.y);
                        const way = Math.hypot(place.x - start.x, place.y - start.y);
                        if (root.owner < 0 && game === root.games && way >= card.unit
                                * Theme.cardDragStart) {
                            card.picked();
                            root.owner = card.index;
                        }
                        if (root.owner === card.index) {
                            root.held = place;
                        }
                    }
                    onReleased: mouse => {
                        if (root.owner === card.index) {
                            root.held = mapToItem(root.parent, mouse.x, mouse.y);
                            root.owner = -1;
                            root.dropped(root.held.x, root.held.y);
                        }
                    }
                    onCanceled: {
                        if (root.owner === card.index) {
                            root.owner = -1;
                        }
                    }
                }
            }
        }
    }
}
