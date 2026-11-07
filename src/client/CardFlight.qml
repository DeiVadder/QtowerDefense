import QtQuick
import QtowerDefense.Client

// The card in the air: held by the finger, then falling onto its cell, where the model stands by
// then. It takes no space of its own, the HUD asks what lies under a dropped card. The game is done
// with the card before it flies, so nothing depends on this animation.
Item {
    id: root

    required property real unit
    property var heldInfo: ({
                                name: "",
                                cost: 0,
                                damage: 0,
                                range: 0,
                                rate: 0
                            })
    property bool holding: false
    property point held

    readonly property bool flying: fall.running

    function fly(info: var, from: point, to: point) {
        flyer.info = info;
        flyer.x = from.x - flyer.width / 2;
        flyer.y = from.y - flyer.height / 2;
        toX.to = to.x - flyer.width / 2;
        toY.to = to.y - flyer.height / 2;
        fall.restart();
    }

    TowerCard {
        objectName: "heldCard"
        x: root.held.x - width / 2
        y: root.held.y - height / 2
        visible: root.holding
        info: root.heldInfo
        unit: root.unit
        highlighted: true
        scale: Theme.cardHeldScale
    }

    TowerCard {
        id: flyer

        objectName: "flyingCard"
        visible: root.flying
        info: root.heldInfo
        unit: root.unit
        highlighted: true
        transform: Rotation {
            id: tilt

            origin.x: flyer.width / 2
            origin.y: flyer.height / 2
            axis.x: 1
            axis.y: 0
            axis.z: 0
        }
    }

    ParallelAnimation {
        id: fall

        NumberAnimation {
            id: toX

            target: flyer
            property: "x"
            duration: Theme.flightMillis
            easing.type: Easing.InQuad
        }
        NumberAnimation {
            id: toY

            target: flyer
            property: "y"
            duration: Theme.flightMillis
            easing.type: Easing.InQuad
        }
        NumberAnimation {
            target: flyer
            property: "scale"
            from: Theme.cardHeldScale
            to: Theme.cardLandedScale
            duration: Theme.flightMillis
        }
        NumberAnimation {
            target: tilt
            property: "angle"
            from: 0
            to: Theme.cardLandedTilt
            duration: Theme.flightMillis
        }
        NumberAnimation {
            target: flyer
            property: "opacity"
            from: 1
            to: 0
            duration: Theme.flightMillis
            easing.type: Easing.InCubic
        }
    }
}
