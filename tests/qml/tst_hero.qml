pragma ComponentBehavior: Bound

import QtQuick
import QtTest
import QtowerDefense.Client

// TestCase itself is invisible: what should render needs a visible parent.
Item {
    id: stage

    width: 640
    height: 360

    Component {
        id: heroComponent

        Item {
            property alias session: session
            property alias hero: hero
            property alias map: map

            anchors.fill: parent

            MouseArea {
                id: map

                property int taps: 0

                anchors.fill: parent
                onClicked: ++taps
            }

            Hero {
                id: hero

                game: session
                unit: 40
            }

            GameSession {
                id: session

                Component.onCompleted: startNewGame(":/content/training_ground.json")
            }
        }
    }

    Component {
        id: hudComponent

        Item {
            property alias hud: hud
            property alias session: session

            anchors.fill: parent

            Hud {
                id: hud

                anchors.fill: parent
                density: 5
                game: session
            }

            GameSession {
                id: session

                Component.onCompleted: startNewGame(":/content/training_ground.json")
            }
        }
    }

    TestCase {
        name: "Hero"
        when: windowShown

        function create() {
            const made = createTemporaryObject(heroComponent, stage);
            return {
                hero: made.hero,
                game: made.session,
                map: made.map,
                voice: findChild(made.hero, "heroVoice"),
                face: findChild(made.hero, "heroFace"),
                bubble: findChild(made.hero, "heroBubble"),
                words: findChild(made.hero, "heroLine")
            };
        }

        function passSeconds(game, seconds) {
            for (let tick = 0; tick < seconds * game.ticksPerSecond / game.speed + 1; ++tick) {
                game.advanceTime(33334);
            }
        }

        function test_theHeroIsSilentAtFirst() {
            const hero = create();
            verify(!hero.bubble.visible);
            compare(hero.voice.mood, HeroVoice.Mood.Calm);
        }

        function test_aNewTowerGetsAComment() {
            const hero = create();
            verify(hero.game.buildTower(4, 4));
            verify(hero.bubble.visible);
            verify(hero.words.text.length > 0);
            compare(hero.voice.mood, HeroVoice.Mood.Glad);
        }

        // The settings can turn the words off: no sign, no moving mouth, the hero still looks.
        function test_aSilentHeroShowsNoWords() {
            const hero = create();
            hero.hero.speaks = false;
            verify(hero.game.buildTower(4, 4));
            verify(hero.voice.line.length > 0, "the hero still has something to say");
            verify(!hero.bubble.visible);
            verify(!hero.face.talking);
            hero.hero.speaks = true;
            verify(hero.bubble.visible);
            verify(hero.face.talking);
        }

        function test_aLineGoesAwayWithGameTime() {
            const hero = create();
            hero.game.buildTower(4, 4);
            passSeconds(hero.game, Theme.speechSeconds - 1);
            verify(hero.bubble.visible);
            passSeconds(hero.game, 2);
            verify(!hero.bubble.visible);
            compare(hero.voice.mood, HeroVoice.Mood.Calm);
        }

        function test_aFastGameLeavesTimeToRead() {
            const hero = create();
            hero.game.speed = hero.game.maxSpeed;
            hero.game.buildTower(4, 4);
            passSeconds(hero.game, Theme.speechSeconds - 1);
            verify(hero.bubble.visible);
        }

        function test_smallTalkWaitsForItsTurn() {
            const hero = create();
            hero.game.buildTower(4, 4);
            const first = hero.words.text;
            hero.game.buildTower(6, 4);
            compare(hero.words.text, first);
        }

        function test_theHeroDoesNotRepeatItself() {
            const hero = create();
            hero.game.buildTower(4, 4);
            const first = hero.words.text;
            passSeconds(hero.game, Theme.quietSeconds + 1);
            hero.game.buildTower(6, 4);
            verify(hero.words.text.length > 0);
            verify(hero.words.text !== first);
        }

        function test_aChosenSpellWakesTheHero() {
            const hero = create();
            hero.game.buildTower(4, 4);
            const smallTalk = hero.words.text;
            hero.game.selectSpell(1);
            verify(hero.words.text !== smallTalk);
            compare(hero.voice.mood, HeroVoice.Mood.Casting);
            verify(Qt.colorEqual(hero.face.spellColor, Theme.ice));
            verify(findChild(hero.face, "heroSpell").visible);
        }

        function test_theHeroLooksWhereTheSpellLands() {
            const hero = create();
            verify(!hero.face.focused);
            hero.game.selectSpell(0);
            passSeconds(hero.game, 1);
            verify(hero.game.castSpellAt(6, 3));
            verify(hero.face.focused);
            compare(hero.face.look.x, 6 / hero.game.gridSize.width);
            compare(hero.face.look.y, 3 / hero.game.gridSize.height);
        }

        function test_aLostLifeWorriesTheHero() {
            const hero = create();
            const lives = hero.game.lives;
            for (let frame = 0; frame < 100000 && hero.game.lives === lives; ++frame) {
                hero.game.advanceTime(33334);
            }
            verify(hero.game.lives < lives);
            compare(hero.voice.moment, HeroVoice.Moment.LifeLost);
            compare(hero.voice.mood, HeroVoice.Mood.Worried);
        }

        function test_theLastWordStays() {
            const hero = create();
            for (let frame = 0; frame < 100000 && !hero.game.lost; ++frame) {
                hero.game.advanceTime(33334);
            }
            verify(hero.game.lost);
            compare(hero.voice.moment, HeroVoice.Moment.Lost);
            hero.game.selectSpell(0);
            compare(hero.voice.moment, HeroVoice.Moment.Lost);
            verify(hero.bubble.visible);
            compare(hero.voice.mood, HeroVoice.Mood.Worried);
        }

        function test_aChosenSpellStaysInHand() {
            const hero = create();
            hero.game.selectSpell(0);
            passSeconds(hero.game, Theme.speechSeconds + 1);
            verify(!hero.bubble.visible);
            verify(findChild(hero.face, "heroSpell").visible);
        }

        function test_aSpellPutAwayEndsItsLine() {
            const hero = create();
            hero.game.selectSpell(0);
            hero.game.selectSpell(0);
            compare(hero.game.selectedSpell, -1);
            verify(!hero.bubble.visible);
            verify(!findChild(hero.face, "heroSpell").visible);
        }

        function test_aLineWaitsOutAPause() {
            const hero = create();
            hero.game.buildTower(4, 4);
            hero.game.paused = true;
            passSeconds(hero.game, Theme.speechSeconds + 1);
            verify(hero.bubble.visible);
        }

        function test_onlyTheBubbleLetsTapsThrough() {
            const hero = create();
            hero.game.buildTower(4, 4);
            waitForRendering(hero.bubble);
            mouseClick(hero.face);
            compare(hero.map.taps, 0);
            mouseClick(hero.bubble);
            compare(hero.map.taps, 1);
        }

        function test_theMomentsFollowTheCues() {
            compare(HeroVoice.Moment.Build, GameSession.Cue.Build);
            compare(HeroVoice.Moment.Upgrade, GameSession.Cue.Upgrade);
            compare(HeroVoice.Moment.Sell, GameSession.Cue.Sell);
            compare(HeroVoice.Moment.Spell, GameSession.Cue.Spell);
            compare(HeroVoice.Moment.Wave, GameSession.Cue.Wave);
            compare(HeroVoice.Moment.Won, GameSession.Cue.Won);
            compare(HeroVoice.Moment.Lost, GameSession.Cue.Lost);
        }

        function test_theHeroLeavesTheButtonsAlone_data() {
            return [
                        {
                            tag: "wide and low",
                            width: 900,
                            height: 320
                        },
                        {
                            tag: "small",
                            width: 480,
                            height: 320
                        },
                        {
                            tag: "roomy",
                            width: 1280,
                            height: 720
                        },
                        {
                            tag: "dense",
                            width: 800,
                            height: 360,
                            density: 6.3
                        }
                    ];
        }

        function apart(first, second, hud) {
            const one = first.mapToItem(hud, 0, 0);
            const other = second.mapToItem(hud, 0, 0);
            return one.x + first.width <= other.x || other.x + second.width <= one.x || one.y
                    + first.height <= other.y || other.y + second.height <= one.y;
        }

        function test_theHeroLeavesTheButtonsAlone(data) {
            stage.width = data.width;
            stage.height = data.height;
            const made = createTemporaryObject(hudComponent, stage);
            const hud = made.hud;
            if (data.density !== undefined) {
                hud.density = data.density;
            }
            const clock = findChild(hud, "buttons");
            const actions = findChild(hud, "towerActions");
            verify(made.session.buildTower(4, 4));
            verify(made.session.chooseTowerAt(4, 4));
            tryVerify(() => actions.visible && actions.height > 0);
            // The hand first has to sink.
            const hero = findChild(hud, "hero");
            const lowest = Math.max(clock.y + clock.height, hero.y + hero.height);
            tryVerify(() => lowest <= actions.y - hud.unit * Theme.inset, 2000, lowest + " and "
            + actions.y);
            const stats = findChild(hud, "stats");
            const buttons = clock.children.filter(child => child.width > 0);
            compare(buttons.length, made.session.maxSpeed + 1);
            for (const button of buttons) {
                verify(apart(button, stats, hud));
                // The HUD asks the clock about a dropped card: it has to reach as far as its buttons.
                const edge = button.mapToItem(hud, button.width - 1, button.height - 1);
                compare(hud.childAt(edge.x, edge.y), clock);
            }
            made.session.clearChosenTower();
            made.session.paused = true;
            const save = findChild(hud, "savePanel");
            tryVerify(() => save.visible && save.height > 0);
            waitForPolish(hud);
            verify(apart(save, stats, hud));
            verify(buttons.every(button => apart(save, button, hud)));
            stage.width = 640;
            stage.height = 360;
        }

        function test_aNewGameStartsInSilence() {
            const hero = create();
            hero.game.buildTower(4, 4);
            hero.game.restart();
            verify(!hero.bubble.visible);
            hero.game.buildTower(4, 4);
            verify(hero.bubble.visible);
        }

        function test_everyMomentHasWordsAndAMood() {
            const voice = create().voice;
            compare(voice.lines.length, HeroVoice.Moment.Broke + 1);
            compare(voice.moods.length, voice.lines.length);
            compare(voice.spoken.length, voice.lines.length);
        }

        function test_theMoodShowsInTheFace() {
            const face = create().face;
            const calm = face.mouth;
            face.mood = HeroVoice.Mood.Worried;
            verify(face.mouth !== calm);
        }
    }
}
