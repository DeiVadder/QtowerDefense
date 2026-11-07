pragma ComponentBehavior: Bound

import QtQuick
import QtowerDefense.Client

// The main menu, after demo A (E-34): a castle wall, the gate with the hero between two torches and
// signs on chains at the right. The settings take the place of the signs.
Item {
    id: root

    required property GameSettings settings
    property bool settingsOpen: false
    // A text menu, as large as the window allows. Signs larger than a finger do no harm.
    readonly property real unit: Math.min(width / Theme.unitsAcross, height / Theme.menuUnitsHigh)
    readonly property real margin: unit * Theme.gap * 2
    readonly property list<string> greetings: [qsTr("Open the gate? In this economy?"), qsTr(
            "Back again? The goblins missed you."), qsTr("I polished my helmet. For you.")]
    readonly property list<string> settingsLines: [qsTr("Fiddling with knobs again?"), qsTr(
            "Louder! I cannot hear my own genius.")]

    enum Choice {
        Campaign,
        Endless,
        Settings,
        Quit
    }

    signal campaign
    signal endless
    signal quit

    function choose(choice: int) {
        if (choice === MainMenu.Choice.Campaign) {
            campaign();
        } else if (choice === MainMenu.Choice.Endless) {
            endless();
        } else if (choice === MainMenu.Choice.Settings) {
            settingsOpen = true;
            say(settingsLines);
        } else {
            quit();
        }
    }

    // Closes what is open, and says whether there was something.
    function back(): bool {
        if (!settingsOpen) {
            return false;
        }
        settingsOpen = false;
        return true;
    }

    function say(lines: list<string>) {
        sign.line = lines[Math.floor(Math.random() * lines.length)];
        silence.restart();
    }

    onVisibleChanged: {
        if (visible) {
            say(greetings);
        }
    }
    Component.onCompleted: say(greetings)

    Canvas {
        id: wall

        anchors.fill: parent
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPaint: {
            const context = getContext("2d");
            context.reset();
            let state = Theme.wallSeed;
            const random = () => {
                state = (state * 1103515245 + 12345) % 2147483648;
                return state / 2147483648;
            };
            const brickWidth = root.unit * Theme.brickWidth;
            const brickHeight = brickWidth / 2;
            const joint = Math.max(1, root.unit * Theme.brickJoint);
            context.fillStyle = Theme.wallJoint;
            context.fillRect(0, 0, width, height);
            for (let row = 0; row * brickHeight < height; ++row) {
                for (let column = -1; column * brickWidth < width; ++column) {
                    const shade = Theme.darkestBrick + (1 - Theme.darkestBrick) * random();
                    context.fillStyle = Qt.rgba(Theme.wallStone.r * shade, Theme.wallStone.g * shade,
                                                Theme.wallStone.b * shade, 1);
                    context.fillRect(column * brickWidth + (row % 2) * brickWidth / 2 + joint, row
                                     * brickHeight + joint, brickWidth - 2 * joint, brickHeight - 2
                                     * joint);
                }
            }
            const middle = Qt.point(gate.x + gate.width / 2, gate.y + gate.height / 2);
            const shadow = context.createRadialGradient(middle.x, middle.y, gate.width / 4, middle.x,
                                                        middle.y, Math.max(width, height));
            shadow.addColorStop(0, Theme.wallLight);
            shadow.addColorStop(1, Theme.wallShadow);
            context.fillStyle = shadow;
            context.fillRect(0, 0, width, height);
            for (const torch of [-1, 1]) {
                const x = middle.x + torch * (gate.width / 2 + root.unit * Theme.menuTorch);
                const y = gate.y + gate.height * Theme.torchHeight;
                const glow = context.createRadialGradient(x, y, 0, x, y, root.unit
                                                          * Theme.torchGlowSize);
                glow.addColorStop(0, Theme.torchGlow);
                glow.addColorStop(1, Theme.wallLight);
                context.fillStyle = glow;
                context.fillRect(x - root.unit * Theme.torchGlowSize, y - root.unit * Theme.torchGlowSize,
                                 2 * root.unit * Theme.torchGlowSize, 2 * root.unit
                                 * Theme.torchGlowSize);
            }
        }
    }

    // The hero follows the mouse with its eyes. It lies under all else
    // and takes no taps.
    MouseArea {
        id: looking

        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
    }

    FramedPanel {
        id: title

        x: root.margin
        y: root.margin
        width: root.width - 2 * root.margin
        height: root.unit * Theme.menuTitle
        unit: root.unit

        Text {
            anchors.centerIn: parent
            width: parent.width - 2 * root.margin
            horizontalAlignment: Text.AlignHCenter
            fontSizeMode: Text.HorizontalFit
            text: qsTr("QtowerDefense")
            color: Theme.gold
            style: Text.Raised
            styleColor: Theme.burnt
            font.family: Theme.face
            font.bold: true
            font.pixelSize: root.unit * Theme.menuTitleText
        }
    }

    // The gate fills the left half below the title, as high as it can.
    Item {
        id: gate

        readonly property real room: root.height - title.y - title.height
        readonly property real hero: width * Theme.menuHero

        x: root.width / 4 - width / 2
        y: title.y + title.height
        width: Math.min(root.width / 2 - 2 * root.unit * Theme.menuTorch, room * Theme.gateAspect)
        height: room
        onWidthChanged: wall.requestPaint()

        Canvas {
            anchors.fill: parent
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            onPaint: {
                const context = getContext("2d");
                context.reset();
                const arch = inset => {
                    const radius = width / 2 - inset;
                    context.beginPath();
                    context.moveTo(inset, height);
                    context.lineTo(inset, radius + inset);
                    context.arc(width / 2, radius + inset, radius, Math.PI, 0);
                    context.lineTo(width - inset, height);
                    context.closePath();
                };
                const stone = width * Theme.gateStone;
                context.fillStyle = Theme.gateArch;
                arch(0);
                context.fill();
                const door = context.createLinearGradient(stone, 0, width - stone, 0);
                door.addColorStop(0, Theme.woodDark);
                door.addColorStop(0.5, Theme.wood);
                door.addColorStop(1, Theme.woodDark);
                context.fillStyle = door;
                arch(stone);
                context.fill();
                context.save();
                context.clip();
                context.strokeStyle = Theme.burnt;
                context.lineWidth = Theme.outline;
                const planks = Theme.gatePlanks;
                for (let plank = 1; plank < planks; ++plank) {
                    const x = stone + plank * (width - 2 * stone) / planks;
                    context.beginPath();
                    context.moveTo(x, 0);
                    context.lineTo(x, height);
                    context.stroke();
                }
                context.fillStyle = Theme.iron;
                const band = width * Theme.gateBand;
                for (const share of Theme.gateBands) {
                    context.fillRect(0, height * share, width, band);
                }
                context.restore();
            }
        }

        Repeater {
            model: 2

            delegate: Item {
                id: torch

                required property int index

                readonly property real size: root.unit * Theme.menuTorch

                x: (index === 0 ? -size * 1.5 : gate.width + size / 2)
                y: gate.height * Theme.torchHeight - size
                width: size
                height: size * (Theme.torchStretch + 1)

                Rectangle {
                    x: (torch.width - width) / 2
                    y: torch.size * Theme.torchStretch * 0.8
                    width: torch.size * Theme.torchStick
                    height: torch.size
                    color: Theme.woodDark
                    border.color: Theme.ironDark
                }

                GameIcon {
                    width: torch.size
                    height: torch.size * Theme.torchStretch
                    name: "meteor"
                    fill: Theme.fire
                    transformOrigin: Item.Bottom

                    SequentialAnimation on scale {
                        loops: Animation.Infinite

                        NumberAnimation {
                            to: Theme.flameHigh
                            duration: Theme.flameMillis
                        }
                        NumberAnimation {
                            to: Theme.flameLow
                            duration: Theme.flameMillis * 1.3
                        }
                    }
                }
            }
        }

        HeroFace {
            id: face

            objectName: "menuHero"
            x: (gate.width - gate.hero) / 2
            y: gate.height * Theme.menuHeroTop
            width: gate.hero
            height: gate.hero
            mood: HeroVoice.Mood.Glad
            talking: root.settings.heroSpeaks && sign.line.length > 0
            focused: looking.containsMouse
            lookAt: Qt.point(looking.mouseX / root.width, looking.mouseY / root.height)
        }

        WordSign {
            id: sign

            objectName: "menuSign"
            wordsName: "menuLine"
            unit: root.unit
            width: Math.min(root.unit * Theme.bubbleWidth, gate.width)
            x: (gate.width - width) / 2
            y: face.y + face.height
            visible: root.settings.heroSpeaks && line.length > 0
        }
    }

    Timer {
        id: silence

        interval: Theme.speechSeconds * 1000
        onTriggered: sign.line = ""
    }

    // Placed by hand, not in a Column: the signs stand where they belong before the first frame.
    Item {
        id: signs

        readonly property real signWidth: Math.min(root.unit * Theme.menuSignWidth, root.width / 2
                                                   - 2 * root.margin)

        x: root.width * 3 / 4 - signWidth / 2
        y: title.y + title.height
        visible: !root.settingsOpen

        Repeater {
            model: [
                {
                    name: "campaignButton",
                    text: qsTr("Campaign"),
                    choice: MainMenu.Choice.Campaign
                },
                {
                    name: "endlessButton",
                    text: qsTr("Endless"),
                    choice: MainMenu.Choice.Endless
                },
                {
                    name: "settingsButton",
                    text: qsTr("Settings"),
                    choice: MainMenu.Choice.Settings
                },
                {
                    name: "quitButton",
                    text: qsTr("Quit"),
                    choice: MainMenu.Choice.Quit
                }
            ]

            delegate: ChainSign {
                id: menuSign

                required property var modelData
                required property int index

                objectName: modelData.name
                visible: modelData.choice !== MainMenu.Choice.Quit || AppInfo.windowed
                y: index * height
                width: signs.signWidth
                height: chain + root.unit * Theme.menuSign
                unit: root.unit
                chain: root.unit * Theme.signChain * 2
                seed: Theme.menuSeed + index
                Accessible.role: Accessible.Button
                Accessible.name: modelData.text

                Text {
                    anchors.centerIn: parent
                    text: menuSign.modelData.text
                    color: Theme.text
                    style: Text.Raised
                    styleColor: Theme.burnt
                    font.family: Theme.face
                    font.bold: true
                    font.pixelSize: root.unit * Theme.menuText
                }

                MouseArea {
                    parent: menuSign
                    anchors.fill: parent
                    onClicked: {
                        menuSign.swing();
                        root.choose(menuSign.modelData.choice);
                    }
                }
            }
        }
    }

    SettingsPanel {
        objectName: "settingsPanel"
        // Beside the gate where there is room, else over it.
        readonly property bool beside: root.width / 2 - 2 * root.margin >= root.unit
                                       * Theme.settingsWidest

        x: beside ? root.width / 2 + root.margin : root.margin
        y: title.y + title.height + root.margin
        width: beside ? root.width / 2 - 2 * root.margin : root.width - 2 * root.margin
        height: Math.min(root.height - y - root.margin, root.unit * Theme.settingsHeight)
        unit: root.unit
        settings: root.settings
        visible: root.settingsOpen
        onClose: root.settingsOpen = false
    }
}
