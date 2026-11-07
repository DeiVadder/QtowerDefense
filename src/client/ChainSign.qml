pragma ComponentBehavior: Bound

import QtQuick

// A board that hangs on two chains and swings when swing() is called. What it shows goes inside, on
// the board below the chains.
Item {
    id: root

    required property real unit
    property real chain: unit * Theme.signChain
    property real bracketUnit: unit * Theme.signBracket
    property color tone: Theme.wood
    property int seed: Theme.signSeed
    default property alias content: board.data

    function swing() {
        swinging.restart();
    }

    transformOrigin: Item.Top

    Repeater {
        model: 2

        delegate: Rectangle {
            required property int index

            x: (index === 0 ? Theme.chainInset : 1 - Theme.chainInset) * root.width - width / 2
            width: root.unit * Theme.bracketBar
            height: root.chain + width
            color: Theme.ironDark
            border.color: Theme.ironLight
        }
    }

    FramedPanel {
        id: board

        y: root.chain
        width: parent.width
        height: parent.height - root.chain
        unit: root.bracketUnit
        tone: root.tone
        seed: root.seed
        takesTaps: false
    }

    SequentialAnimation {
        id: swinging

        NumberAnimation {
            target: root
            property: "rotation"
            from: Theme.swingStart
            to: Theme.swingOut
            duration: Theme.swingTime
            easing.type: Easing.OutQuad
        }
        NumberAnimation {
            target: root
            property: "rotation"
            to: Theme.swingBack
            duration: Theme.swingTime
            easing.type: Easing.InOutQuad
        }
        NumberAnimation {
            target: root
            property: "rotation"
            to: 0
            duration: Theme.swingRest
        }
    }
}
