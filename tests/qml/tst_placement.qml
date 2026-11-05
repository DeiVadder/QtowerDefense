pragma ComponentBehavior: Bound

import QtQuick
import QtTest
import QtowerDefense.Client

// How the HUD turns and offers towers. TestCase itself is invisible: what should render or take input needs a visible parent.
Item {
    id: stage

    width: 640
    height: 360

    TestCase {
        id: testCase

        name: "Placement"
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

        function cleanup() {
            stage.width = 640;
            stage.height = 360;
        }

        // A tap on a tower chooses it, the next tap elsewhere only lets go of it.
        function test_aTowerIsChosenAndToldWhereToAim() {
            const game = createTemporaryObject(gameComponent, stage);
            waitForRendering(game.scene);
            const session = game.scene.game;
            const buttons = findChild(game.hud, "priorityButtons");
            verify(!buttons.visible);
            let place = game.scene.cellOnScreen(4, 4);
            mouseClick(game.scene, place.x, place.y);
            compare(session.towerCount, 1);
            waitForRendering(game.scene);
            place = game.scene.blockTopOnScreen(4, 4, 0.7);
            mouseClick(game.scene, place.x, place.y);
            compare(session.towerCount, 1);
            compare(session.chosenPriority, 0);
            verify(buttons.visible);
            tryVerify(() => buttons.children.length > 3);
            mouseClick(buttons.children[3]);
            compare(session.chosenPriority, 3);
            verify(buttons.children[3].highlighted);
            place = game.scene.cellOnScreen(10, 6);
            mouseClick(game.scene, place.x, place.y);
            compare(session.chosenPriority, -1);
            compare(session.towerCount, 1);
            verify(!buttons.visible);
            mouseClick(game.scene, place.x, place.y);
            compare(session.towerCount, 2);
        }

        function test_theHudUpgradesAndSellsTheChosenTower() {
            const game = createTemporaryObject(gameComponent, stage);
            waitForRendering(game.scene);
            const session = game.scene.game;
            const actions = findChild(game.hud, "towerActions");
            const upgrade = findChild(game.hud, "upgradeButton");
            const sell = findChild(game.hud, "sellButton");
            verify(!actions.visible);
            verify(session.buildTower(4, 4));
            waitForRendering(game.scene);
            const place = game.scene.blockTopOnScreen(4, 4, 0.7);
            mouseClick(game.scene, place.x, place.y);
            verify(actions.visible);
            tryVerify(() => upgrade.x > sell.x + sell.width - 1);
            compare(sell.x, 0); // it stays where it is when the upgrade goes away
            verify(upgrade.text.indexOf("40") >= 0);
            mouseClick(upgrade);
            compare(session.gold, 30);
            compare(session.towerCount, 1);
            verify(upgrade.text.indexOf("70") >= 0);
            mouseClick(sell);
            compare(session.gold, 93);
            compare(session.towerCount, 0);
            verify(!actions.visible);
        }

        // The tap names the tower by the instance it hits, so each tower needs its own answer.
        function test_aTapChoosesTheTowerItHits() {
            const game = createTemporaryObject(gameComponent, stage);
            waitForRendering(game.scene);
            const session = game.scene.game;
            verify(session.buildTower(4, 4));
            verify(session.buildTower(10, 6));
            waitForRendering(game.scene);
            let place = game.scene.blockTopOnScreen(4, 4, 0.7);
            mouseClick(game.scene, place.x, place.y);
            session.setChosenPriority(3);
            session.clearChosenTower();
            place = game.scene.blockTopOnScreen(10, 6, 0.7);
            mouseClick(game.scene, place.x, place.y);
            compare(session.chosenPriority, 0);
            session.clearChosenTower();
            place = game.scene.blockTopOnScreen(4, 4, 0.7);
            mouseClick(game.scene, place.x, place.y);
            compare(session.chosenPriority, 3);
        }

        // A quarter turn clockwise: the wide tower now reaches down on the map.
        function test_theRotateButtonTurnsWhatATapBuilds() {
            const game = createTemporaryObject(gameComponent, stage);
            waitForRendering(game.scene);
            const towerButtons = findChild(game.hud, "towerButtons");
            tryVerify(() => towerButtons.children.length > 3);
            mouseClick(towerButtons.children[1]);
            mouseClick(findChild(game.hud, "rotateButton"));
            const place = game.scene.cellOnScreen(4, 4);
            mouseClick(game.scene, place.x, place.y);
            const cells = game.scene.game.towerCells;
            compare(cells.length, 2);
            compare(Qt.point(cells[1].x, cells[1].y), Qt.point(4, 5));
        }

        // The list scrolls, it must not take a second row of the map.
        function test_theTowerListStaysOneRow() {
            const game = createTemporaryObject(gameComponent, stage);
            waitForRendering(game.scene);
            const list = findChild(game.hud, "towerList");
            const towerButtons = findChild(game.hud, "towerButtons");
            tryVerify(() => towerButtons.children.length > 3);
            verify(list.contentWidth > list.width);
            compare(towerButtons.height, list.height);
            const rotation = findChild(game.hud, "rotation");
            verify(list.x + list.width <= rotation.x);
        }

        function test_aSwipeOnAButtonScrollsTheList_data() {
            return [
                        {
                            tag: "mouse",
                            touch: false
                        },
                        {
                            tag: "touch",
                            touch: true
                        },
                    ];
        }

        function test_aSwipeOnAButtonScrollsTheList(data) {
            const game = createTemporaryObject(gameComponent, stage);
            waitForRendering(game.scene);
            const list = findChild(game.hud, "towerList");
            const towerButtons = findChild(game.hud, "towerButtons");
            tryVerify(() => towerButtons.children.length > 3);
            const y = list.y + list.height / 2;
            const startX = list.x + towerButtons.children[1].x + 20;
            const touch = touchEvent(game.hud);
            if (data.touch) {
                touch.press(0, game.hud, startX, y).commit();
            } else {
                mousePress(game.hud, startX, y);
            }
            for (let step = 1; step <= 10; ++step) {
                wait(20); // Quick merges moves within a frame
                if (data.touch) {
                    touch.move(0, game.hud, startX - step * 15, y).commit();
                } else {
                    mouseMove(game.hud, startX - step * 15, y);
                }
            }
            verify(list.contentX > 0, list.contentX);
            compare(game.scene.panX, 0);
            compare(game.scene.panZ, 0);
            if (data.touch) {
                touch.release(0, game.hud, startX - 150, y).commit();
            } else {
                mouseRelease(game.hud, startX - 150, y);
            }
            compare(game.scene.game.selectedTower, 0);
        }

        function test_aDragOnAButtonLeavesTheMap() {
            const game = createTemporaryObject(gameComponent, stage);
            waitForRendering(game.scene);
            const button = findChild(game.hud, "rotateButton");
            mousePress(button, 10, 10);
            for (let step = 1; step <= 10; ++step) {
                wait(20);
                mouseMove(button, 10, 10 - step * 15);
            }
            mouseRelease(button, 10, -140);
            compare(game.scene.panZ, 0);
        }

        // Under the shape there may be a cell, a tap on the shape must not build there.
        function test_aTapOnTheShapeBuildsNothing() {
            const game = createTemporaryObject(gameComponent, stage);
            waitForRendering(game.scene);
            const shape = findChild(game.hud, "shape");
            const middle = shape.mapToItem(game.scene, shape.width / 2, shape.height / 2);
            game.scene.zoom = game.scene.minZoom;
            for (let step = 0; step < 5; ++step) { // perspective: a few steps bring the cell there
                const place = game.scene.cellOnScreen(12, 5);
                game.scene.panBy(middle.x - place.x, middle.y - place.y);
            }
            waitForRendering(game.scene);
            const width = game.scene.game.gridSize.width;
            compare(game.scene.pick(middle.x, middle.y).instanceIndex, 5 * width + 12);
            mouseClick(shape);
            compare(game.scene.game.towerCount, 0);
        }

        // The wide tower turned once: the dark cell on top, the other below it.
        function test_theShapeStandsAsTheTowerWill() {
            const game = createTemporaryObject(gameComponent, stage);
            waitForRendering(game.scene);
            const towerButtons = findChild(game.hud, "towerButtons");
            tryVerify(() => towerButtons.children.length > 3);
            mouseClick(towerButtons.children[1]);
            mouseClick(findChild(game.hud, "rotateButton"));
            const shape = findChild(game.hud, "shape");
            tryCompare(shape, "extent", 2);
            const cells = Array.from(shape.children).filter(child => child.modelData !== undefined);
            compare(cells.length, 2);
            const anchor = cells.find(cell => cell.modelData.anchor);
            const other = cells.find(cell => !cell.modelData.anchor);
            compare(Qt.point(anchor.x, anchor.y), Qt.point(0, 0));
            compare(Qt.point(other.x, other.y), Qt.point(0, shape.cellSize));
            verify(!Qt.colorEqual(anchor.color, other.color));
        }

        function test_theShapeShowsEveryCell() {
            const game = createTemporaryObject(gameComponent, stage);
            waitForRendering(game.scene);
            const towerButtons = findChild(game.hud, "towerButtons");
            tryVerify(() => towerButtons.children.length > 3);
            const shape = findChild(game.hud, "shape");
            game.scene.game.selectedTower = 3;
            tryCompare(shape, "extent", 3);
            compare(Array.from(shape.children).filter(child => child.modelData !== undefined).length,
                    5);
        }
    }
}
