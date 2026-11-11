import QtQuick
import QtTest
import QtowerDefense.Client

// TestCase itself is invisible: what should take input needs a visible parent.
Item {
    id: stage

    width: 200
    height: 200

    Component {
        id: framedMap

        Item {
            property alias taps: map.taps

            anchors.fill: parent

            MouseArea {
                id: map

                property int taps: 0

                anchors.fill: parent
                onClicked: ++taps
            }

            FramedPanel {
                width: 100
                height: 100
                unit: 40
            }
        }
    }

    TestCase {
        name: "Theme"
        when: windowShown

        // WCAG: the relative luminance of a color, and a ratio of at least 4.5 for text.
        function luminance(color) {
            const linear = part => part <= 0.03928 ? part / 12.92 : Math.pow((part + 0.055) / 1.055,
                                                                             2.4);
            return 0.2126 * linear(color.r) + 0.7152 * linear(color.g) + 0.0722 * linear(color.b);
        }

        function contrast(one, two) {
            const lighter = Math.max(luminance(one), luminance(two));
            const darker = Math.min(luminance(one), luminance(two));
            return (lighter + 0.05) / (darker + 0.05);
        }

        function test_theTextOfTheThemeCanBeRead_data() {
            return [
                        {
                            tag: "text on wood",
                            front: Theme.text,
                            back: Theme.wood
                        },
                        {
                            tag: "gold on wood",
                            front: Theme.gold,
                            back: Theme.wood
                        },
                        {
                            tag: "a price on a card",
                            front: Theme.gold,
                            back: Theme.ironDark
                        },
                        {
                            tag: "the words of the hero on the darkest plank",
                            front: Theme.burnt,
                            back: Qt.rgba(Theme.signWood.r * Theme.darkestPlank, Theme.signWood.g
                                          * Theme.darkestPlank, Theme.signWood.b
                                          * Theme.darkestPlank, 1)
                        },
                        {
                            tag: "an error on wood",
                            front: Theme.danger,
                            back: Theme.wood
                        },
                        {
                            tag: "text on iron",
                            front: Theme.text,
                            back: Theme.iron
                        },
                        {
                            tag: "accent on the dark iron",
                            front: Theme.accent,
                            back: Theme.ironDark
                        },
                        {
                            tag: "accent on iron",
                            front: Theme.accent,
                            back: Theme.iron
                        },
                        {
                            tag: "gold on iron",
                            front: Theme.gold,
                            back: Theme.iron
                        }
                    ];
        }

        // On a desktop the finger is smaller than the smallest unit: the setting must still change it.
        function test_theSizeOfTheControlsScalesTheUnit_data() {
            return [
                        {
                            tag: "desktop",
                            density: 3.8
                        },
                        {
                            tag: "phone",
                            density: 6.3
                        }
                    ];
        }

        function test_theSizeOfTheControlsScalesTheUnit(data) {
            const normal = Theme.hudUnit(data.density, 4000, 1);
            verify(normal >= Theme.smallestUnit);
            const large = Theme.controlScales[GameSettings.Large];
            const small = Theme.controlScales[GameSettings.Small];
            fuzzyCompare(Theme.hudUnit(data.density, 4000, large), normal * large, 0.001);
            fuzzyCompare(Theme.hudUnit(data.density, 4000, small), normal * small, 0.001);
            compare(Theme.controlScales[GameSettings.Normal], 1);
        }

        function test_aNarrowWindowLimitsEvenLargeControls() {
            const width = 480;
            const large = Theme.controlScales[GameSettings.Large];
            compare(Theme.hudUnit(6.3, width, large), width / Theme.unitsAcross);
        }

        // WCAG asks 3 for what is no text.
        function test_theHeartStandsOutOfTheWood() {
            verify(contrast(Theme.heart, Theme.wood) >= 3);
        }

        function test_theIconOfACardStandsOutOfTheWood() {
            verify(contrast(Theme.burnt, Theme.cardWoodDark) >= 3);
        }

        function test_aFrameKeepsTapsFromTheMap() {
            const map = createTemporaryObject(framedMap, stage);
            waitForRendering(map);
            mouseClick(map, 50, 50);
            compare(map.taps, 0);
            mouseClick(map, 150, 150);
            compare(map.taps, 1);
        }

        function test_theTextOfTheThemeCanBeRead(data) {
            verify(contrast(data.front, data.back) >= 4.5, data.tag);
        }
    }
}
