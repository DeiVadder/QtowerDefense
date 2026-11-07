import QtQuick
import QtQuick.Shapes

import "IconPaths.js" as IconPaths

// One icon of IconPaths.js.
Shape {
    id: root

    required property string name
    property color fill: Theme.text

    preferredRendererType: Shape.CurveRenderer

    ShapePath {
        scale: Qt.size(root.width / 64, root.height / 64)
        fillColor: root.fill
        strokeColor: Theme.ironDark
        strokeWidth: root.width * Theme.iconLine
        fillRule: ShapePath.OddEvenFill

        PathSvg {
            path: IconPaths.pathOf(root.name)
        }
    }
}
