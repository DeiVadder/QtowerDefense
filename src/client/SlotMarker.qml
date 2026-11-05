import QtQuick
import QtQuick3D

// A place to build on. It is pickable, a tap on it asks for a tower.
Model {
    id: root

    required property int index
    required property var modelData
    required property real fieldSize
    property bool affordable: false

    source: "#Cylinder"
    pickable: true
    x: modelData.x * fieldSize
    z: modelData.y * fieldSize
    y: 2
    scale: Qt.vector3d(fieldSize / 60, 0.04, fieldSize / 60)
    materials: PrincipledMaterial {
        baseColor: root.affordable ? "#d8c58a" : "#6f6a5c"
    }
}
