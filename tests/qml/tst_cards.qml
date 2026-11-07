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
            property Item cards: null

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
        name: "Cards"
        when: windowShown

        // One card of each of the first towers, in that order.
        function deal(game) {
            for (let type = 0; type < game.scene.game.handLimit; ++type) {
                verify(game.scene.game.buyCard(type));
            }
        }

        function create() {
            const game = createTemporaryObject(gameComponent, stage);
            waitForRendering(game.scene);
            deal(game);
            const cards = findChild(game.hud, "towerButtons");
            tryVerify(() => cards.children.length > 3);
            game.cards = cards;
            return game;
        }

        function cardsOf(cards) {
            return cards.children.filter(child => child.info !== undefined);
        }

        function dragCard(game, card, target, data) {
            const start = card.mapToItem(game.hud, card.width / 2, 10);
            const touch = touchEvent(game.hud);
            if (data.touch) {
                touch.press(0, game.hud, start.x, start.y).commit();
            } else {
                mousePress(game.hud, start.x, start.y);
            }
            for (let step = 1; step <= 10; ++step) {
                wait(20); // Quick merges moves within a frame
                const x = start.x + (target.x - start.x) * step / 10;
                const y = start.y + (target.y - start.y) * step / 10;
                if (data.touch) {
                    touch.move(0, game.hud, x, y).commit();
                } else {
                    mouseMove(game.hud, x, y);
                }
            }
            return () => {
                if (data.touch) {
                    touch.release(0, game.hud, target.x, target.y).commit();
                } else {
                    mouseRelease(game.hud, target.x, target.y);
                }
            };
        }

        function test_theCardsFitBesideTheRotation_data() {
            return [
                        {
                            tag: "small",
                            width: 480,
                            height: 320
                        },
                        {
                            tag: "wide",
                            width: 1280,
                            height: 720
                        }
                    ];
        }

        function test_theCardsFitBesideTheRotation(data) {
            stage.width = data.width;
            stage.height = data.height;
            const game = create();
            const hand = findChild(game.hud, "towerList");
            const rotation = findChild(game.hud, "rotation");
            verify(hand.x + hand.width <= rotation.x, hand.width);
            const buttons = findChild(game.hud, "cardButtons");
            verify(hand.x + hand.width <= buttons.x, hand.width + " and " + buttons.x);
            verify(hand.step > 0);
            stage.width = 640;
            stage.height = 360;
        }

        function test_aCardShowsWhatItsTowerCan() {
            const game = create();
            const card = cardsOf(game.cards)[1];
            const type = game.scene.game.towerTypes[1];
            compare(findChild(card, "cardCost").text, String(type.cost));
            const shown = name => Number(findChild(findChild(card, name), "value").text);
            verify(shown("card_sword") > 0);
            verify(Math.abs(shown("card_sword") - type.damage) <= 0.05);
            verify(Math.abs(shown("card_target") - type.range) <= 0.05);
            verify(Math.abs(shown("card_fast") - type.rate) <= 0.05);
        }

        function test_theChosenCardRises() {
            const game = create();
            const cards = cardsOf(game.cards);
            mouseClick(cards[2]);
            compare(game.scene.game.selectedTower, 2);
            tryVerify(() => cards[2].y < cards[1].y);
        }

        function test_aChosenTowerPutsTheHandDown() {
            const game = create();
            const session = game.scene.game;
            const cards = cardsOf(game.cards);
            verify(session.buildTower(4, 4));
            verify(session.chooseTowerAt(4, 4));
            const card = cards[session.selectedTower];
            tryVerify(() => card.scale === 1 && !card.raised);
            const rows = findChild(game.hud, "priorityButtons");
            tryVerify(() => rows.y + rows.height <= findChild(game.hud, "towerList").y);
        }

        function test_aCardDraggedOntoTheMapBuilds_data() {
            return [
                        {
                            tag: "mouse",
                            touch: false
                        },
                        {
                            tag: "touch",
                            touch: true
                        }
                    ];
        }

        function test_aCardDraggedOntoTheMapBuilds(data) {
            const game = create();
            const session = game.scene.game;
            const card = cardsOf(game.cards)[2];
            const place = game.scene.cellOnScreen(4, 4);
            const release = dragCard(game, card, Qt.point(place.x, place.y), data);
            compare(session.selectedTower, 2);
            verify(findChild(game.hud, "heldCard").visible);
            compare(card.opacity, 0);
            compare(session.towerCount, 0);
            compare(game.scene.panX, 0);
            compare(game.scene.panZ, 0);
            release();
            compare(session.towerCount, 1);
            compare(Qt.point(session.towerCells[0].x, session.towerCells[0].y), Qt.point(4, 4));
            verify(!findChild(game.hud, "heldCard").visible);
            compare(session.hand.indexOf(2), -1);
            tryVerify(() => cardsOf(game.cards).length === session.handLimit - 1);
        }

        function test_theGridShowsDuringADrag() {
            const game = create();
            compare(game.scene.gridStrength, Theme.gridFaint);
            const place = game.scene.cellOnScreen(4, 4);
            const release = dragCard(game, cardsOf(game.cards)[0], Qt.point(place.x, place.y), {
                                         touch: false
                                     });
            compare(game.scene.gridStrength, Theme.gridHeld);
            release();
            compare(game.scene.gridStrength, Theme.gridFaint);
        }

        function test_aNewGameEndsADrag() {
            const game = create();
            const session = game.scene.game;
            const place = game.scene.cellOnScreen(4, 4);
            const release = dragCard(game, cardsOf(game.cards)[2], Qt.point(place.x, place.y), {
                                         touch: false
                                     });
            session.restart();
            verify(!findChild(game.hud, "heldCard").visible);
            compare(game.scene.gridStrength, Theme.gridFaint);
            release();
            compare(session.towerCount, 0);
        }

        function test_aLostFingerEndsADrag() {
            const game = create();
            const hand = findChild(game.hud, "towerList");
            const place = game.scene.cellOnScreen(4, 4);
            const release = dragCard(game, cardsOf(game.cards)[2], Qt.point(place.x, place.y), {
                                         touch: false
                                     });
            verify(hand.dragging);
            hand.visible = false;
            verify(!hand.dragging);
            release();
            compare(game.scene.game.towerCount, 0);
        }

        function test_aSecondFingerTakesNoCard() {
            const game = create();
            const session = game.scene.game;
            const cards = cardsOf(game.cards);
            const first = cards[2].mapToItem(game.hud, cards[2].width / 2, 10);
            const second = cards[4].mapToItem(game.hud, cards[4].width / 2, 10);
            const place = game.scene.cellOnScreen(4, 4);
            const touch = touchEvent(game.hud);
            touch.press(0, game.hud, first.x, first.y).commit();
            wait(20);
            for (let step = 1; step <= 5; ++step) {
                wait(20);
                touch.move(0, game.hud, first.x, first.y - step * 8).commit();
            }
            const hand = findChild(game.hud, "towerList");
            compare(hand.owner, 2);
            touch.stationary(0).press(1, game.hud, second.x, second.y).commit();
            for (let step = 1; step <= 10; ++step) {
                wait(20); // Quick merges moves within a frame
                touch.move(0, game.hud, first.x + (place.x - first.x) * step / 10, first.y + (
                               place.y - first.y) * step / 10);
                touch.move(1, game.hud, second.x, second.y - step * 8).commit();
            }
            compare(hand.owner, 2);
            touch.stationary(0).release(1, game.hud, second.x, second.y - 80).commit();
            compare(game.scene.panX, 0);
            compare(game.scene.panZ, 0);
            compare(session.towerCount, 0);
            touch.release(0, game.hud, place.x, place.y).commit();
            compare(session.towerCount, 1);
            compare(session.selectedTower, 2);
            compare(Qt.point(session.towerCells[0].x, session.towerCells[0].y), Qt.point(4, 4));
        }

        function test_aTapDuringADragBuildsNothing() {
            const game = create();
            const session = game.scene.game;
            const cards = cardsOf(game.cards);
            const start = cards[2].mapToItem(game.hud, cards[2].width / 2, 10);
            const free = game.scene.cellOnScreen(10, 6);
            const touch = touchEvent(game.hud);
            touch.press(0, game.hud, start.x, start.y).commit();
            for (let step = 1; step <= 5; ++step) {
                wait(20);
                touch.move(0, game.hud, start.x, start.y - step * 8).commit();
            }
            verify(game.scene.cardInHand);
            touch.stationary(0).press(1, game.hud, free.x, free.y).commit();
            wait(20);
            touch.stationary(0).release(1, game.hud, free.x, free.y).commit();
            wait(20);
            compare(session.towerCount, 0);
            touch.release(0, game.hud, start.x, start.y - 40).commit();
        }

        function test_aMissLeavesAChosenTowerChosen() {
            const game = create();
            const session = game.scene.game;
            verify(session.buildTower(4, 4));
            verify(session.chooseTowerAt(4, 4));
            verify(!game.scene.dropCardAt(330, 8));
            verify(session.chosenPriority >= 0);
        }

        function test_aCardFliesToItsTower() {
            const game = create();
            verify(game.scene.game.buildTower(4, 4));
            verify(findChild(game.hud, "flyingCard").visible);
        }

        // Beside the hand, over a cell that a drop on the map would build on.
        function coveredCell(game) {
            const hand = findChild(game.hud, "towerList");
            const size = game.scene.game.gridSize;
            for (let y = 0; y < size.height; ++y) {
                for (let x = 0; x < size.width; ++x) {
                    const place = game.scene.cellOnScreen(x, y);
                    const over = game.hud.childAt(place.x, place.y);
                    if (over !== null && over !== hand && game.scene.game.buildTower(x, y)) {
                        game.scene.game.restart();
                        deal(game);
                        return Qt.point(place.x, place.y);
                    }
                }
            }
            return null;
        }

        function test_aCardDroppedOnTheHudBuildsNothing() {
            stage.width = 480;
            const game = create();
            waitForRendering(game.scene);
            const over = coveredCell(game);
            verify(over !== null);
            tryVerify(() => cardsOf(game.cards).length > 3);
            dragCard(game, cardsOf(game.cards)[0], over, {
                         touch: false
                     })();
            compare(game.scene.game.towerCount, 0);
            stage.width = 640;
        }

        function test_aCardDroppedBesideTheMapBuildsNothing() {
            const game = create();
            const release = dragCard(game, cardsOf(game.cards)[0], Qt.point(330, 8), {
                                         touch: false
                                     });
            release();
            compare(game.scene.game.towerCount, 0);
            verify(!findChild(game.hud, "flyingCard").visible);
        }

        function test_aDropLetsGoOfAChosenTower() {
            const game = create();
            const session = game.scene.game;
            verify(session.buildTower(4, 4));
            verify(session.chooseTowerAt(4, 4));
            const place = game.scene.cellOnScreen(6, 4);
            dragCard(game, cardsOf(game.cards)[0], Qt.point(place.x, place.y), {
                         touch: false
                     })();
            compare(session.towerCount, 2);
            compare(session.chosenPriority, -1);
        }
    }
}
