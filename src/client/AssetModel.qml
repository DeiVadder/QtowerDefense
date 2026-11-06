import QtQuick
import QtQuick3D

// A model of the resources with its texture, or a plain shape if there is none. Instances bring
// their places and tint it.
Model {
    id: root

    required property string name
    required property string fallback

    readonly property bool found: name.length > 0
    readonly property real size: found ? 1 : 100 // the edge of the shape in its own units
    property bool tower: false

    source: found ? "qrc:/assets/" + name + ".mesh" : fallback
    instanceRoot: root // without it, instances ignore the place of the map
    materials: PrincipledMaterial {
        baseColorMap: root.found ? texture : null
        roughness: 0.9
        metalness: 0
    }

    Texture {
        id: texture

        source: root.found ? "qrc:/assets/" + root.name + ".png" : ""
    }
}
