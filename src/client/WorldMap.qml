pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Window
import QtowerDefense.Client

// The campaign as one world in the fog (E-32): a framed map with a marker on every section, below
// it a sign with the chosen section and the button to play it. The hero at its left says what it
// thinks of the chosen section.
FramedPanel {
    id: root

    required property WorldSession world
    property real density: Screen.pixelDensity
    property real controlScale: 1 // the setting for the size of the controls
    property int quality: GameSettings.High // the setting for the quality of the pictures
    readonly property var chosen: world.sections[world.selected] ?? null
    readonly property var landscapeNames: ({
                                               meadow: qsTr("Meadow"),
                                               swamp: qsTr("Swamp"),
                                               forest: qsTr("Forest"),
                                               highland: qsTr("Highland"),
                                               heath: qsTr("Heath"),
                                               badlands: qsTr("Badlands"),
                                               barrows: qsTr("Barrows"),
                                               frost: qsTr("Frost"),
                                               ashlands: qsTr("Ashlands")
                                           })

    // The world may bring more sections than this client has words for: they keep their own name.
    readonly property var sectionNames: ({
                                             training_ground: qsTr("Training Ground"),
                                             mire_siege: qsTr("Mire Siege"),
                                             whispering_pines: qsTr("Whispering Pines"),
                                             grey_peaks: qsTr("Grey Peaks"),
                                             heather_downs: qsTr("Heather Downs"),
                                             red_gulch: qsTr("Red Gulch"),
                                             barrow_fields: qsTr("Barrow Fields"),
                                             frost_gate: qsTr("Frost Gate"),
                                             ashen_keep: qsTr("Ashen Keep")
                                         })

    property bool heroSpeaks: true
    property bool endless: false // offers the endless games of the won sections (E-36)
    // What the hero says of a landscape. The words for a section in the fog come first.
    readonly property var landscapeLines: ({
                                               meadow: qsTr("Grass, sheep, goblins. The usual."),
                                               swamp: qsTr(
                                                          "A swamp. Bring boots. Or a spare pair of legs."),
                                               forest: qsTr("Trees whisper here. Mostly insults."),
                                               highland: qsTr("Thin air, thick trolls."),
                                               heath: qsTr(
                                                          "Heather and ponds. Lovely, until the rats come."),
                                               badlands: qsTr(
                                                             "Red rock and fire imps. Bring water."),
                                               barrows: qsTr("Old graves. Let us not wake anyone."),
                                               frost: qsTr(
                                                          "Cold enough to freeze a wraith. Almost."),
                                               ashlands: qsTr(
                                                             "Ash, lava and a dragon. My favourite.")
                                           })

    signal play(int section)
    signal menu

    function lineOf(section: var): string {
        if (!section.open) {
            return qsTr("Nobody goes into that fog. Not even me.");
        }
        if (!section.playable) {
            return qsTr("No map of that place yet. I asked.");
        }
        if (endless && !section.endlessOpen) {
            return qsTr("Win it first. Then they never stop coming.");
        }
        if (section.stars >= 3) {
            return qsTr("Done that. Perfectly, thanks to me.");
        }
        return landscapeLines[section.landscape] ?? "";
    }

    function comment() {
        if (heroSpeaks && chosen !== null) {
            words.line = "";
            words.line = lineOf(chosen);
            silence.restart();
        }
    }

    onChosenChanged: comment()
    onVisibleChanged: {
        if (visible) {
            comment();
        }
    }

    unit: Theme.hudUnit(density, width, controlScale)

    function iconOf(section: var): string {
        if (section.stars > 0) {
            return "check";
        }
        if (!section.open) {
            return "lock";
        }
        return section.playable ? "flag" : "hourglass";
    }

    function canPlay(section: var): bool {
        return endless ? section.endlessOpen : section.playable;
    }

    function statusOf(section: var): string {
        if (endless && section.endlessOpen) {
            return qsTr("Best: %n round(s)", "", section.best);
        }
        if (canPlay(section)) {
            return "";
        }
        if (section.playable) {
            return qsTr("Not won yet");
        }
        return section.open ? qsTr("Not charted yet") : qsTr("Hidden in the fog");
    }

    Rectangle {
        id: frame

        objectName: "worldFrame"
        anchors.fill: parent
        anchors.margins: root.unit * Theme.gap
        // Clear of the notch and the rounded corners of a phone (E-55).
        anchors.leftMargin: root.unit * Theme.gap + root.SafeArea.margins.left
        anchors.rightMargin: root.unit * Theme.gap + root.SafeArea.margins.right
        anchors.bottomMargin: root.unit * (Theme.worldPanel + Theme.gap * 2)
        color: Theme.ironDark
        border.color: Theme.iron
        border.width: Math.max(1, root.unit * Theme.chosenBorder)

        WorldScene {
            id: scene

            objectName: "worldScene"
            anchors.fill: parent
            anchors.margins: frame.border.width
            world: root.world
            quality: root.quality
        }

        HudButton {
            objectName: "menuButton"
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: root.unit * Theme.gap
            z: 1
            unit: root.unit
            icon: "menu"
            text: qsTr("Menu")
            onClicked: root.menu()
        }

        Repeater {
            model: root.world.sections

            Rectangle {
                id: marker

                required property var modelData
                required property int index

                readonly property bool selected: index === root.world.selected
                // Read again when the view changes its size, the camera stays where it is.
                readonly property vector3d place: scene.camera && scene.width + scene.height > 0
                                                  ? scene.mapFrom3DScene(scene.placeOf(modelData,
                                                                                       Theme.worldMarkerHeight)) :
                                                    Qt.vector3d(0, 0, 0)

                objectName: "section_" + modelData.id
                x: scene.x + place.x - width / 2
                y: scene.y + place.y - height / 2
                width: root.unit * Theme.worldMarker
                height: width
                rotation: 45
                radius: width * Theme.corner
                color: modelData.open ? Theme.iron : Theme.ironDark
                border.color: selected ? Theme.accent : Theme.ironDark
                border.width: selected ? Math.max(Theme.thinnestChosenBorder, root.unit
                                                  * Theme.chosenBorder) * 2 : 1

                GameIcon {
                    anchors.centerIn: parent
                    width: parent.width * Theme.iconSize
                    height: width
                    rotation: -45
                    name: root.iconOf(marker.modelData)
                    fill: marker.modelData.stars > 0 ? Theme.gold : Theme.text
                }

                MouseArea {
                    anchors.fill: parent
                    Accessible.role: Accessible.Button
                    Accessible.name: root.sectionNames[marker.modelData.id] ?? marker.modelData.name
                    onClicked: root.world.selected = marker.index
                }
            }
        }
    }

    FramedPanel {
        id: sign

        objectName: "worldSign"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: root.unit * Theme.gap
        anchors.leftMargin: frame.anchors.leftMargin
        anchors.rightMargin: frame.anchors.rightMargin
        height: root.unit * Theme.worldPanel
        unit: root.unit
        seed: Theme.signSeed
        visible: root.chosen !== null

        HeroFace {
            id: hero

            objectName: "worldHero"
            anchors.left: parent.left
            anchors.leftMargin: root.unit * Theme.gap
            anchors.verticalCenter: parent.verticalCenter
            width: parent.height - root.unit * Theme.gap
            height: width
            mood: HeroVoice.Mood.Glad
            talking: words.visible
        }

        Column {
            anchors.left: hero.right
            anchors.leftMargin: root.unit * Theme.endMargin
            anchors.right: playButton.left
            anchors.verticalCenter: parent.verticalCenter
            spacing: root.unit * Theme.iconGap

            Text {
                objectName: "sectionName"
                width: parent.width
                elide: Text.ElideRight
                text: root.chosen ? root.sectionNames[root.chosen.id] ?? root.chosen.name : ""
                color: Theme.text
                font.family: Theme.face
                font.bold: true
                font.pixelSize: root.unit * Theme.titleText
            }

            Row {
                spacing: root.unit * Theme.gap

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.landscapeNames[root.chosen?.landscape] ?? ""
                    color: Theme.gold
                    font.family: Theme.face
                    font.pixelSize: root.unit * Theme.bodyText
                }

                Repeater {
                    model: root.endless ? 0 : 3

                    GameIcon {
                        required property int index

                        anchors.verticalCenter: parent.verticalCenter
                        width: root.unit * Theme.worldStar
                        height: width
                        name: "star"
                        fill: index < (root.chosen?.stars ?? 0) ? Theme.gold : Theme.starOff
                    }
                }

                Text {
                    objectName: "sectionStatus"
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.chosen ? root.statusOf(root.chosen) : ""
                    color: Theme.text
                    font.family: Theme.face
                    font.italic: true
                    font.pixelSize: root.unit * Theme.bodyText
                }
            }
        }

        HudButton {
            id: playButton

            objectName: "playButton"
            anchors.right: parent.right
            anchors.rightMargin: root.unit * Theme.endMargin
            anchors.verticalCenter: parent.verticalCenter
            width: root.unit * Theme.worldPlay
            height: width
            unit: root.unit
            icon: "sword"
            text: qsTr("Play")
            highlighted: available
            available: root.chosen ? root.canPlay(root.chosen) : false
            onClicked: root.play(root.world.selected)
        }
    }

    WordSign {
        id: words

        objectName: "worldWords"
        wordsName: "worldLine"
        x: sign.x + root.unit * Theme.gap
        y: frame.y + frame.height - height - root.unit * Theme.gap
        width: Math.min(root.unit * Theme.bubbleWidth, frame.width / 2)
        unit: root.unit
        visible: root.heroSpeaks && line.length > 0
    }

    Timer {
        id: silence

        interval: Theme.speechSeconds * 1000
        onTriggered: words.line = ""
    }
}
