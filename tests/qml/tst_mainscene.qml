pragma ComponentBehavior: Bound

import QtQuick
import QtTest
import QtowerDefense.Client

// TestCase itself is invisible: what should render or take input needs a visible parent.
Item {
    id: stage

    width: 640
    height: 360

    Component {
        id: soundsComponent

        GameSounds {}
    }

    TestCase {
        id: testCase

        name: "MainScene"
        when: windowShown

        Component {
            id: gameComponent

            Item {
                property alias scene: scene
                property alias hud: hud

                anchors.fill: parent

                MainScene {
                    id: scene

                    anchors.fill: parent
                    game: session
                }

                Hud {
                    id: hud

                    anchors.fill: parent
                    game: session
                }

                GameSession {
                    id: session

                    Component.onCompleted: startNewGame(":/content/training_ground.json")
                }
            }
        }

        function createScene() {
            return createTemporaryObject(gameComponent, stage).scene;
        }

        function createHud() {
            return createTemporaryObject(gameComponent, stage).hud;
        }

        function cleanup() {
            stage.width = 640;
            stage.height = 360;
        }

        function runUntil(game, condition) {
            for (let frame = 0; frame < 100000 && !condition(); ++frame) {
                game.advanceTime(33334);
            }
            verify(condition());
        }

        // Moves the map until the cell (4, 4) lies under this point of the view.
        function moveCellTo(scene, target) {
            for (let step = 0; step < 20; ++step) {
                wait(50); // the camera moves with the next frame
                const place = scene.cellOnScreen(4, 4);
                scene.panBy(target.x - place.x, target.y - place.y);
            }
            tryVerify(() => scene.pick(target.x, target.y).objectHit !== null, 3000,
            "no cell under the button");
        }

        function test_fitCameraDistance_data() {
            return [
                        {
                            tag: "right angle",
                            extent: 100,
                            fov: 90,
                            distance: 50
                        },
                        {
                            tag: "sixty degrees",
                            extent: 200,
                            fov: 60,
                            distance: 173.20508
                        },
                    ];
        }

        function test_fitCameraDistance(data) {
            const scene = createScene();
            verify(scene !== null);
            fuzzyCompare(scene.fitCameraDistance(data.extent, data.fov), data.distance, 0.001);
        }

        function test_cameraIsAboveTheGround() {
            const scene = createScene();
            verify(scene.camera !== null);
            verify(scene.camera.y > 0);
        }

        function test_aTapOnAFreeCellBuildsATower() {
            const scene = createScene();
            waitForRendering(scene);
            const place = scene.cellOnScreen(4, 4);
            mouseClick(scene, place.x, place.y);
            compare(scene.game.towerCount, 1);
            compare(scene.game.gold, 70);
        }

        function test_aTapBesideTheMapBuildsNothing() {
            const scene = createScene();
            waitForRendering(scene);
            mouseClick(scene, 2, 2);
            compare(scene.game.towerCount, 0);
        }

        function test_aTapOnBlockedCellsBuildsNothing_data() {
            return [
                        {
                            tag: "path",
                            x: 3,
                            y: 2
                        },
                        {
                            tag: "rock",
                            x: 2,
                            y: 5
                        },
                        {
                            tag: "water",
                            x: 2,
                            y: 9
                        },
                    ];
        }

        function test_aTapOnBlockedCellsBuildsNothing(data) {
            const scene = createScene();
            waitForRendering(scene);
            const place = scene.cellOnScreen(data.x, data.y);
            mouseClick(scene, place.x, place.y);
            compare(scene.game.towerCount, 0);
            compare(scene.game.gold, 120);
        }

        // The tap names the cell, the wide tower also takes the cell to its right.
        function test_theChosenTowerIsBuiltOnTheTappedCell() {
            const game = createTemporaryObject(gameComponent, stage);
            waitForRendering(game.scene);
            const towerButtons = findChild(game.hud, "towerButtons");
            tryVerify(() => towerButtons.children.length > 3);
            mouseClick(towerButtons.children[1]);
            compare(game.scene.game.selectedTower, 1);
            verify(towerButtons.children[1].highlighted);
            const place = game.scene.cellOnScreen(4, 4);
            mouseClick(game.scene, place.x, place.y);
            compare(game.scene.game.gold, 30);
            const cells = game.scene.game.towerCells;
            compare(cells.length, 2);
            compare(Qt.point(cells[0].x, cells[0].y), Qt.point(4, 4));
            compare(Qt.point(cells[1].x, cells[1].y), Qt.point(5, 4));
        }

        function test_aTapHitsTheCellUnderTheFinger_data() {
            return [
                        {
                            tag: "near the camera",
                            x: 12,
                            y: 10
                        },
                        {
                            tag: "far from the camera",
                            x: 6,
                            y: 0
                        },
                        {
                            tag: "left edge",
                            x: 0,
                            y: 5
                        },
                        {
                            tag: "right edge",
                            x: 16,
                            y: 5
                        },
                    ];
        }

        function test_aTapHitsTheCellUnderTheFinger(data) {
            const scene = createScene();
            waitForRendering(scene);
            const place = scene.cellOnScreen(data.x, data.y);
            mouseClick(scene, place.x, place.y);
            const cells = scene.game.towerCells;
            compare(cells.length, 1);
            compare(Qt.point(cells[0].x, cells[0].y), Qt.point(data.x, data.y));
        }

        // A high block hides the cells behind it, a tap on it means the block.
        function test_aTapOnTheTopOfAnObstacleBuildsNothing_data() {
            return [
                        {
                            tag: "tree",
                            x: 13,
                            y: 2,
                            height: 0.85
                        },
                        {
                            tag: "tree beside the path",
                            x: 14,
                            y: 3,
                            height: 0.85
                        },
                        {
                            tag: "rock",
                            x: 2,
                            y: 5,
                            height: 0.45
                        },
                    ];
        }

        function test_aTapOnTheTopOfAnObstacleBuildsNothing(data) {
            const scene = createScene();
            waitForRendering(scene);
            const place = scene.blockTopOnScreen(data.x, data.y, data.height);
            mouseClick(scene, place.x, place.y);
            compare(scene.game.towerCount, 0);
        }

        function test_aTapOnTheTopOfATowerBuildsNothing() {
            const scene = createScene();
            verify(scene.game.buildTower(12, 5));
            waitForRendering(scene);
            const place = scene.blockTopOnScreen(12, 5, 0.65);
            mouseClick(scene, place.x, place.y);
            compare(scene.game.towerCount, 1);
            compare(scene.game.gold, 70);
        }

        function test_aTapOnTheTopOfATallModelStaysOnIt() {
            const scene = createScene();
            scene.game.selectedTower = scene.game.towerModels.indexOf("venom_spire");
            verify(scene.game.buildTower(12, 5));
            waitForRendering(scene);
            const place = scene.blockTopOnScreen(12, 5, 1.5);
            mouseClick(scene, place.x, place.y);
            compare(scene.game.towerCount, 1);
            compare(scene.game.chosenPriority >= 0, true);
        }

        function test_aTreeTakesATapOnItsTop() {
            const scene = createScene();
            waitForRendering(scene);
            const place = scene.blockTopOnScreen(13, 2, 1.45);
            mouseClick(scene, place.x, place.y);
            compare(scene.game.towerCount, 0);
        }

        function test_theCellBehindATreeTakesATap() {
            const scene = createScene();
            waitForRendering(scene);
            const place = scene.blockTopOnScreen(12.7, 1, 0.04);
            mouseClick(scene, place.x, place.y);
            verify(scene.game.chooseTowerAt(13, 1));
        }

        function test_aSpellIsSeenWhereItLands_data() {
            return [
                        {
                            tag: "near",
                            x: 3,
                            y: 9
                        },
                        {
                            tag: "far",
                            x: 16,
                            y: 2
                        }
                    ];
        }

        function test_aSpellIsSeenWhereItLands(data) {
            const scene = createScene();
            waitForRendering(scene);
            const place = scene.blockTopOnScreen(data.x, data.y, 0.05);
            const ground = grabImage(scene).pixel(place.x, place.y);
            scene.game.spellCast(data.x, data.y, 2, 0);
            tryVerify(() => !Qt.colorEqual(grabImage(scene).pixel(place.x, place.y), ground), 400,
            "the spell does not show");
        }

        function test_everyCueHasASound() {
            const scene = createScene();
            const sounds = createTemporaryObject(soundsComponent, stage, {
                                                     game: scene.game
                                                 });
            failOnWarning(/./);
            for (let cue = GameSession.Build; cue <= GameSession.Lost; ++cue) {
                scene.game.cue(cue);
            }
            verify(sounds !== null);
        }

        function test_aBuiltTowerIsDrawn() {
            const scene = createScene();
            waitForRendering(scene);
            const place = scene.blockTopOnScreen(12, 5, 0.7);
            const before = grabImage(scene).pixel(place.x, place.y);
            verify(scene.game.buildTower(12, 5));
            tryVerify(() => !Qt.colorEqual(grabImage(scene).pixel(place.x, place.y), before), 3000,
            "the tower does not show");
        }

        function test_zoomStaysInItsRange() {
            const scene = createScene();
            scene.zoomBy(0.001);
            compare(scene.zoom, scene.maxZoom);
            scene.zoomBy(1000);
            compare(scene.zoom, scene.minZoom);
        }

        function test_anEnemyIsDrawnOnThePath() {
            const scene = createScene();
            // As in the game: frames without enemies come first, nothing may stand there yet.
            waitForRendering(scene);
            const empty = grabImage(scene);
            const origin = scene.enemyOnScreen(0, 0);
            runUntil(scene.game, () => scene.game.enemyCount > 0);
            scene.game.paused = true;
            const place = scene.enemyOnScreen(0, 2);
            const ground = empty.pixel(place.x, place.y);
            const sameGround = scene.enemyOnScreen(2, 0);
            verify(Qt.colorEqual(empty.pixel(origin.x, origin.y), empty.pixel(sameGround.x,
                                                                              sameGround.y)));
            tryVerify(() => !Qt.colorEqual(grabImage(scene).pixel(place.x, place.y), ground), 3000,
            "no enemy at the start of the path");
        }

        function test_theWholeGroundFitsTheView_data() {
            return [
                        {
                            tag: "16:9",
                            width: 640,
                            height: 360
                        },
                        {
                            tag: "4:3",
                            width: 640,
                            height: 480
                        },
                        {
                            tag: "small",
                            width: 480,
                            height: 320
                        },
                    ];
        }

        function test_theWholeGroundFitsTheView(data) {
            stage.width = data.width;
            stage.height = data.height;
            const scene = createScene();
            waitForRendering(scene);
            const corners = [[-1.5, -1.5], [17.5, -1.5], [-1.5, 11.5], [17.5, 11.5]];
            for (const corner of corners) {
                const place = scene.enemyOnScreen(corner[0], corner[1]);
                verify(place.x >= 0 && place.x <= scene.width, "x " + place.x);
                verify(place.y >= 0 && place.y <= scene.height, "y " + place.y);
            }
        }

        function test_theMapFollowsTheFinger() {
            const scene = createScene();
            const before = scene.camera.x;
            scene.panBy(40, 0);
            verify(scene.camera.x < before);
            scene.panBy(1000000, 0);
            compare(scene.panX, -scene.groundWidth / 2);
        }

        function test_aDragMovesTheMapAndBuildsNothing() {
            const scene = createScene();
            waitForRendering(scene);
            const place = scene.cellOnScreen(4, 4);
            mouseDrag(scene, place.x, place.y, 60, 0);
            verify(scene.panX < 0);
            compare(scene.game.towerCount, 0);
        }

        function test_theWheelZoomsByItsStrength() {
            const scene = createScene();
            waitForRendering(scene);
            mouseWheel(scene, 300, 180, 0, 120);
            fuzzyCompare(scene.zoom, 1 / 1.1, 0.0001);
            scene.zoom = 1;
            mouseWheel(scene, 300, 180, 0, 12);
            verify(scene.zoom < 1 && scene.zoom > 1 / 1.1);
            scene.zoom = 1;
            mouseWheel(scene, 300, 180, 120, 0);
            compare(scene.zoom, 1);
        }

        function test_aTouchOnACellBuildsATower() {
            const scene = createScene();
            waitForRendering(scene);
            const place = scene.cellOnScreen(4, 4);
            const touch = touchEvent(scene);
            touch.press(0, scene, place.x, place.y).commit();
            touch.release(0, scene, place.x, place.y).commit();
            compare(scene.game.towerCount, 1);
        }

        function test_aPinchZooms() {
            const scene = createScene();
            waitForRendering(scene);
            const touch = touchEvent(scene);
            touch.press(0, scene, 250, 180).press(1, scene, 350, 180).commit();
            for (let step = 1; step <= 10; ++step) {
                touch.move(0, scene, 250 - step * 8, 180).move(1, scene, 350 + step * 8, 180).commit(
                            );
                // Quick merges the touch moves of one frame. Without frames in between, a busy machine
                // sends one single move, and that only starts the pinch.
                wait(50);
            }
            touch.release(0, scene, 170, 180).release(1, scene, 430, 180).commit();
            verify(scene.zoom < 1, "zoom " + scene.zoom);
        }

        function test_playingAgainBuildsNothing() {
            const game = createTemporaryObject(gameComponent, stage);
            const session = game.scene.game;
            runUntil(session, () => session.lost);
            const button = findChild(game.hud, "restartButton");
            waitForRendering(game.scene);
            moveCellTo(game.scene, button.mapToItem(game.scene, button.width / 2, button.height
                                                    / 2));
            mouseClick(button);
            compare(session.tick, 0);
            compare(session.gold, 120);
            compare(session.towerCount, 0);
        }

        function test_theHudFitsSmallDisplays_data() {
            return [
                        {
                            tag: "window",
                            width: 480,
                            density: 3.8
                        },
                        {
                            tag: "phone",
                            width: 640,
                            density: 6.3
                        },
                        {
                            tag: "wide phone",
                            width: 800,
                            density: 6.3
                        },
                    ];
        }

        function test_theHudFitsSmallDisplays(data) {
            stage.width = data.width;
            const hud = createHud();
            hud.density = data.density;
            const stats = findChild(hud, "stats");
            const buttons = findChild(hud, "buttons");
            tryVerify(() => buttons.width > 0);
            verify(stats.x + stats.width <= buttons.x, stats.width + " and " + buttons.x);
            const towerButtons = findChild(hud, "towerButtons");
            tryVerify(() => towerButtons.children.length > 3);
            const list = findChild(hud, "towerList");
            const rotation = findChild(hud, "rotation");
            verify(stats.y + stats.height <= list.y);
            const spells = findChild(hud, "spellButtons");
            tryVerify(() => spells.children.length > 1);
            verify(spells.y + spells.height <= rotation.y, spells.y + " and " + rotation.y);
            verify(list.width >= towerButtons.children[0].width / 2, list.width);
            verify(list.x + list.width <= rotation.x);
            verify(rotation.x + rotation.width <= hud.width);
        }

        function test_aMapThatDoesNotLoadOffersNoNewTry() {
            const hud = createHud();
            hud.game.startNewGame("none.json");
            verify(findChild(hud, "endPanel").visible);
            verify(!findChild(hud, "restartButton").visible);
        }

        function test_theHudShowsTheGame() {
            const hud = createHud();
            compare(findChild(hud, "goldLabel").text, "Gold: 120");
            compare(findChild(hud, "livesLabel").text, "Lives: 5");
            hud.game.buildTower(4, 4);
            compare(findChild(hud, "goldLabel").text, "Gold: 70");
        }

        function test_theButtonPauses() {
            const hud = createHud();
            waitForRendering(hud);
            mouseClick(findChild(hud, "pauseButton"));
            verify(hud.game.paused);
            mouseClick(findChild(hud, "pauseButton"));
            verify(!hud.game.paused);
        }

        function test_theEndOfAGameOffersAnother() {
            const hud = createHud();
            verify(!findChild(hud, "endPanel").visible);
            runUntil(hud.game, () => hud.game.lost);
            verify(findChild(hud, "endPanel").visible);
            waitForRendering(hud);
            mouseClick(findChild(hud, "restartButton"));
            compare(hud.game.tick, 0);
            verify(!findChild(hud, "endPanel").visible);
        }

        function test_versionHasThreeNumbers() {
            verify(/^\d+\.\d+\.\d+$/.test(AppInfo.version), AppInfo.version);
        }
    }
}
