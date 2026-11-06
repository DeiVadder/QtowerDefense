import QtQuick

// An iron plate for mouse and touch. Plain Quick, the project has no Controls module. With an icon,
// the text only says what the button means, for screen readers. It is seen without one.
Rectangle {
    id: root

    required property string text
    required property real unit
    property string icon: ""
    property string caption: "" // a short value beside the icon
    property color captionColor: Theme.text
    property bool highlighted: false
    // Not enabled: a disabled MouseArea lets the tap through to the map below.
    property bool available: true

    readonly property color shown: highlighted ? Theme.accent : Theme.text

    signal clicked

    function press() {
        if (available) {
            clicked();
        }
    }

    implicitWidth: Math.max(unit, content.implicitWidth + unit * Theme.buttonPadding)
    implicitHeight: unit
    radius: unit * Theme.corner
    opacity: available ? 1 : Theme.unavailable
    border.color: highlighted ? Theme.accent : Theme.ironDark
    border.width: highlighted ? Math.max(Theme.thinnestChosenBorder, unit * Theme.chosenBorder) : 1
    gradient: Gradient {
        GradientStop {
            position: 0
            color: Theme.iron
        }
        GradientStop {
            position: 1
            color: Theme.ironDark
        }
    }

    Accessible.role: Accessible.Button
    Accessible.name: text
    Accessible.onPressAction: press()

    Row {
        id: content

        anchors.centerIn: parent
        spacing: root.unit * Theme.iconGap

        GameIcon {
            anchors.verticalCenter: parent.verticalCenter
            visible: root.icon.length > 0
            width: root.unit * Theme.iconSize
            height: width
            name: root.icon
            fill: root.shown
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            visible: text.length > 0
            text: root.icon.length > 0 ? root.caption : root.text
            color: root.highlighted ? root.shown : root.captionColor
            font.family: Theme.face
            font.bold: true
            font.pixelSize: root.unit * Theme.bodyText
        }
    }

    // Not a TapHandler: it only watches, and the map below would take the same click.
    MouseArea {
        anchors.fill: parent
        onClicked: root.press()
    }
}
