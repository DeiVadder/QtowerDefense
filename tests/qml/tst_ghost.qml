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
        id: gameComponent

        Item {
            property alias scene: gameScene
            property alias hud: hud

            anchors.fill: parent

            MainScene {
                id: gameScene

                anchors.fill: parent
                game: session
            }

            Hud {
                id: hud

                anchors.fill: parent
                game: session
                scene: gameScene
            }

            GameSession {
                id: session

                Component.onCompleted: startNewGame(richMap) // qmllint disable unqualified
            }
        }
    }

    TestCase {
        name: "Ghost"
        when: windowShown

        function create() {
            const game = createTemporaryObject(gameComponent, stage);
            waitForRendering(game.scene);
            return game;
        }

        function pointAt(game, cellX, cellY) {
            const spot = game.scene.cellOnScreen(cellX, cellY);
            game.scene.showGhostAt(spot.x, spot.y, true);
        }

        function test_aChosenCardShowsWhereItWouldStand() {
            const game = create();
            const ghost = findChild(game.scene, "ghost");
            pointAt(game, 4, 4);
            verify(!ghost.visible); // no card chosen
            verify(game.scene.game.buyCard(0));
            game.scene.game.chooseCard(0);
            pointAt(game, 4, 4);
            verify(ghost.visible);
            verify(ghost.fits);
            compare(ghost.x, 4 * game.scene.fieldSize);
            pointAt(game, 8, 4); // on the path
            verify(ghost.visible);
            verify(!ghost.fits);
        }

        function test_theGhostGoesWithTheCard() {
            const game = create();
            const ghost = findChild(game.scene, "ghost");
            verify(game.scene.game.buyCard(0));
            game.scene.game.chooseCard(0);
            pointAt(game, 4, 4);
            verify(ghost.visible);
            verify(game.scene.game.buildTower(4, 4));
            verify(!ghost.visible); // the card is gone
        }

        // The mouse over a button of the HUD shows no ghost on the map behind it.
        function test_overTheHudThereIsNoGhost() {
            const game = create();
            const ghost = findChild(game.scene, "ghost");
            verify(game.scene.game.buyCard(0));
            game.scene.game.chooseCard(0);
            const spot = game.scene.cellOnScreen(4, 4);
            mouseMove(game.hud, spot.x, spot.y);
            tryVerify(() => ghost.visible);
            const roll = findChild(game.hud, "rollButton");
            const over = roll.mapToItem(game.hud, roll.width / 2, roll.height / 2);
            mouseMove(game.hud, over.x, over.y);
            tryVerify(() => !ghost.visible);
        }

        // A HUD that keeps clear of a notch (E-55) still means the cell under the pointer.
        function test_aHudBesideANotchMeansTheCellUnderIt() {
            const game = create();
            const ghost = findChild(game.scene, "ghost");
            const notch = 60;
            game.hud.anchors.leftMargin = notch;
            verify(game.scene.game.buyCard(0));
            game.scene.game.chooseCard(0);
            // A free cell that no control of the HUD covers.
            let cell = null;
            for (let x = 2; x < 10 && !cell; ++x) {
                const there = game.scene.cellIn(game.hud, x, 5);
                if (game.scene.game.canBuildAt(x, 5) && !game.hud.childAt(there.x, there.y)) {
                    cell = Qt.point(x, 5);
                }
            }
            verify(cell);
            const spot = game.scene.cellOnScreen(cell.x, cell.y);
            const inHud = game.scene.cellIn(game.hud, cell.x, cell.y);
            compare(inHud.x, spot.x - notch);
            mouseMove(game.hud, inHud.x, inHud.y);
            tryVerify(() => ghost.visible);
            compare(ghost.x, cell.x * game.scene.fieldSize);
            findChild(game.hud, "towerList").dropped(inHud.x, inHud.y);
            compare(game.scene.game.towerCount, 1);
            // The card flies to the tower, not a notch beside it.
            const flyer = findChild(game.hud, "flyingCard");
            tryVerify(() => !flyer.visible);
            fuzzyCompare(flyer.x + flyer.width / 2, inHud.x, 1);
            verify(!game.scene.game.canBuildAt(cell.x, cell.y)); // there, not a notch to the left
        }

        // Its colour follows a turn without the pointer moving.
        function test_theColourFollowsATurn() {
            const game = create();
            const ghost = findChild(game.scene, "ghost");
            verify(game.scene.game.buyCard(1)); // two cells side by side
            game.scene.game.chooseCard(0);
            pointAt(game, 0, 1);
            const before = ghost.fits;
            compare(before, game.scene.game.canBuildAt(0, 1));
            for (let turn = 0; turn < 3 && ghost.fits === before; ++turn) {
                game.scene.game.rotateSelection();
                compare(ghost.fits, game.scene.game.canBuildAt(0, 1));
            }
            verify(ghost.fits !== before);
        }
    }
}
