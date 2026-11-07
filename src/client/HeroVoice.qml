import QtQuick
import QtowerDefense.Client

// What the hero says and how it feels about it. Time here is game time, so a line waits out a pause
// and a test needs no clock. It only listens, the game does not know of it.
Item {
    id: root

    // The first ones in the order of GameSession.Cue.
    enum Moment {
        Build,
        Upgrade,
        Sell,
        Spell,
        Wave,
        Won,
        Lost,
        Roll,
        Coin,
        LifeLost,
        SpellChosen,
        HandFull,
        Broke
    }

    enum Mood {
        Calm,
        Glad,
        Worried,
        Casting
    }

    required property GameSession game

    property string line: ""
    property int moment: HeroVoice.Moment.Build
    readonly property bool over: game.won || game.lost
    readonly property bool spellInHand: game.selectedSpell >= 0 && !over
    readonly property int mood: spellInHand ? HeroVoice.Mood.Casting : line.length === 0
                                              ? HeroVoice.Mood.Calm : moods[moment]

    readonly property list<int> moods: [HeroVoice.Mood.Glad, HeroVoice.Mood.Glad,
        HeroVoice.Mood.Worried, HeroVoice.Mood.Casting, HeroVoice.Mood.Glad, HeroVoice.Mood.Glad,
        HeroVoice.Mood.Worried, HeroVoice.Mood.Glad, HeroVoice.Mood.Glad, HeroVoice.Mood.Worried,
        HeroVoice.Mood.Casting, HeroVoice.Mood.Worried, HeroVoice.Mood.Worried]
    readonly property list<string> buildLines: [qsTr("Nice spot. For a beginner."), qsTr(
            "That should scare a goblin. Maybe."), qsTr("Another one? Show-off.")]
    readonly property list<string> upgradeLines: [qsTr("Bigger is better. Usually."), qsTr(
            "Shiny. Now use it.")]
    readonly property list<string> sellLines: [qsTr("Selling? I liked that one."), qsTr(
            "Refunds. How heroic.")]
    readonly property list<string> spellLines: [qsTr("Mind your eyebrows!"), qsTr(
            "Stand back, this is art.")]
    readonly property list<string> waveLines: [qsTr("Early? Bold. Or foolish."), qsTr(
            "More for me!")]
    readonly property list<string> wonLines: [qsTr("We won! I did most of it.")]
    readonly property list<string> lostLines: [qsTr("I blame the architect.")]
    readonly property list<string> rollLines: [qsTr("Feeling lucky?"), qsTr(
            "The dice hate you. Roll anyway.")]
    readonly property list<string> coinLines: [qsTr("Mine! I mean, ours."), qsTr("Shiny!")]
    readonly property list<string> fullLines: [qsTr("Your hand is full. Like my patience.")]
    readonly property list<string> brokeLines: [qsTr("We are broke. Try not being broke.")]
    readonly property list<string> lifeLines: [qsTr("One got through! Were you watching?"), qsTr(
            "Ouch. That was my favorite life.")]
    readonly property list<string> chosenLines: [qsTr("Point where it should hurt.")]
    readonly property var lines: [buildLines, upgradeLines, sellLines, spellLines, waveLines,
        wonLines, lostLines, rollLines, coinLines, lifeLines, chosenLines, fullLines, brokeLines]

    property var spoken: lines.map(() => 0)
    property int lastLives: -1
    property int lastSpell: -1
    property real silentAt: 0 // the tick the line goes away
    property real quietUntil: 0 // no small talk before this tick

    function ticks(seconds) {
        return seconds * game.ticksPerSecond * game.speed;
    }

    function matters(what) {
        return what === HeroVoice.Moment.Spell || what === HeroVoice.Moment.Won || what
                === HeroVoice.Moment.Lost || what === HeroVoice.Moment.LifeLost || what
                === HeroVoice.Moment.SpellChosen;
    }

    function say(what) {
        const lastWord = what === HeroVoice.Moment.Won || what === HeroVoice.Moment.Lost;
        if (over && !lastWord || !matters(what) && game.tick < quietUntil) {
            return;
        }
        const choices = lines[what];
        line = choices[spoken[what] % choices.length];
        ++spoken[what];
        moment = what;
        silentAt = game.tick + ticks(Theme.speechSeconds);
        quietUntil = game.tick + ticks(Theme.quietSeconds);
    }

    function hush() {
        line = "";
        quietUntil = 0;
    }

    visible: false

    Connections {
        target: root.game

        function onCue(what: int) {
            root.say(what);
        }

        function onHandRefused(full: bool) {
            root.say(full ? HeroVoice.Moment.HandFull : HeroVoice.Moment.Broke);
        }

        function onStateChanged() {
            if (root.game.lives < root.lastLives) {
                root.say(HeroVoice.Moment.LifeLost);
            }
            root.lastLives = root.game.lives;
            if (root.game.tick >= root.silentAt) {
                root.line = "";
            }
        }

        function onSelectionChanged() {
            if (root.game.selectedSpell >= 0 && root.lastSpell < 0) {
                root.say(HeroVoice.Moment.SpellChosen);
            } else if (root.game.selectedSpell < 0 && root.moment
                       === HeroVoice.Moment.SpellChosen) {

                root.line = "";
            }
            root.lastSpell = root.game.selectedSpell;
        }

        function onMapChanged() {
            root.hush();
            root.lastLives = root.game.lives;
        }
    }
}
