pragma ComponentBehavior: Bound

import QtQuick
import QtQuick3D
import QtowerDefense.Client

View3D {
    id: root

    required property GameSession game

    readonly property real fieldSize: 50
    readonly property real fieldOfView: 60
    readonly property real groundWidth: (game.gridSize.width + 2) * fieldSize
    readonly property real groundDepth: (game.gridSize.height + 2) * fieldSize
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
        const hit = pick(x, y);
        if (hit.objectHit === towers) {
            const cell = game.towerCells[hit.instanceIndex];
            // Right after a restart, the picture may still show towers of the last game.
            return cell !== undefined && game.chooseTowerAt(cell.x, cell.y);
        }
        // With a tower chosen, a tap elsewhere only lets go of it: nothing gets built by accident.
        if (game.chosenPriority >= 0) {
            game.clearChosenTower();
            return false;
        }
        if (hit.objectHit !== board) {
            return false;
        }
        const width = game.gridSize.width;
        return game.buildTower(hit.instanceIndex % width, Math.floor(hit.instanceIndex / width));
    }

    camera: sceneCamera

    environment: SceneEnvironment {
        backgroundMode: SceneEnvironment.Color
        clearColor: "#1d2733"
    }

    DragHandler {
        property point last: Qt.point(0, 0)

        target: null
        // Handlers see every press, even on the HUD. A drag that began there is not for the map.
        grabPermissions: PointerHandler.CanTakeOverFromHandlersOfSameType
                         | PointerHandler.CanTakeOverFromHandlersOfDifferentType
                         | PointerHandler.ApprovesTakeOverByAnything
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
            baseColor: "#33502c"
        }
    }

    Node {
        id: map

        x: -(root.game.gridSize.width - 1) * root.fieldSize / 2
        z: -(root.game.gridSize.height - 1) * root.fieldSize / 2

        Model {
            id: board

            source: "#Cube"
            pickable: true // the block of a cell takes the taps, its instance names the cell
            instanceRoot: board
            instancing: MapInstances {
                game: root.game
                layer: MapInstances.Board
                fieldSize: root.fieldSize
            }
            materials: PrincipledMaterial {
                baseColor: "white"
            }
        }

        Model {
            id: towers

            source: "#Cube"
            pickable: true // a tap on a tower must not reach the cell behind it
            instanceRoot: towers
            instancing: MapInstances {
                game: root.game
                layer: MapInstances.Towers
                fieldSize: root.fieldSize
            }
            materials: PrincipledMaterial {
                baseColor: "white"
            }
        }

        Model {
            id: enemies

            source: "#Sphere"
            y: root.enemyHeight
            instanceRoot: enemies // without it, instances ignore the place of the map
            instancing: MapInstances {
                game: root.game
                layer: MapInstances.Enemies
                fieldSize: root.fieldSize
                enemyScale: root.fieldSize / 120
            }
            materials: PrincipledMaterial {
                baseColor: "white" // the instances bring their colour, it shows status effects
            }
        }
    }

    // The place of an enemy at this map position on the screen.
    function enemyOnScreen(mapX: real, mapY: real): vector3d {
        return mapFrom3DScene(map.mapPositionToScene(Qt.vector3d(mapX * fieldSize, enemyHeight,
                                                                 mapY * fieldSize)));
    }

    // The middle of a cell on the screen.
    function cellOnScreen(cellX: int, cellY: int): vector3d {
        return blockTopOnScreen(cellX, cellY, 0);
    }

    // The middle of the top of a block on a cell, its height in shares of a field.
    function blockTopOnScreen(cellX: int, cellY: int, height: real): vector3d {
        return mapFrom3DScene(map.mapPositionToScene(Qt.vector3d(cellX * fieldSize, height
                                                                 * fieldSize, cellY * fieldSize)));
    }
}
