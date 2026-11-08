import QtQuick
import QtowerDefense.Client

// The intro at the start and the curtain while a map loads (E-42). It falls at once and fades only
// when it lifts. While it is there it takes every tap, finger and wheel. A tap ends the intro.
Rectangle {
    id: root

    // The intro plays once, when the window opens.
    property bool intro: true
    // The curtain hangs while a new scene draws its first frames. The scene stays hidden in the
    // first ones: the frame with the curtain alone is on the screen before the scene takes its time.
    property bool loading: false
    property int framesShown: 0 // ticks since the curtain fell
    readonly property bool hidesScene: loading && framesShown < Theme.curtainAloneFrames
    readonly property real side: Math.min(width, height)

    function cover() {
        framesShown = 0;
        loading = true;
    }

    color: Theme.burnt
    opacity: 0
    visible: opacity > 0
    states: State {
        name: "covered"
        when: root.intro || root.loading

        PropertyChanges {
            root.opacity: 1
        }
    }
    transitions: Transition {
        from: "covered"
        to: ""

        NumberAnimation {
            property: "opacity"
            duration: Theme.curtainMillis
        }
    }

    Timer {
        running: root.intro
        interval: Theme.introMillis
        onTriggered: root.intro = false
    }

    // Ticks before each frame, in the thread of the interface, and keeps the frames coming.
    FrameAnimation {
        running: root.loading
        onTriggered: {
            root.framesShown += 1;
            if (root.framesShown > Theme.curtainAloneFrames + Theme.curtainFrames) {
                root.loading = false;
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: root.intro = false
        onWheel: wheel => wheel.accepted = true
    }

    // Every finger, not only the first one that the mouse area sees.
    MultiPointTouchArea {
        anchors.fill: parent
        mouseEnabled: false
        onReleased: root.intro = false
    }

    Image {
        id: emblem

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.verticalCenter
        width: root.side * Theme.emblemShare
        height: width
        source: "qrc:/assets/icon.png"
        sourceSize: Qt.size(Theme.emblemPixels, Theme.emblemPixels)
        smooth: true

        NumberAnimation on scale {
            from: Theme.emblemStart
            to: 1
            duration: Theme.emblemMillis
            easing.type: Easing.OutBack
        }
    }

    Text {
        id: title

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: emblem.bottom
        anchors.topMargin: root.side * Theme.splashNote
        text: qsTr("QtowerDefense")
        color: Theme.gold
        style: Text.Raised
        styleColor: Theme.woodDark
        font.family: Theme.face
        font.bold: true
        font.pixelSize: root.side * Theme.splashTitle
    }

    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: title.bottom
        anchors.topMargin: root.side * Theme.splashNote
        visible: root.loading && !root.intro
        text: qsTr("Loading the map…")
        color: Theme.text
        font.family: Theme.face
        font.pixelSize: root.side * Theme.splashNote
    }
}
