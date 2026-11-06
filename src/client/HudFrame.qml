import QtQuick

// The frame behind a group of the HUD. It comes and goes with the group.
FramedPanel {
    required property Item around

    anchors.fill: around
    anchors.margins: -unit * Theme.inset
    visible: around.visible && around.width > 0
}
