pragma ComponentBehavior: Bound

import QtQuick

// Planks held by iron brackets, the frame of everything in the HUD. It lies over the map, a tap on
// it means nothing there.
Item {
    id: root

    required property real unit
    readonly property real bracket: unit * Theme.bracket
    readonly property real bar: Math.max(Theme.thinnestBar, unit * Theme.bracketBar)

    MouseArea {
        anchors.fill: parent
    }

    Canvas {
        anchors.fill: parent
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPaint: {
            const context = getContext("2d");
            context.reset();
            const rows = Math.max(1, Math.round(height / (root.unit * Theme.plank)));
            const plank = height / rows;
            for (let row = 0; row < rows; ++row) {
                const shade = Theme.darkestPlank + (1 - Theme.darkestPlank) * ((row * 37) % 10)
                      / 10;
                const tone = Theme.wood;
                context.fillStyle = Qt.rgba(tone.r * shade, tone.g * shade, tone.b * shade, 1);
                context.fillRect(0, row * plank, width, plank);
                context.fillStyle = Theme.woodDark;
                context.fillRect(0, (row + 1) * plank - Theme.seam, width, Theme.seam);
            }
            context.strokeStyle = Theme.woodDark;
            context.lineWidth = Theme.outline;
            const edge = Theme.outline / 2;
            context.strokeRect(edge, edge, width - Theme.outline, height - Theme.outline);
        }
    }

    Repeater {
        model: 4

        delegate: Item {
            id: corner

            required property int index
            readonly property bool atRight: index % 2 === 1
            readonly property bool atBottom: index >= 2

            x: atRight ? root.width - root.bracket : 0
            y: atBottom ? root.height - root.bracket : 0
            width: root.bracket
            height: root.bracket

            Rectangle {
                y: corner.atBottom ? parent.height - root.bar : 0
                width: parent.width
                height: root.bar
                color: Theme.iron
                border.color: Theme.ironDark
            }

            Rectangle {
                x: corner.atRight ? parent.width - root.bar : 0
                width: root.bar
                height: parent.height
                color: Theme.iron
                border.color: Theme.ironDark
            }
        }
    }
}
