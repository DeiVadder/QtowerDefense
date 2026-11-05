pragma ComponentBehavior: Bound

import QtQuick
import QtQuick3D
import QtowerDefense.Client

View3D {
    id: root

    required property GameSession game

    readonly property real fieldSize: 50
    readonly property real fieldOfView: 60
    readonly property real groundWidth: (game.mapSize.width + 3) * fieldSize
    readonly property real groundDepth: (game.mapSize.height + 3) * fieldSize
    // What the camera has to see from top to bottom. The factors leave room for the tilt: the near
    // edge of the ground looks wider than the far one.
    readonly property real groundExtent: Math.max(1.3 * groundDepth, 1.6 * groundWidth * viewHeight
                                                  / viewWidth)
    readonly property real viewWidth: Math.max(width, 1)
    readonly property real viewHeight: Math.max(height, 1)
    readonly property real enemyHeight: 15
    readonly property real minZoom: 0.4
    readonly property real maxZoom: 1.6
    property real zoom: 1
    property real panX: 0
    property real panZ: 0

    // Distance at which a camera with the given vertical field of view sees the whole extent.
    function fitCameraDistance(extent: real, fieldOfViewDegrees: real): real {
        return extent / 2 / Math.tan(fieldOfViewDegrees * Math.PI / 360);
    }

    function zoomBy(factor: real) {
        zoom = Math.min(maxZoom, Math.max(minZoom, zoom / factor));
    }

    // Moves the map with the finger: pixels on the screen to scene units on the ground.
    function panBy(pixelsX: real, pixelsY: real) {
        const unitsPerPixel = groundExtent * zoom / viewHeight;
        // The camera keeps looking at the ground, so that the map cannot get lost.
        const limitX = groundWidth / 2;
        const limitZ = groundDepth / 2;
        panX = Math.min(limitX, Math.max(-limitX, panX - pixelsX * unitsPerPixel));
        panZ = Math.min(limitZ, Math.max(-limitZ, panZ - pixelsY * unitsPerPixel));
    }

    function tapAt(x: real, y: real): bool {
        const slot = pick(x, y).objectHit as SlotMarker;
        return slot !== null && game.buildTower(slot.index);
    }

    camera: sceneCamera

    environment: SceneEnvironment {
        backgroundMode: SceneEnvironment.Color
        clearColor: "#1d2733"
    }

    DragHandler {
        property point last: Qt.point(0, 0)

        target: null
        onActiveChanged: last = Qt.point(0, 0)
        onActiveTranslationChanged: {
            root.panBy(activeTranslation.x - last.x, activeTranslation.y - last.y);
            last = Qt.point(activeTranslation.x, activeTranslation.y);
        }
    }

    PinchHandler {
        property real last: 1

        target: null
        onActiveChanged: last = 1
        onActiveScaleChanged: {
            root.zoomBy(activeScale / last);
            last = activeScale;
        }
    }

    WheelHandler {
        target: null
        // 120 is one notch of a wheel, touchpads send many small steps.
        onWheel: event => root.zoomBy(Math.pow(1.1, event.angleDelta.y / 120))
    }

    TapHandler {
        onTapped: eventPoint => root.tapAt(eventPoint.position.x, eventPoint.position.y)
    }

    PerspectiveCamera {
        id: sceneCamera

        fieldOfView: root.fieldOfView
        eulerRotation.x: -50
        x: root.panX
        y: 0.77 * root.zoom * root.fitCameraDistance(root.groundExtent, root.fieldOfView)
        z: root.panZ + 0.64 * root.zoom * root.fitCameraDistance(root.groundExtent,
                                                                 root.fieldOfView)
    }

    DirectionalLight {
        eulerRotation.x: -50
        eulerRotation.y: -30
    }

    Model {
        source: "#Rectangle"
        eulerRotation.x: -90
        scale: Qt.vector3d(root.groundWidth / 100, root.groundDepth / 100, 1)
        materials: PrincipledMaterial {
            baseColor: "#4f7942"
        }
    }

    Node {
        id: map

        x: -root.game.mapSize.width * root.fieldSize / 2
        z: -root.game.mapSize.height * root.fieldSize / 2

        Repeater3D {
            model: Math.max(root.game.path.length - 1, 0)

            delegate: Model {
                id: pathPiece

                required property int index
                readonly property point from: root.game.path[index]
                readonly property point to: root.game.path[index + 1]
                readonly property real length: Math.hypot(to.x - from.x, to.y - from.y)

                source: "#Cube"
                x: (from.x + to.x) / 2 * root.fieldSize
                z: (from.y + to.y) / 2 * root.fieldSize
                y: 1
                eulerRotation.y: -Math.atan2(to.y - from.y, to.x - from.x) * 180 / Math.PI
                scale: Qt.vector3d((length + 0.8) * root.fieldSize / 100, 0.02, 0.8 * root.fieldSize
                                   / 100)
                materials: PrincipledMaterial {
                    baseColor: "#8a6f4d"
                }
            }
        }

        Repeater3D {
            id: slots

            model: root.game.buildSlots

            delegate: SlotMarker {
                fieldSize: root.fieldSize
                affordable: root.game.gold >= root.game.towerCost
            }
        }

        Repeater3D {
            model: root.game.towerPlaces

            delegate: Model {
                required property var modelData

                source: "#Cone"
                x: modelData.x * root.fieldSize
                z: modelData.y * root.fieldSize
                y: 8
                scale: Qt.vector3d(root.fieldSize / 80, root.fieldSize / 60, root.fieldSize / 80)
                materials: PrincipledMaterial {
                    baseColor: "#c9a227"
                }
            }
        }

        Model {
            id: enemies

            source: "#Sphere"
            y: root.enemyHeight
            instanceRoot: enemies // without it, instances ignore the place of the map
            instancing: EnemyInstances {
                game: root.game
                fieldSize: root.fieldSize
                enemyScale: root.fieldSize / 120
            }
            materials: PrincipledMaterial {
                baseColor: "#b5372e"
            }
        }
    }

    // The place of an enemy at this map position on the screen.
    function enemyOnScreen(mapX: real, mapY: real): vector3d {
        return mapFrom3DScene(map.mapPositionToScene(Qt.vector3d(mapX * fieldSize, enemyHeight,
                                                                 mapY * fieldSize)));
    }

    // The place of a build slot on the screen.
    function slotOnScreen(slot: int): vector3d {
        return mapFrom3DScene((slots.objectAt(slot) as SlotMarker).scenePosition);
    }
}
