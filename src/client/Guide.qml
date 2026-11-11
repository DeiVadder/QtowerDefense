pragma ComponentBehavior: Bound

import QtQuick
import QtowerDefense.Client

// The first game shows the player around (E-53): roll, choose, turn, build, start. Each step points
// at its control and waits until the player has done it; who does more at once skips ahead. The game
// stands still until the last step. The shade only darkens, every tap still reaches the game.
Item {
    id: root

    enum Step {
        Roll,
        Choose,
        Turn,
        Build,
        Start,
        Done
    }

    required property GameSession game
    required property real unit
    // A game on the screen that was never guided before.
    property bool wanted: false
    property list<Item> targets: [] // the dice, the hand, the rotate button, the pause button

    property bool running: false
    property int step: Guide.Step.Roll
    property int rotationAtTurn: 0
    property bool built: false
    property bool pausedBefore: false // how the player had the game when the guide began
    property rect hole: Qt.rect(0, 0, 0, 0)

    readonly property list<string> lines: [qsTr(
            "Towers come as cards. Tap the dice to roll for one."), qsTr(
            "A new card! Tap it to choose that tower."), qsTr(
            "Towers can turn. Tap here to turn yours, the dark cell is where you tap the map."),
        qsTr("Now tap a free field beside the path to build there. You can also drag the card onto the map."),
        qsTr("The enemies wait for you. Tap play when you are ready. Good luck!")]
    readonly property real holeMargin: unit * 0.15
    readonly property color shade: "#9e000000"

    signal finished // done or skipped, either way not again

    visible: running
    onWantedChanged: wanted ? begin() : stop()
    onStepChanged: sign.swing()
    Component.onCompleted: {
        if (wanted) {
            begin();
        }
    }

    function begin() {
        built = false;
        step = Guide.Step.Roll;
        running = true;
        pausedBefore = game.paused;
        game.paused = true;
        settle();
    }

    function stop() {
        running = false;
    }

    function finish() {
        running = false;
        finished();
    }

    function targetOf(which: int): Item {
        return [targets[0], targets[1], targets[2], null, targets[3]][which] ?? null;
    }

    function done(which: int): bool {
        switch (which) {
        case Guide.Step.Roll:
            return built || game.hand.length > 0;
        case Guide.Step.Choose:
            return built || game.chosenCard >= 0;
        case Guide.Step.Turn:
            return built || game.selectedRotation !== rotationAtTurn;
        case Guide.Step.Build:
            return built;
        default:
            return !game.paused;
        }
    }

    // Goes on past every step that is done, the player may be ahead of the guide.
    function settle() {
        if (!running) {
            return;
        }
        while (step < Guide.Step.Done && done(step)) {
            ++step;
            if (step === Guide.Step.Turn) {
                rotationAtTurn = game.selectedRotation;
            }
        }
        if (step === Guide.Step.Done) {
            finish();
        }
    }

    function place() {
        const target = running ? targetOf(step) : null;
        let found = Qt.rect(0, 0, 0, 0);
        if (target && target.visible) {
            const corner = target.mapToItem(root, 0, 0);
            found = Qt.rect(corner.x - holeMargin, corner.y - holeMargin, target.width + 2
                            * holeMargin, target.height + 2 * holeMargin);
        }
        if (found !== hole) {
            hole = found;
        }
    }

    // The controls move: the hand grows, the window changes. The hole follows them every frame.
    FrameAnimation {
        running: root.running
        onTriggered: root.place()
    }

    Connections {
        target: root.game
        enabled: root.running

        function onHandChanged() {
            root.settle();
        }
        function onSelectionChanged() {
            root.settle();
        }
        function onClockChanged() {
            root.settle();
        }
        function onTowerBuilt() {
            root.built = true;
            root.settle();
        }
    }

    // The shade around the hole, in four pieces, so that the target stays bright.
    Item {
        anchors.fill: parent
        visible: root.hole.width > 0

        Rectangle {
            width: parent.width
            height: root.hole.y
            color: root.shade
        }
        Rectangle {
            y: root.hole.y + root.hole.height
            width: parent.width
            height: Math.max(0, parent.height - y)
            color: root.shade
        }
        Rectangle {
            y: root.hole.y
            width: root.hole.x
            height: root.hole.height
            color: root.shade
        }
        Rectangle {
            x: root.hole.x + root.hole.width
            y: root.hole.y
            width: Math.max(0, parent.width - x)
            height: root.hole.height
            color: root.shade
        }

        Rectangle {
            objectName: "guideRing"
            x: root.hole.x
            y: root.hole.y
            width: root.hole.width
            height: root.hole.height
            radius: root.unit * Theme.corner
            color: "transparent"
            border.color: Theme.accent
            border.width: Math.max(2, root.unit * 0.08)

            SequentialAnimation on opacity {
                running: root.running
                loops: Animation.Infinite

                NumberAnimation {
                    to: 0.35
                    duration: 600
                    easing.type: Easing.InOutSine
                }
                NumberAnimation {
                    to: 1
                    duration: 600
                    easing.type: Easing.InOutSine
                }
            }
        }
    }

    // What to do, on a sign in the upper middle, away from the controls in the corners.
    ChainSign {
        id: sign

        objectName: "guideSign"
        anchors.horizontalCenter: parent.horizontalCenter
        // Below the words of the hero; while building, higher, to leave the middle of the map free.
        y: parent.height * (root.step === Guide.Step.Build ? 0.12 : 0.3)
        width: Math.min(root.unit * 11, parent.width - 2 * root.unit * Theme.gap)
        height: words.height + buttons.height + 3 * root.unit * Theme.gap + chain
        unit: root.unit
        tone: Theme.signWood

        Text {
            id: words

            objectName: "guideText"
            x: root.unit * Theme.gap
            y: root.unit * Theme.gap
            width: sign.width - 2 * x
            wrapMode: Text.Wrap
            text: root.running && root.step < Guide.Step.Done ? root.lines[root.step] : ""
            color: Theme.burnt
            font.family: Theme.face
            font.bold: true
            font.pixelSize: root.unit * Theme.signText
            Accessible.role: Accessible.StaticText
            Accessible.name: text
        }

        Row {
            id: buttons

            anchors.right: parent.right
            anchors.rightMargin: root.unit * Theme.gap
            y: words.y + words.height + root.unit * Theme.gap
            spacing: root.unit * Theme.gap

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("%1 of %2").arg(root.step + 1).arg(Guide.Step.Done)
                color: Theme.burnt
                font.family: Theme.face
                font.pixelSize: root.unit * Theme.signText * 0.8
            }
            HudButton {
                objectName: "guideNext"
                unit: root.unit
                visible: root.step === Guide.Step.Turn
                text: qsTr("Next")
                onClicked: {
                    root.step = Guide.Step.Build;
                    root.settle();
                }
            }
            HudButton {
                objectName: "guideSkip"
                unit: root.unit
                text: qsTr("Skip the guide")
                onClicked: {
                    root.game.paused = root.pausedBefore; // as if the guide had never stopped it
                    root.finish();
                }
            }
        }
    }
}
