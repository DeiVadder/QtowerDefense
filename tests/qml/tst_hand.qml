import QtQuick
import QtTest
import QtowerDefense.Client

// The hand as a player sees it: the dice, the shop, the bin and the coins on the map.
Item {
    id: stage

    width: 640
    height: 360

    Component {
        id: gameComponent

        Item {
            property alias scene: gameScene
            property alias hud: hud
            property alias session: session

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
        name: "Hand"
        when: windowShown

        function create() {
            const game = createTemporaryObject(gameComponent, stage);
            waitForRendering(game.scene);
            return game;
        }

        function cards(game) {
            return findChild(game.hud, "towerButtons").children.filter(child => child.info
                                                                                !== undefined);
        }

        function press(game, name) {
            const button = findChild(game.hud, name);
            mouseClick(button);
        }

        function test_aGameStartsWithAnEmptyHand() {
            const game = create();
            compare(game.session.hand.length, 0);
            compare(cards(game).length, 0);
            compare(game.session.chosenCard, -1);
        }

        function test_aTapOnTheMapWithoutACardBuildsNothing() {
            const game = create();
            const place = game.scene.cellOnScreen(4, 4);
            mouseClick(game.scene, place.x, place.y);
            compare(game.session.towerCount, 0);
        }

        function test_aRolledCardShowsWhenTheDiceStop() {
            const game = create();
            const gold = game.session.gold;
            const dice = findChild(game.hud, "dice");
            press(game, "rollButton");
            compare(game.session.gold, gold - game.session.rollCost);
            compare(game.session.hand.length, 1);
            verify(dice.rolling);
            compare(cards(game).length, 0);
            tryVerify(() => !dice.rolling, 5000);
            tryVerify(() => cards(game).length === 1);
            compare(cards(game)[0].info, game.session.towerTypes[game.session.hand[0]]);
        }

        function test_theDiceRollOnlyOnce() {
            const game = create();
            const dice = findChild(game.hud, "dice");
            press(game, "rollButton");
            press(game, "rollButton");
            compare(game.session.hand.length, 1);
            tryVerify(() => !dice.rolling, 5000);
        }

        function test_theRollButtonNamesItsPrice() {
            const game = create();
            const button = findChild(game.hud, "rollButton");
            compare(button.caption, String(game.session.rollCost));
            verify(game.session.rollCost < game.session.towerTypes[0].cost);
        }

        function test_aPlacedCardCostsNothingMore() {
            const game = create();
            verify(game.session.buyCard(2));
            const gold = game.session.gold;
            tryVerify(() => cards(game).length === 1);
            mouseClick(cards(game)[0]);
            compare(game.session.chosenCard, 0);
            const place = game.scene.cellOnScreen(4, 4);
            mouseClick(game.scene, place.x, place.y);
            compare(game.session.towerCount, 1);
            compare(game.session.gold, gold);
            compare(game.session.hand.length, 0);
            compare(game.session.chosenCard, -1);
        }

        function test_theSameTypeCountsFromTheFirstCard() {
            const game = create();
            verify(game.session.buyCard(1));
            verify(game.session.buyCard(0));
            verify(game.session.buyCard(1));
            game.session.chooseCard(2);
            compare(game.session.chosenCard, 0);
        }

        function test_theShopSellsTheCardYouWant() {
            const game = create();
            const shop = findChild(game.hud, "shop");
            verify(!shop.visible);
            press(game, "shopButton");
            verify(shop.visible);
            const row = findChild(shop, "shopCards");
            tryVerify(() => row.children.length >= game.session.towerTypes.length);
            const gold = game.session.gold;
            mouseClick(row.children[3]);
            compare(game.session.hand.length, 1);
            compare(game.session.hand[0], 3);
            compare(game.session.gold, gold - game.session.towerTypes[3].cost);
            verify(!shop.visible);
        }

        function test_theShopClosesWithoutASale() {
            const game = create();
            const shop = findChild(game.hud, "shop");
            press(game, "shopButton");
            mouseClick(findChild(shop, "shopClose"));
            verify(!shop.visible);
            press(game, "shopButton");
            mouseClick(shop, 2, 2);
            verify(!shop.visible);
            compare(game.session.hand.length, 0);
        }

        function test_theShopFitsASmallWindow() {
            stage.width = 480;
            stage.height = 320;
            const game = create();
            press(game, "shopButton");
            const board = findChild(game.hud, "shopBoard");
            verify(board.x >= 0 && board.x + board.width <= game.hud.width, board.width);
            verify(board.y >= 0 && board.y + board.height <= game.hud.height, board.height);
            stage.width = 640;
            stage.height = 360;
        }

        function test_theBinNeedsAChosenCard() {
            const game = create();
            verify(game.session.buyCard(0));
            verify(!game.session.discardCard());
            const button = findChild(game.hud, "discardButton");
            verify(!button.available);
            game.session.chooseCard(0);
            verify(button.available);
            const gold = game.session.gold;
            mouseClick(button);
            compare(game.session.hand.length, 0);
            compare(game.session.gold, gold);
            compare(game.session.chosenCard, -1);
        }

        function test_aFullHandIsRefused() {
            const game = create();
            for (let card = 0; card < game.session.handLimit; ++card) {
                verify(game.session.buyCard(0));
            }
            verify(findChild(game.hud, "discardButton").highlighted);
            const gold = game.session.gold;
            verify(!game.session.buyCard(0));
            verify(!game.session.rollCard());
            compare(game.session.gold, gold);
        }

        function test_aNewGameTakesTheHandAway() {
            const game = create();
            verify(game.session.buyCard(0));
            game.session.chooseCard(0);
            game.session.restart();
            compare(game.session.hand.length, 0);
            compare(game.session.chosenCard, -1);
            tryVerify(() => cards(game).length === 0);
        }

        function test_aNewGameStopsTheDice() {
            const game = create();
            const dice = findChild(game.hud, "dice");
            press(game, "rollButton");
            verify(dice.rolling);
            game.session.restart();
            verify(!dice.rolling);
            compare(game.session.hand.length, 0);
        }

        function test_aNewGameClosesTheShop() {
            const game = create();
            const shop = findChild(game.hud, "shop");
            press(game, "shopButton");
            verify(shop.visible);
            game.session.restart();
            verify(!shop.visible);
        }

        function test_noShopWhileTheDiceRoll() {
            const game = create();
            press(game, "rollButton");
            press(game, "shopButton");
            verify(!findChild(game.hud, "shop").visible);
            tryVerify(() => !findChild(game.hud, "dice").rolling, 5000);
        }

        // Another finger may roll while one drags: the card stays under the first.
        function test_theDiceLeaveADragAlone() {
            const game = create();
            verify(game.session.buyCard(0));
            tryVerify(() => cards(game).length === 1);
            const hand = findChild(game.hud, "towerList");
            const card = cards(game)[0];
            const start = card.mapToItem(game.hud, card.width / 2, card.height / 2);
            const place = game.scene.cellOnScreen(4, 4);
            mousePress(game.hud, start.x, start.y);
            mouseMove(game.hud, start.x, start.y - 40);
            mouseMove(game.hud, place.x, place.y);
            verify(hand.dragging);
            verify(game.session.rollCard());
            tryVerify(() => !findChild(game.hud, "dice").rolling, 5000);
            tryVerify(() => cards(game).length === 2);
            verify(hand.dragging);
            compare(cards(game)[0], card);
            mouseRelease(game.hud, place.x, place.y);
            compare(game.session.towerCount, 1);
            verify(!hand.dragging);
        }

        function test_aDiscardEndsADragOfThatCard_data() {
            return [
                        {
                            tag: "alone",
                            hand: [0]
                        },
                        {
                            tag: "another type behind it",
                            hand: [0, 2]
                        },
                        {
                            tag: "the same type behind it",
                            hand: [0, 0]
                        }
                    ];
        }

        // The finger stays down and goes on: it holds nothing any more.
        function test_aDiscardEndsADragOfThatCard(data) {
            const game = create();
            data.hand.forEach(type => verify(game.session.buyCard(type)));
            tryVerify(() => cards(game).length === data.hand.length);
            const hand = findChild(game.hud, "towerList");
            const card = cards(game)[0];
            const start = card.mapToItem(game.hud, card.width / 2, card.height / 2);
            const place = game.scene.cellOnScreen(4, 4);
            mousePress(game.hud, start.x, start.y);
            mouseMove(game.hud, start.x, start.y - 40);
            mouseMove(game.hud, start.x, start.y - 80);
            verify(hand.dragging);
            verify(game.session.discardCard());
            verify(!hand.dragging);
            verify(cards(game).every(left => left.opacity === 1));
            mouseMove(game.hud, place.x, place.y);
            verify(!hand.dragging);
            mouseRelease(game.hud, place.x, place.y);
            compare(game.session.towerCount, 0);
            compare(game.session.hand.length, data.hand.length - 1);
        }

        function test_aSavedGameBringsItsHandBack() {
            const game = create();
            game.session.saveFile = saveFolder + "/hand.json"; // qmllint disable unqualified
            verify(game.session.buyCard(2));
            verify(game.session.saveGame());
            game.session.restart();
            compare(game.session.hand.length, 0);
            verify(game.session.loadGame());
            compare(game.session.hand.length, 1);
            compare(game.session.hand[0], 2);
            tryVerify(() => cards(game).length === 1);
        }

        function test_aNewGameTakesTheCoinsAway() {
            const game = create();
            const coin = dropCoin(game);
            const id = coin.coinId;
            const layer = findChild(game.scene, "coins");
            game.session.restart();
            tryVerify(() => layer.coinNamed(id) === null);
        }

        // Runs the game until an enemy has fallen to a tower beside the path.
        function dropCoin(game) {
            const layer = findChild(game.scene, "coins");
            const coins = () => layer.children.filter(child => child.coinId !== undefined);
            for (let x = 0; x < 16; ++x) {
                for (let y = 0; y < 9; ++y) {
                    game.session.buildTower(x, y);
                }
            }
            game.session.callWave();
            for (let frame = 0; frame < 2000 && coins().length === 0; ++frame) {
                game.session.advanceTime(33333);
            }
            verify(coins().length > 0);
            return coins()[0];
        }

        function test_aCoinPaysItsTap() {
            const game = create();
            const coin = dropCoin(game);
            game.session.paused = true;
            const value = coin.value;
            verify(value > 0);
            const gold = game.session.gold;
            tryVerify(() => coin.hop === 0);
            mouseClick(coin);
            compare(game.session.gold, gold + value);
            verify(!game.session.collectCoin(coin.coinId));
        }

        function test_aCoinIsGoneInTime() {
            const game = create();
            const coin = dropCoin(game);
            const id = coin.coinId;
            const layer = findChild(game.scene, "coins");
            for (let frame = 0; frame < 150; ++frame) {
                game.session.advanceTime(33333);
            }
            tryVerify(() => layer.coinNamed(id) === null);
        }

        // The tap names the cell, the wide tower also takes the cell to its right.
        function test_theChosenTowerIsBuiltOnTheTappedCell() {
            const game = create();
            verify(game.session.buyCard(1));
            tryVerify(() => cards(game).length === 1);
            mouseClick(cards(game)[0]);
            compare(game.session.selectedTower, 1);
            verify(cards(game)[0].highlighted);
            const place = game.scene.cellOnScreen(4, 4);
            mouseClick(game.scene, place.x, place.y);
            const cells = game.session.towerCells;
            compare(cells.length, 2);
            compare(Qt.point(cells[0].x, cells[0].y), Qt.point(4, 4));
            compare(Qt.point(cells[1].x, cells[1].y), Qt.point(5, 4));
        }

        function test_theLayerLeavesTheMapItsTaps() {
            const game = create();
            verify(game.session.buyCard(0));
            game.session.chooseCard(0);
            const place = game.scene.cellOnScreen(4, 4);
            mouseClick(game.scene, place.x, place.y);
            compare(game.session.towerCount, 1);
        }
    }
}
