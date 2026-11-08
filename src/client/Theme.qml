pragma Singleton

import QtQuick

// Every color and type of the interface, see E-34. A component names no color of its own.
QtObject {
    readonly property color accent: "#2cde85"

    readonly property color wood: "#6b4426"
    readonly property color cardWood: "#9a6a3a"
    readonly property color cardWoodDark: "#8a5e33"
    readonly property color burnt: "#22130a"
    readonly property color grain: "#52190c04"
    readonly property color plankEdge: "#24ffe1aa"
    readonly property color plankGap: "#bf0f0702"
    readonly property color woodDark: "#2f1c0f"
    readonly property color iron: "#4a4f54"
    readonly property color ironLight: "#868d92"
    readonly property color ironDark: "#1e2124"

    readonly property color text: "#f4e3c1"
    readonly property color gold: "#f2c23e"
    readonly property color heart: "#ff8a80"
    readonly property color danger: "#ffc4bc"

    readonly property color fire: "#e8702a"
    readonly property color ice: "#7fd4f5"

    readonly property color heroSkin: "#d9a678"
    readonly property color heroSkinDark: "#c48f62"
    readonly property color heroLine: "#4a2a16"
    readonly property color heroArmor: "#3c3f44"
    readonly property color heroShield: "#6d2a20"
    readonly property color heroBrow: "#5b5148"
    readonly property color heroMoustache: "#7b6f62"
    readonly property color heroMoustacheLine: "#2d2620"
    readonly property color heroMouth: "#2a0d08"
    readonly property color heroEye: "#f4efe2"
    readonly property color heroPupil: "#22190f"
    readonly property color helmetLight: "#9aa1a6"
    readonly property color helmetMiddle: "#6b7277"
    readonly property color helmetDark: "#3c4146"
    readonly property color signWood: "#c9a06a"

    function spellColor(spell: int): color {
        return spell === 0 ? fire : ice;
    }

    readonly property string face: Qt.fontFamilies().includes("Rockwell") ? "Rockwell" : "Georgia"

    // In shares of the unit of the HUD.
    readonly property real gap: 0.25
    readonly property real inset: 0.125 // a frame around what it holds, and small gaps
    readonly property real iconSize: 0.6
    readonly property real iconLine: 0.03 // of the width of an icon
    readonly property real corner: 0.125
    readonly property real buttonPadding: 0.4
    readonly property real iconGap: 0.1
    readonly property real chosenBorder: 0.0625
    readonly property real statsHeight: 1.25
    readonly property real statsPadding: 0.6
    readonly property real endWidth: 8
    readonly property real endHeight: 3.5
    readonly property real endRoundsHeight: 1.2
    readonly property real endMargin: 0.5
    readonly property real heroSize: 3.4
    readonly property real dialButton: 0.9 // what stands around the portrait
    readonly property real dialGap: 0.45 // from the portrait to the middle of a button
    readonly property real dialTurn: 90 // degrees from the first button to the last
    readonly property real signChain: 0.3
    readonly property real chainInset: 0.15 // of the width of the sign
    readonly property int signSeed: 21
    readonly property real swingStart: -6 // degrees of a sign that drops in
    readonly property real swingOut: 3
    readonly property real swingBack: -1.5
    readonly property int swingTime: 170
    readonly property int swingRest: 130
    readonly property real signBracket: 0.6
    readonly property real signText: 0.36
    readonly property real leftRows: 2 // the rows of buttons above the hand, at most
    readonly property real leftInsets: 3 // the edges of the window and of the frames along them

    readonly property real cardWidth: 3
    readonly property real cardHeight: 4.25
    readonly property real coinSize: 0.7
    readonly property real coinTap: 1.2 // a finger hits more than the coin
    readonly property real coinHop: 1.2
    readonly property real coinRim: 0.08 // of the coin
    readonly property real coinMark: 0.6
    readonly property int coinMillis: 500
    readonly property int diceSteps: 9
    readonly property int diceFirstStep: 45
    readonly property real diceSlowing: 3.5
    readonly property int diceMillis: 1100
    readonly property int diceRest: 450
    readonly property real diceSpin: 1080
    readonly property int diceSeed: 33
    readonly property int shopSeed: 12
    readonly property real cardPeek: 1.6 // what stays of the hand while a tower is chosen
    readonly property real cardRaise: 1.15
    readonly property real cardChosenScale: 1.1
    readonly property real cardStep: 0.9 // of its width, when there is room
    readonly property real cardTilt: 4.5 // degrees from one card to the next
    readonly property real cardSag:
        0.067 // how far the outer cards hang, by the square of the place

    readonly property real cardPlank: 0.84
    readonly property real cardLift: 0.4 // the fan above the edge of the window
    readonly property real handShare: 16 // the unit of a card is at most this share of the height
    readonly property real cardDragStart: 0.25 // the way a finger goes before a tap is a drag
    readonly property real cardHeldScale: 0.7
    readonly property real cardLandedScale: 0.15
    readonly property real cardLandedTilt: 60 // degrees: it lies down on the board
    readonly property int cardMillis: 220
    readonly property int flightMillis: 350

    // The opacity of the grid on the map.
    readonly property real gridFaint: 0.16
    readonly property real gridHeld: 0.55

    readonly property real bubbleWidth: 6
    readonly property real plank: 0.6
    readonly property real bracket: 0.3
    readonly property real bracketBar: 0.09
    readonly property real rivet: 0.09
    readonly property real rivetOpacity: 0.6

    readonly property real bodyText: 0.42
    readonly property real statText: 0.46
    readonly property real smallText: 0.3
    readonly property real titleText: 0.6

    // The unit of the HUD: a finger in millimeters, at least some pixels, and a share of a window.
    readonly property real fingerSize: 9

    // The world map (E-32): muted land on a grey sea, the fog covers what is not open.
    readonly property color worldSea: "#3f565c"
    readonly property color worldShore: "#b9a77e"
    readonly property color worldWay: "#e2cd98"
    readonly property color worldFog: "#e8ebe8"
    readonly property color starOff: "#2b2f33"
    readonly property real worldWidth: 16 // in the units of the 3D scene
    readonly property real worldDepth: 9
    readonly property real worldLand: 0.085 // the radius of a section, of the width of the world
    readonly property real worldWayWidth: 0.006
    readonly property real worldLandmark: 0.45 // of the size of a model in its landscape
    // Where the three landmarks of a section stand, from its middle, in the units of the scene.
    readonly property list<vector3d> worldLandmarkPlaces: [Qt.vector3d(0, 0, -0.35), Qt.vector3d(-0.8, 0,
                                                                                                 0.3), Qt.vector3d(
            0.8, 0, 0.4)]
    readonly property color worldAmbient: "#737373"
    readonly property real worldFogMiddle: 0.85 // the opacity of a cloud halfway out
    readonly property real worldFogSize: 4.4 // the width of one cloud
    readonly property real worldFogTop: 1.25 // the highest layer, taller models rise above it
    readonly property real worldFogOpacity: 0.62
    readonly property real worldMarker: 0.95
    readonly property real worldMarkerHeight: 2.3 // above the land, in the units of the scene
    readonly property real worldPanel: 2.6
    readonly property real worldStar: 0.5
    readonly property real worldPlay: 1.5
    readonly property int worldPicture: 1600 // the width of the painted land in pixels
    readonly property real smallestUnit: 40
    readonly property real unitsAcross: 14

    // The main menu: a wall of bricks, the gate with the hero, torches, signs on chains.
    readonly property color wallStone: "#4e4943"
    readonly property color wallJoint: "#14100d"
    readonly property color wallLight: "#00000000"
    readonly property color wallShadow: "#c7000000"
    readonly property color gateArch: "#3d3833"
    readonly property int wallSeed: 41
    readonly property int menuSeed: 40
    readonly property real darkestBrick: 0.7
    readonly property real brickWidth: 2.1
    readonly property real brickJoint: 0.04
    readonly property real menuUnitsHigh: 12 // the menu fits a window this many units high
    readonly property real menuTitle: 1.9
    readonly property real menuTitleText: 1.25
    readonly property real menuSign: 1.4
    readonly property real menuSignWidth: 7
    readonly property real menuText: 0.55
    readonly property real menuTorch: 0.9
    readonly property real torchStick: 0.25 // of the flame
    readonly property real torchGlowSize: 3.2
    readonly property color torchGlow: "#59e8702a"
    readonly property real torchStretch: 1.4
    readonly property real torchHeight: 0.3 // of the gate, from its top
    readonly property real flameHigh: 1.14
    readonly property real flameLow: 0.92
    readonly property int flameMillis: 130
    readonly property real gateAspect: 0.85 // width by height
    readonly property real gateStone: 0.08 // of the width of the gate
    readonly property int gatePlanks: 7
    readonly property real gateBand: 0.035
    readonly property list<real> gateBands: [0.45, 0.85] // from the top, in shares of the height
    readonly property real menuHero: 0.42 // of the width of the gate
    readonly property real menuHeroTop: 0.22
    readonly property int settingsSeed: 9
    readonly property real settingsHeight: 7.75
    readonly property real settingsRow: 1.1
    readonly property real settingsLabel: 2.2
    readonly property real settingsPercent: 1.6
    readonly property real settingsToggle: 1.8
    readonly property real settingsChoice: 2.4 // wide enough for "Deutsch" and "System"
    readonly property real sliderTrack: 0.2
    readonly property real sliderKnob: 0.55
    readonly property real loudnessRange: 100 // from the quietest step of the slider to the loudest
    readonly property int volumeStep: 10 // percent per press of an arrow key
    // Narrower halves of the menu give the settings all their width: margins, label, track, percent.
    readonly property real settingsWidest: 4 * endMargin + settingsLabel + settingsTrack
                                           + settingsPercent + gap
    readonly property real settingsTrack: 3 // the slider gets at least this, before the label

    readonly property real unavailable: 0.5 // the opacity of what cannot be used now

    // How long the hero talks, and how long it keeps small talk to itself, in seconds of play.
    readonly property int speechSeconds: 4
    readonly property int quietSeconds: 10

    // In pixels, what must stay visible on a small screen.
    readonly property real thinnestBar: 3
    readonly property real thinnestChosenBorder: 2
    readonly property real seam: 1.5
    readonly property int grainLines: 4 // on every plank
    readonly property real grainMargin: 3
    readonly property real grainBend: 3
    readonly property real grainWidth: 1
    readonly property real grainFirstBend: 0.3 // of the width of the plank
    readonly property real grainSecondBend: 0.6
    readonly property real outline: 3

    // The darkest plank, the lightest one has the color wood. Text must be readable on that one.
    readonly property real darkestPlank: 0.85

    // The splash screen at the start and the curtain while a map loads (E-42).
    readonly property int introMillis: 2200
    readonly property int curtainMillis: 350 // to fade in and out
    readonly property int emblemMillis: 900 // the emblem grows into its place
    readonly property int curtainAloneFrames: 2 // frames of the curtain before the scene shows
    readonly property int curtainFrames: 3 // frames of the scene behind the curtain before it lifts
    readonly property int emblemPixels: 256 // the size of icon.png
    readonly property real emblemShare: 0.38 // of the short side of the window
    readonly property real emblemStart: 0.6 // the size the emblem grows from
    readonly property real splashTitle: 0.09 // of the short side
    readonly property real splashNote: 0.035
}
