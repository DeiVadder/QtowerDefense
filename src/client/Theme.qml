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
    readonly property color ironDark: "#1e2124"

    readonly property color text: "#f4e3c1"
    readonly property color gold: "#f2c23e"
    readonly property color heart: "#ff8a80"
    readonly property color danger: "#ffc4bc"

    readonly property color fire: "#e8702a"
    readonly property color ice: "#7fd4f5"

    readonly property color heroSkin: "#e2b48c"
    readonly property color heroBeard: "#b5562a"

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
    readonly property real endMargin: 0.5
    readonly property real heroSize: 2.2
    readonly property real leftRows: 2 // the rows of buttons above the hand, at most

    readonly property real cardWidth: 3
    readonly property real cardHeight: 4.25
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
    readonly property real smallestUnit: 40
    readonly property real unitsAcross: 14

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
}
