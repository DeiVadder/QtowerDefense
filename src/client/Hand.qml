pragma ComponentBehavior: Bound

import QtQuick
import QtowerDefense.Client

// The cards of the player at the bottom edge, a stiff fan on a shared pivot, as tight as it takes to fit. A tap chooses one, for building with
// a tap on the map. A chosen card can also be dragged there, also straight from the hand.
Item {
    id: root

    required property GameSession game
    required property real unit
    required property real maxWidth
    property bool lowered: false // the room above is needed for the buttons of a tower
    property int heldBack: 0 // the last cards do not show yet

    // One card at a time, also with two fingers on the hand.
    property int owner: -1
    property int games: 0
    readonly property bool dragging: owner >= 0
    property point held // the finger during a drag, in the coordinates of the parent

    readonly property real cardWidth: unit * Theme.cardWidth
    readonly property var shown: game.hand.slice(0, Math.max(0, game.hand.length - heldBack))
    readonly property int count: slots.count
    readonly property real step: count < 2 ? 0 : Math.min(cardWidth * Theme.cardStep, (maxWidth
                                                                                       - cardWidth)
                                                          / (count - 1))

    signal dropped(real x, real y)

    // Keeps the cards that stay: a finger holds on to its card while others come and go.
    function sync() {
        // A card that goes takes the drag along, also with the finger still on the next one.
        if (shown.length < slots.count) {
            owner = -1;
            ++games;
        }
        while (slots.count > shown.length) {
            slots.remove(slots.count - 1);
        }
        shown.forEach((tower, slot) => {
            if (slot === slots.count) {
                slots.append({
                                 tower: tower
                             });
            } else {
                slots.setProperty(slot, "tower", tower);
            }
        });
    }

    onShownChanged: sync()
    Component.onCompleted: sync()

    // The middle of the chosen card, in the coordinates of the parent.
    function chosenCenter(): point {
        const card = cards.itemAt(game.chosenCard);
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

    ListModel {
        id: slots
    }

    Item {
        objectName: "towerButtons"
        anchors.fill: parent

        Repeater {
            id: cards

            model: slots

            delegate: TowerCard {
                id: card

                required property int index
                required property int tower
                readonly property bool chosen: root.game.chosenCard === index
                readonly property bool raised: chosen && !root.lowered
                readonly property real fan: index - (root.count - 1) / 2
                readonly property real outerFan: (root.count - 1) / 2

                info: root.game.towerTypes[tower]
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

                onPicked: root.game.chooseCard(index)

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
