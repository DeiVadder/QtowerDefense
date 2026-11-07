pragma ComponentBehavior: Bound

import QtQuick

// A light sign on two chains with the words of the hero. A new line drops in and swings out.
// Without a line it is not seen. It takes no taps.
ChainSign {
    id: root

    property string line: ""
    property alias wordsName: words.objectName

    readonly property real padding: unit * Theme.gap

    visible: line.length > 0
    width: unit * Theme.bubbleWidth
    height: words.height + 2 * padding + chain
    tone: Theme.signWood

    onLineChanged: {
        if (line.length > 0) {
            swing();
        }
    }

    Text {
        id: words

        x: root.padding
        y: root.padding
        width: root.width - 2 * root.padding
        wrapMode: Text.Wrap
        text: root.line
        color: Theme.burnt
        font.family: Theme.face
        font.bold: true
        font.pixelSize: root.unit * Theme.signText
        Accessible.role: Accessible.StaticText
        Accessible.name: text
    }
}
