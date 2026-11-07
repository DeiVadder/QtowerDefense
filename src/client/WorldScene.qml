pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Shapes
import QtQuick3D

// The land of the world map in 3D (E-32): painted sections on a sea, the models of each landscape
// on them, layers of fog over every section that is not open. Taller models rise out of the fog.
View3D {
    id: root

    required property WorldSession world

    // The place of a section on the ground of the scene.
    function placeOf(section: var, height: real): vector3d {
        return Qt.vector3d((section.x - 0.5) * Theme.worldWidth, height, (section.y - 0.5)
                           * Theme.worldDepth);
    }

    environment: SceneEnvironment {
        clearColor: Theme.worldSea
        backgroundMode: SceneEnvironment.Color
        antialiasingMode: SceneEnvironment.MSAA
        antialiasingQuality: SceneEnvironment.High
    }

    camera: eye

    PerspectiveCamera {
        id: eye

        position: Qt.vector3d(0, 11.5, 7.6)
        eulerRotation.x: -57
        fieldOfView: 40
        clipNear: 1
        clipFar: 60
    }

    DirectionalLight {
        eulerRotation: Qt.vector3d(-50, -30, 0)
        brightness: 1.1
        ambientColor: Theme.worldAmbient
    }

    // The sea, the shores, the land and the ways, painted once into one picture.
    Model {
        source: "#Rectangle"
        eulerRotation.x: -90
        scale: Qt.vector3d(Theme.worldWidth / 100, Theme.worldDepth / 100, 1)
        materials: PrincipledMaterial {
            lighting: PrincipledMaterial.NoLighting // the sea of the picture meets the clear color
            baseColorMap: Texture {
                sourceItem: WorldPainting {
                    world: root.world
                }
            }
            roughness: 1
            metalness: 0
        }
    }

    Repeater3D {
        model: root.world.sections

        Node {
            id: section

            required property var modelData
            required property int index

            position: root.placeOf(modelData, 0)

            Repeater3D {
                model: section.modelData.landmarks

                AssetModel {
                    required property var modelData
                    required property int index

                    name: modelData.model
                    fallback: "#Cone"
                    // The first in the middle and tall, the others beside it.
                    position: Theme.worldLandmarkPlaces[index]
                    scale: Qt.vector3d(1, 1, 1).times(modelData.size * Theme.worldLandmark * size)
                    eulerRotation.y: 25 + index * 70
                }
            }

            // Three layers of cloud: the land vanishes, only the tops of tall models show.
            Repeater3D {
                model: section.modelData.open ? 0 : 3

                Model {
                    required property int index

                    objectName: "fog"
                    source: "#Rectangle"
                    y: Theme.worldFogTop * (index + 1) / 3
                    eulerRotation: Qt.vector3d(-90, index * 40, 0)
                    scale: Qt.vector3d(1, 1, 1).times(Theme.worldFogSize * (1.1 - index * 0.1)
                                                      / 100)
                    opacity: Theme.worldFogOpacity
                    materials: PrincipledMaterial {
                        lighting: PrincipledMaterial.NoLighting
                        baseColorMap: fogPicture
                        alphaMode: PrincipledMaterial.Blend
                        depthDrawMode: Material.NeverDepthDraw
                    }
                }
            }
        }
    }

    // One soft cloud, white in the middle and gone at its edge.
    Texture {
        id: fogPicture

        sourceItem: Shape {
            width: 256
            height: 256

            ShapePath {
                strokeWidth: -1
                fillGradient: RadialGradient {
                    centerX: 128
                    centerY: 128
                    centerRadius: 128
                    focalX: 128
                    focalY: 128

                    GradientStop {
                        position: 0
                        color: Theme.worldFog
                    }
                    GradientStop {
                        position: 0.55
                        color: Qt.alpha(Theme.worldFog, Theme.worldFogMiddle)
                    }
                    GradientStop {
                        position: 1
                        color: Qt.alpha(Theme.worldFog, 0)
                    }
                }

                PathAngleArc {
                    centerX: 128
                    centerY: 128
                    radiusX: 128
                    radiusY: 128
                    sweepAngle: 360
                }
            }
        }
    }
}
