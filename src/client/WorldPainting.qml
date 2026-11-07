pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Shapes

// The ground of the world map as a picture: each section is a soft patch of its landscape, the
// patches flow into each other on a shore, the ways between them are dashed.
Item {
    id: root

    required property WorldSession world

    readonly property real land: width * Theme.worldLand

    width: Theme.worldPicture
    height: Theme.worldPicture * Theme.worldDepth / Theme.worldWidth

    // Lumps around the middle of a patch, the same for every section, turned by its number.
    function lumps(index: int): list<point> {
        const turn = index * 1.7;
        const points = [Qt.point(0, 0)];
        for (let lump = 0; lump < 5; ++lump) {
            const angle = turn + lump * 2 * Math.PI / 5;
            points.push(Qt.point(Math.cos(angle) * 0.55, Math.sin(angle) * 0.45));
        }
        return points;
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.worldSea
    }

    // First the shores of all sections, then their land, so that neighbors share one coast.
    Repeater {
        model: [1.25, 1]

        Item {
            id: coast

            required property real modelData
            required property int index

            anchors.fill: parent

            Repeater {
                model: root.world.sections

                Item {
                    id: patch

                    required property var modelData
                    required property int index

                    x: modelData.x * root.width
                    y: modelData.y * root.height

                    Repeater {
                        model: root.lumps(patch.index)

                        Rectangle {
                            required property point modelData
                            required property int index

                            readonly property real size: root.land * coast.modelData * (index === 0
                                                                                        ? 2 : 1.2)

                            x: modelData.x * root.land - size / 2
                            y: modelData.y * root.land - size / 2
                            width: size
                            height: size
                            radius: size / 2
                            color: coast.index === 0 ? Theme.worldShore : patch.modelData.color
                        }
                    }
                }
            }
        }
    }

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeColor: Theme.worldWay
            strokeWidth: root.width * Theme.worldWayWidth
            strokeStyle: ShapePath.DashLine
            dashPattern: [2, 1.5]
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap

            PathMultiline {
                paths: root.world.links.map(link => [link.from, link.to].map(index => {
                    const section = root.world.sections[index];
                    return Qt.point(section.x * root.width, section.y * root.height);
                }))
            }
        }
    }
}
