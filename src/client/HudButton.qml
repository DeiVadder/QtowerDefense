import QtQuick

// A button for mouse and touch. Plain Quick, the project has no Controls module.
Rectangle {
    id: root

    required property string text
    required property real unit
    property bool highlighted: false
    // Not enabled: a disabled MouseArea lets the tap through to the map below.
    property bool available: true

    signal clicked

    implicitWidth: Math.max(unit * 2, label.implicitWidth + unit)
    implicitHeight: unit
    radius: unit / 6
    color: highlighted ? "#c9a227" : "#2f3e4e"
    opacity: available ? 1 : 0.5
    border.color: "#8fa3b5"

    Text {
        id: label

        anchors.centerIn: parent
        text: root.text
        color: root.highlighted ? "#1d2733" : "#f0f4f8"
        font.pixelSize: root.unit * 0.42
    }

    // Not a TapHandler: it only watches, and the map below would take the same click.
    MouseArea {
        anchors.fill: parent
        onClicked: {
            if (root.available) {
                root.clicked();
            }
        }
    }
}
