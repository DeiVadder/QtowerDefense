pragma ComponentBehavior: Bound

import QtQuick

// The end of a game: victory, defeat or the error of a map that could not be loaded. An endless
// game shows its rounds and the best of its map (E-36).
FramedPanel {
    id: panel

    required property GameSession game
    property bool failed: false
    property int best: 0

    signal leave

    width: Math.min(parent.width - panel.unit, Math.max(panel.unit * Theme.endWidth, message.implicitWidth
                                                        + panel.unit))
    height: panel.unit * (Theme.endHeight + (rounds.visible ? Theme.endRoundsHeight : 0))

    Text {
        id: message

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: panel.unit * Theme.endMargin
        width: parent.width - panel.unit
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        text: panel.failed ? panel.game.error : panel.game.won ? qsTr("Victory") : qsTr("Defeat")
        color: Theme.text
        font.family: Theme.face
        font.bold: true
        font.pixelSize: panel.unit * Theme.titleText
    }

    // The score of an endless game: the rounds come through and the best of the map (E-36).
    Text {
        id: rounds

        objectName: "roundsLabel"
        visible: panel.game.endless && !panel.failed
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: message.bottom
        anchors.topMargin: panel.unit * Theme.iconGap
        width: parent.width - panel.unit
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        text: panel.game.round > panel.best ? qsTr("%n round(s) survived, your best yet", "",
                                                   panel.game.round) : qsTr(
                                                  "%n round(s) survived, best: %1", "",
                                                  panel.game.round).arg(panel.best)
        color: Theme.gold
        font.family: Theme.face
        font.bold: true
        font.pixelSize: panel.unit * Theme.bodyText
        Accessible.role: Accessible.StaticText
        Accessible.name: text
    }

    Row {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: panel.unit * Theme.endMargin
        spacing: panel.unit * Theme.gap

        HudButton {
            objectName: "restartButton"
            visible: !panel.failed
            unit: panel.unit
            text: qsTr("Play again")
            onClicked: panel.game.restart()
        }

        HudButton {
            objectName: "worldButton"
            unit: panel.unit
            text: qsTr("World map")
            onClicked: panel.leave()
        }
    }
}
