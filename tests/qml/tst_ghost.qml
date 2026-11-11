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

        function test_overTheHudThereIsNoGhost() {
            const game = create();
            const ghost = findChild(game.scene, "ghost");
            verify(game.scene.game.buyCard(0));
            game.scene.game.chooseCard(0);
            const spot = game.scene.cellOnScreen(4, 4);
            game.scene.showGhostAt(spot.x, spot.y, false);
            verify(!ghost.visible);
        }
    }
}
