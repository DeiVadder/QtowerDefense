pragma ComponentBehavior: Bound

import QtQuick
import QtQuick3D
import QtowerDefense.Client

View3D {
    id: root

    required property GameSession game

    readonly property real fieldSize: 50

    property real coinUnit: Theme.smallestUnit // the HUD knows how large a finger is
    property alias quality: looks.quality
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
    // Faintly in view for taps, the HUD makes it clear during a drag (E-34).
    property real gridStrength: Theme.gridFaint
    // Other fingers must not touch the map then: a finger on a card would pass through to it.
    property bool cardInHand: false
    // The cell under the finger or the mouse while a card is chosen: there the ghost of its tower
    // stands. Nothing while no card is chosen or the pointer is off the map.
    property var ghostCell: undefined
    property real zoom: 1
    property real panX: 0
    property real panZ: 0

    // The shots of one shape. Fire, ice, poison and magic glow, wood and stone take the light.
    component ShotModel: Model {
        id: shotModel

        required property int shot
        required property GameSession game
        required property real fieldSize
        property bool lit: false
        property bool blended: false

        instanceRoot: shotModel
        instancing: MapInstances {
            game: shotModel.game
            layer: MapInstances.Shots
            shot: shotModel.shot
            fieldSize: shotModel.fieldSize
        }
        materials: PrincipledMaterial {
            baseColor: "white"
            lighting: shotModel.lit ? PrincipledMaterial.FragmentLighting :
                                      PrincipledMaterial.NoLighting
            alphaMode: shotModel.blended ? PrincipledMaterial.Blend : PrincipledMaterial.Default
        }
    }

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

    // Towers and trees reach higher than their blocks, so their models take taps.
    function lookOf(hit: var): AssetModel {
        return hit.objectHit as AssetModel;
    }

    function isTower(hit: var): bool {
        const look = lookOf(hit);
        return hit.objectHit === towers || (look !== null && look.tower);
    }

    // The cell under a hit of the map, nothing beside it.
    function cellOf(hit: var): var {
        if (lookOf(hit) !== null) {
            const place = map.mapPositionFromScene(hit.scenePosition);
            return Qt.point(Math.round(place.x / fieldSize), Math.round(place.z / fieldSize));
        }
        if (hit.objectHit === towers) {
            return game.towerCells[hit.instanceIndex];
        }
        if (hit.objectHit !== board) {
            return undefined;
        }
        const width = game.gridSize.width;
        return Qt.point(hit.instanceIndex % width, Math.floor(hit.instanceIndex / width));
    }

    // Where a card lands. It means to build, whatever was chosen before.
    function dropCardAt(x: real, y: real): bool {
        const cell = cellOf(pick(x, y));
        if (game.chosenCard < 0 || cell === undefined || !game.buildTower(cell.x, cell.y)) {
            return false;
        }
        game.clearChosenTower();
        return true;
    }

    // Shows where the chosen card would stand. Off the map, or over the HUD, it shows nothing.
    function showGhostAt(x: real, y: real, onMap: bool) {
        ghostCell = onMap && game.chosenCard >= 0 ? cellOf(pick(x, y)) : undefined;
    }

    function tapAt(x: real, y: real): bool {
        const hit = pick(x, y);
        if (game.selectedSpell >= 0) {
            const target = cellOf(hit);
            return target !== undefined && game.castSpellAt(target.x, target.y);
        }
        if (isTower(hit)) {
            const cell = cellOf(hit);
            // Right after a restart, the picture may still show towers of the last game.
            return cell !== undefined && game.chooseTowerAt(cell.x, cell.y);
        }
        // With a tower chosen, a tap elsewhere only lets go of it: nothing gets built by accident.
        if (game.chosenPriority >= 0) {
            game.clearChosenTower();
            return false;
        }
        const cell = cellOf(hit);
        return game.chosenCard >= 0 && cell !== undefined && game.buildTower(cell.x, cell.y);
    }

    camera: sceneCamera

    environment: SceneEnvironment {
        objectName: "sceneEnvironment"
        backgroundMode: SceneEnvironment.Color
        antialiasingMode: looks.antialiasingMode
        antialiasingQuality: looks.antialiasingQuality
        clearColor: landscape.sky
        fog: Fog {
            enabled: true
            color: landscape.sky
            depthEnabled: true
            depthNear: 1.25 * root.zoom * root.fitCameraDistance(root.groundExtent,
                                                                 root.fieldOfView)
            depthFar: 3 * root.zoom * root.fitCameraDistance(root.groundExtent, root.fieldOfView)
        }
    }

    RenderQuality {
        id: looks
    }

    LandscapeView {
        id: landscape

        game: root.game
    }

    DragHandler {
        enabled: !root.cardInHand
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
        enabled: !root.cardInHand
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

    onCardInHandChanged: {
        if (!cardInHand) {
            ghostCell = undefined;
        }
    }

    // A mouse shows the ghost before the tap, a finger only while it drags a card.
    HoverHandler {
        enabled: root.game.chosenCard >= 0 && !root.cardInHand
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        onPointChanged: root.showGhostAt(point.position.x, point.position.y, hovered)
        onHoveredChanged: {
            if (!hovered) {
                root.ghostCell = undefined;
            }
        }
        onEnabledChanged: root.ghostCell = undefined
    }

    TapHandler {
        enabled: !root.cardInHand
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
        objectName: "sun"
        eulerRotation.x: -50
        eulerRotation.y: -30
        ambientColor: "#505a60" // the far side of a model stays visible
        castsShadow: looks.shadows
        shadowFactor: 55
        shadowMapQuality: looks.shadowMapQuality
        softShadowQuality: looks.softShadowQuality
        shadowMapFar: 2500
    }

    Node {
        id: map

        x: -(root.game.gridSize.width - 1) * root.fieldSize / 2
        z: -(root.game.gridSize.height - 1) * root.fieldSize / 2

        Ground {
            view: landscape
            game: root.game
            fieldSize: root.fieldSize
            gridStrength: root.gridStrength
        }

        Model {
            id: board

            source: "#Cube"
            castsShadows: false
            receivesShadows: false
            pickable: true // the block of a cell takes the taps, its instance names the cell
            instanceRoot: board
            instancing: MapInstances {
                game: root.game
                layer: MapInstances.Board
                fieldSize: root.fieldSize
            }
            materials: PrincipledMaterial {
                baseColor: "white"
                alphaMode: PrincipledMaterial.Blend // only takes taps, Ground shows the terrain
            }
        }

        Model {
            id: towers

            source: "#Cube"
            castsShadows: false
            receivesShadows: false
            pickable: true // a tap on a tower must not reach the cell behind it
            instanceRoot: towers
            instancing: MapInstances {
                game: root.game
                layer: MapInstances.Towers
                fieldSize: root.fieldSize
            }
            materials: PrincipledMaterial {
                baseColor: "white"
                alphaMode: PrincipledMaterial.Blend // only takes taps, TowerLooks shows the towers
            }
        }

        Repeater3D {
            model: root.game.towerModels

            delegate: AssetModel {
                id: towerLook

                required property int index
                required property string modelData

                name: towerLook.modelData
                fallback: "#Cube"
                tower: true
                pickable: true
                y: found ? 0 : root.fieldSize / 2 // a plain cube has its middle at its place
                instancing: MapInstances {
                    game: root.game
                    layer: MapInstances.TowerLooks
                    typeIndex: towerLook.index
                    fieldSize: root.fieldSize
                    modelSize: towerLook.size
                }
            }
        }

        Repeater3D {
            model: root.game.enemyModels

            delegate: Node {
                id: enemyType

                required property int index
                required property string modelData
                readonly property int frames: root.game.enemyFrames[index] ?? 0

                // A model with walk frames: one instanced model for each frame.
                Repeater3D {
                    model: enemyType.frames

                    delegate: AssetModel {
                        id: walkLook

                        required property int index

                        name: enemyType.modelData + "_" + walkLook.index
                        texture: "monsters"
                        fallback: "#Sphere"
                        instancing: MapInstances {
                            game: root.game
                            layer: MapInstances.Enemies
                            typeIndex: enemyType.index
                            walkFrame: walkLook.index
                            fieldSize: root.fieldSize
                            enemyScale: root.fieldSize * 1.1
                        }
                    }
                }

                AssetModel {
                    id: enemyLook

                    visible: enemyType.frames === 0
                    name: enemyType.frames === 0 ? enemyType.modelData : ""
                    fallback: "#Sphere"
                    y: enemyType.modelData.length > 0 ? 0 : root.enemyHeight
                    instancing: MapInstances {
                        game: enemyType.frames === 0 ? root.game : null // the frames draw them
                        layer: MapInstances.Enemies
                        typeIndex: enemyType.index
                        fieldSize: root.fieldSize
                        enemyScale: root.fieldSize * (enemyType.modelData.length > 0 ? 1.1 : 1
                                                                                       / 120)

                    }
                }
            }
        }

        // Every tower type has a shot of its own (E-52): bolts, a fireball, a volley of arrows, a
        // shard of ice, a glob of poison and a boulder with dust where it lands.
        ShotModel {
            shot: MapInstances.Bolt
            source: "#Cylinder"
            game: root.game
            fieldSize: root.fieldSize
            lit: true
        }
        ShotModel {
            shot: MapInstances.Fireball
            source: "#Sphere"
            game: root.game
            fieldSize: root.fieldSize
        }
        ShotModel {
            shot: MapInstances.Arrow
            source: "#Cylinder"
            game: root.game
            fieldSize: root.fieldSize
            lit: true
        }
        ShotModel {
            shot: MapInstances.Shard
            source: "#Cone"
            game: root.game
            fieldSize: root.fieldSize
        }
        ShotModel {
            shot: MapInstances.Glob
            source: "#Sphere"
            game: root.game
            fieldSize: root.fieldSize
        }
        ShotModel {
            shot: MapInstances.Boulder
            source: "#Sphere"
            game: root.game
            fieldSize: root.fieldSize
            lit: true
        }
        ShotModel {
            shot: MapInstances.Dust
            source: "#Cylinder"
            game: root.game
            fieldSize: root.fieldSize
            blended: true
        }

        // The ghost of the chosen tower (E-54): its model on each of its cells, see-through, over
        // tiles that are green where it can be built and red where not.
        Node {
            id: ghost

            readonly property bool shown: root.ghostCell !== undefined && root.game.chosenCard >= 0
            readonly property var anchorCell: root.game.selectedShape.find(cell => cell.anchor)
                                              ?? Qt.point(0, 0)
            readonly property bool fits: shown && root.game.canBuildAt(root.ghostCell.x,
                                                                       root.ghostCell.y)

            objectName: "ghost"
            visible: shown
            x: shown ? root.ghostCell.x * root.fieldSize : 0
            z: shown ? root.ghostCell.y * root.fieldSize : 0

            Repeater3D {
                model: root.game.selectedShape

                delegate: Node {
                    id: ghostCell

                    required property var modelData

                    x: (modelData.x - ghost.anchorCell.x) * root.fieldSize
                    z: (modelData.y - ghost.anchorCell.y) * root.fieldSize

                    Model {
                        source: "#Rectangle"
                        y: 1.5
                        eulerRotation.x: -90
                        scale: Qt.vector3d(0.0095 * root.fieldSize, 0.0095 * root.fieldSize, 1)
                        castsShadows: false
                        receivesShadows: false
                        materials: PrincipledMaterial {
                            baseColor: ghost.fits ? "#702cde85" : "#80ff5a4a"
                            lighting: PrincipledMaterial.NoLighting
                            alphaMode: PrincipledMaterial.Blend
                        }
                    }

                    AssetModel {
                        name: root.game.towerModels[root.game.selectedTower] ?? ""
                        fallback: "#Cube"
                        opacity: ghost.fits ? 0.6 : 0.35
                        castsShadows: false
                        y: found ? 0 : root.fieldSize / 2
                        scale: Qt.vector3d(1, 1, 1).times(0.9 * root.fieldSize / size)
                    }
                }
            }
        }

        // A spell where it lands: it swells to its radius and fades. It has no say in the game.
        Model {
            id: blast

            source: "#Sphere"
            visible: blastAnimation.running
            materials: PrincipledMaterial {
                id: blastMaterial

                alphaMode: PrincipledMaterial.Blend
                lighting: PrincipledMaterial.NoLighting
            }

            ParallelAnimation {
                id: blastAnimation

                NumberAnimation {
                    id: blastGrowth

                    target: blast
                    properties: "scale.x,scale.y,scale.z"
                    from: 0
                    duration: 450
                    easing.type: Easing.OutQuad
                }
                NumberAnimation {
                    target: blastMaterial
                    property: "opacity"
                    from: 0.7
                    to: 0
                    duration: 450
                }
            }
        }

        Connections {
            target: root.game

            function onSpellCast(x: int, y: int, radius: real, spell: int) {
                blast.x = x * root.fieldSize;
                blast.z = y * root.fieldSize;
                blastMaterial.baseColor = Theme.spellColor(spell);
                blastGrowth.to = 2 * radius * root.fieldSize / 100;
                blastAnimation.restart();
            }
        }

        AssetModel {
            name: landscape.rockModel
            fallback: "#Cube"
            instancing: MapInstances {
                game: root.game
                layer: MapInstances.Rocks
                fieldSize: root.fieldSize
                modelSize: 0.42
            }
        }

        AssetModel {
            name: landscape.treeModel
            fallback: "#Cube"
            pickable: true
            instancing: MapInstances {
                game: root.game
                layer: MapInstances.Trees
                fieldSize: root.fieldSize
                modelSize: 1
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

    // Over the map, under the HUD: the HUD asks for its children under a dropped card.
    CoinLayer {
        objectName: "coins"
        anchors.fill: parent
        game: root.game
        scene: root
        unit: root.coinUnit
    }

    // The middle of the top of a block on a cell, its height in shares of a field.
    function blockTopOnScreen(cellX: real, cellY: real, height: real): vector3d {
        return mapFrom3DScene(map.mapPositionToScene(Qt.vector3d(cellX * fieldSize, height
                                                                 * fieldSize, cellY * fieldSize)));
    }
}
