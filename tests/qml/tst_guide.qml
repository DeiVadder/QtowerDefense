pragma ComponentBehavior: Bound

import QtQuick
import QtTest
import QtowerDefense.Client

// TestCase itself is invisible: what should render or take input needs a visible parent.
Item {
    id: stage

    width: 960
    height: 540

    Component {
        id: gameComponent

        Item {
            property alias hud: hud
            property alias session: session

            anchors.fill: parent

            Hud {
                id: hud

                anchors.fill: parent
                game: session
                guide: true
            }

            GameSession {
                id: session

                Component.onCompleted: startNewGame(richMap) // qmllint disable unqualified
            }
        }
    }

    Component {
        id: spyComponent

        SignalSpy {
            signalName: "guided"
        }
    }

    TestCase {
        name: "Guide"
        when: windowShown

        function create() {
            const game = createTemporaryObject(gameComponent, stage);
            waitForRendering(game.hud);
            return game;
        }

        function guideOf(game) {
            return findChild(game.hud, "guideSign").parent;
        }

        function centerOf(item, root) {
            return item.mapToItem(root, item.width / 2, item.height / 2);
        }

        // The ring lies around the control the step is about.
        function ringsAround(game, name) {
            const ring = findChild(game.hud, "guideRing");
            const target = centerOf(findChild(game.hud, name), ring.parent);
            return ring.visible && target.x > ring.x && target.x < ring.x + ring.width && target.y
                    > ring.y && target.y < ring.y + ring.height;
        }

        function test_theFirstGameStandsStillAndPointsAtTheDice() {
            const game = create();
            verify(guideOf(game).visible);
            verify(game.session.paused);
            tryVerify(() => ringsAround(game, "rollButton"));
            compare(findChild(game.hud, "guideText").text,
                    "Towers come as cards. Tap the dice to roll for one.");
        }

        function test_eachStepWaitsForThePlayer() {
            const game = create();
            const spy = createTemporaryObject(spyComponent, stage, {
                                                  target: game.hud
                                              });
            const guide = guideOf(game);
            mouseClick(findChild(game.hud, "rollButton"));
            compare(guide.step, Guide.Choose);
            tryVerify(() => guide.hole.width > 0); // the hand
            game.session.chooseCard(0);
            compare(guide.step, Guide.Turn);
            tryVerify(() => ringsAround(game, "rotateButton"));
            mouseClick(findChild(game.hud, "rotateButton"));
            compare(guide.step, Guide.Build);
            tryVerify(() => !findChild(game.hud, "guideRing").visible); // the whole map is meant
            verify(game.session.paused);
            verify(game.session.buildTower(4, 4));
            compare(guide.step, Guide.Start);
            tryVerify(() => ringsAround(game, "pauseButton"));
            compare(spy.count, 0);
            mouseClick(findChild(game.hud, "pauseButton"));
            verify(!game.session.paused);
            compare(spy.count, 1);
            verify(!guide.visible);
        }

        // Who builds right away has done the first four steps at once.
        function test_aPlayerAheadOfTheGuideSkipsSteps() {
            const game = create();
            const guide = guideOf(game);
            verify(game.session.buyCard(0));
            compare(guide.step, Guide.Choose);
            game.session.chooseCard(0);
            verify(game.session.buildTower(4, 4));
            compare(guide.step, Guide.Start);
        }

        function test_theTurnCanBeLeftOut() {
            const game = create();
            const guide = guideOf(game);
            verify(game.session.buyCard(0));
            game.session.chooseCard(0);
            compare(guide.step, Guide.Turn);
            wait(2 * Theme.swingTime + Theme.swingRest
                 + 50); // a swinging sign is not where it seems
            mouseClick(findChild(game.hud, "guideNext"));
            compare(guide.step, Guide.Build);
        }

        function test_skippingLetsTheGameRun() {
            const game = create();
            const spy = createTemporaryObject(spyComponent, stage, {
                                                  target: game.hud
                                              });
            mouseClick(findChild(game.hud, "guideSkip"));
            compare(spy.count, 1);
            verify(!guideOf(game).visible);
            verify(!game.session.paused);
        }

        function test_withoutTheWishThereIsNoGuide() {
            const game = create();
            game.hud.guide = false;
            verify(!guideOf(game).visible);
            game.session.paused = false;
            game.hud.guide = true; // a new wish starts at the beginning
            compare(guideOf(game).step, Guide.Roll);
            verify(game.session.paused);
        }
    }
}
