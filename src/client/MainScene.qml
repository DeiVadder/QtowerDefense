import QtQuick
import QtQuick3D

View3D {
    id: root

    readonly property real groundExtent: 600
    readonly property real fieldOfView: 60

    // Distance at which a camera with the given vertical field of view sees the whole extent.
    function fitCameraDistance(extent: real, fieldOfViewDegrees: real): real {
        return extent / 2 / Math.tan(fieldOfViewDegrees * Math.PI / 360);
    }

    camera: sceneCamera

    environment: SceneEnvironment {
        backgroundMode: SceneEnvironment.Color
        clearColor: "#1d2733"
    }

    PerspectiveCamera {
        id: sceneCamera

        fieldOfView: root.fieldOfView
        eulerRotation.x: -35
        y: 0.57 * root.fitCameraDistance(root.groundExtent, root.fieldOfView)
        z: 0.82 * root.fitCameraDistance(root.groundExtent, root.fieldOfView)
    }

    DirectionalLight {
        eulerRotation.x: -50
        eulerRotation.y: -30
    }

    Model {
        source: "#Rectangle"
        eulerRotation.x: -90
        scale: Qt.vector3d(root.groundExtent / 100, root.groundExtent / 100, 1)
        materials: PrincipledMaterial {
            baseColor: "#4f7942"
        }
    }

    Model {
        source: "#Cube"
        y: 50
        materials: PrincipledMaterial {
            baseColor: "#c9a227"
        }
    }
}
