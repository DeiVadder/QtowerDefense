pragma ComponentBehavior: Bound

import QtQuick

// Planks held by iron brackets, the frame of everything in the HUD. It lies over the map, a tap on
// it means nothing there.
Item {
    id: root

    required property real unit
    property color tone: Theme.wood
    property int seed: 7 // every board has a grain of its own
    property real plankUnits: Theme.plank
    property bool takesTaps: true
    readonly property real bracket: unit * Theme.bracket
    readonly property real bar: Math.max(Theme.thinnestBar, unit * Theme.bracketBar)

    MouseArea {
        anchors.fill: parent
        enabled: root.takesTaps
    }

    Canvas {
        anchors.fill: parent
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPaint: {
            const context = getContext("2d");
            context.reset();
            let state = root.seed * 977 + 13;
            const random = () => {
                state = (state * 1103515245 + 12345) % 2147483648;
                return state / 2147483648;
            };
            const rows = Math.max(1, Math.round(height / (root.unit * root.plankUnits)));
            const plank = height / rows;
            for (let row = 0; row < rows; ++row) {
                const shade = Theme.darkestPlank + (1 - Theme.darkestPlank) * random();
                context.fillStyle = Qt.rgba(root.tone.r * shade, root.tone.g * shade, root.tone.b * shade,
                                            1);
                context.fillRect(0, row * plank, width, plank);
                context.strokeStyle = Theme.grain;
                context.lineWidth = Theme.grainWidth;
                for (let line = 0; line < Theme.grainLines; ++line) {
                    const y = row * plank + Theme.grainMargin + random() * (plank - 2
                                                                            * Theme.grainMargin);
                    const bend = (2 * random() - 1) * Theme.grainBend;
                    context.beginPath();
                    context.moveTo(0, y);
                    context.bezierCurveTo(width * Theme.grainFirstBend, y + bend, width * Theme.grainSecondBend,
                                          y - bend, width, y + bend / 2);
                    context.stroke();
                }
                context.fillStyle = Theme.plankEdge;
                context.fillRect(0, row * plank + Theme.grainWidth, width, Theme.seam);
                context.fillStyle = Theme.plankGap;
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

            Rectangle {
                readonly property real size: root.unit * Theme.rivet

                x: corner.atRight ? parent.width - root.bar / 2 - size / 2 : root.bar / 2 - size / 2
                y: corner.atBottom ? parent.height - root.bar / 2 - size / 2 : root.bar / 2 - size
                                     / 2

                width: size
                height: size
                radius: size / 2
                color: Theme.text
                opacity: Theme.rivetOpacity
            }
        }
    }
}
