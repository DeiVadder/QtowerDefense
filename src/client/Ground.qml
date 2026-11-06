pragma ComponentBehavior: Bound

import QtQuick
import QtQuick3D
import QtowerDefense.Client

// The land of a map with its water and its plants, in the coordinates of the map. It takes no taps.
Node {
    id: root

    required property LandscapeView view
    required property GameSession game
    required property real fieldSize
    property real gridStrength: 0

    readonly property real middleX: (game.gridSize.width - 1) * fieldSize / 2
    readonly property real middleZ: (game.gridSize.height - 1) * fieldSize / 2
    readonly property real farReach: 4 * fieldSize // in hundreds of units, a #Rectangle has 100

    // New for each landscape: a texture that only gets new data keeps its old mipmaps.
    Repeater3D {
        model: [root.view.generation]

        delegate: Model {
            geometry: TerrainMesh {
                fieldSize: root.fieldSize
                view: root.view
            }
            castsShadows: false
            materials: PrincipledMaterial {
                roughness: 1
                metalness: 0
                baseColorMap: Texture {
                    generateMipmaps: true
                    mipFilter: Texture.Linear
                    textureData: TerrainPicture {
                        view: root.view
                    }
                }
            }
        }
    }

    // The land beyond the ground, it reaches into the fog. The camera may look past the ground.
    Model {
        source: "#Rectangle"
        visible: !root.view.lakes
        x: root.middleX
        y: -0.4 * root.fieldSize // below the water cells, the same color hides the step
        z: root.middleZ
        eulerRotation.x: -90
        scale: Qt.vector3d(root.farReach, root.farReach, 1)
        castsShadows: false
        materials: PrincipledMaterial {
            baseColor: root.view.plain
            roughness: 1
            metalness: 0
        }
    }

    // With lakes it is a sea to the horizon, else it only fills the water cells.
    Model {
        source: "#Rectangle"
        visible: root.view.hasWater
        x: root.middleX
        y: root.view.waterLevel * root.fieldSize
        z: root.middleZ
        eulerRotation.x: -90
        scale: root.view.lakes ? Qt.vector3d(root.farReach, root.farReach, 1) : Qt.vector3d(
                                     root.game.gridSize.width * root.fieldSize / 100,
                                     root.game.gridSize.height * root.fieldSize / 100, 1)
        castsShadows: false
        materials: PrincipledMaterial {
            baseColor: root.view.water
            alphaMode: PrincipledMaterial.Blend
            opacity: 0.82
            roughness: 0.25
            metalness: 0
        }
    }

    // Where towers go. It lies a little above the ground, the board is flat.
    Model {
        source: "#Rectangle"
        visible: root.gridStrength > 0
        x: root.middleX
        y: 0.5
        z: root.middleZ
        eulerRotation.x: -90
        scale: Qt.vector3d(root.game.gridSize.width * root.fieldSize / 100,
                           root.game.gridSize.height * root.fieldSize / 100, 1)
        castsShadows: false
        receivesShadows: false
        materials: PrincipledMaterial {
            alphaMode: PrincipledMaterial.Blend
            lighting: PrincipledMaterial.NoLighting
            opacity: root.gridStrength
            baseColorMap: Texture {
                tilingModeHorizontal: Texture.Repeat
                tilingModeVertical: Texture.Repeat
                scaleU: root.game.gridSize.width
                scaleV: root.game.gridSize.height
                generateMipmaps: true
                mipFilter: Texture.Linear
                textureData: GridCellPicture {}
            }
        }
    }

    Repeater3D {
        model: root.view.sceneryModels

        delegate: AssetModel {
            id: plant

            required property int index
            required property string modelData

            name: plant.modelData
            fallback: "#Cube"
            instancing: SceneryInstances {
                view: root.view
                item: plant.index
                fieldSize: root.fieldSize
            }
        }
    }
}
