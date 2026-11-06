pragma Singleton

import QtQuick

// Every color and type of the interface, see E-34. A component names no color of its own.
QtObject {
    readonly property color accent: "#2cde85"

    readonly property color wood: "#6b4426"
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
    readonly property real leftRows: 3 // the rows of buttons below the hero, at most
    readonly property real bubbleWidth: 6
    readonly property real plank: 0.6
    readonly property real bracket: 0.3
    readonly property real bracketBar: 0.09

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
    readonly property real outline: 3

    // The darkest plank, the lightest one has the color wood. Text must be readable on that one.
    readonly property real darkestPlank: 0.85
}
