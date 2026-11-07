pragma ComponentBehavior: Bound

import QtQuick
import QtTest
import QtowerDefense.Client

// TestCase itself is invisible: what should take input needs a visible parent.
Item {
    id: stage

    width: 640
    height: 360

    Component {
        id: menuComponent

        MainMenu {
            property int campaigns: 0

            anchors.fill: parent
            settings: GameSettings {
                // qmllint disable unqualified
                location: "file:///" + saveFolder + "/menu_settings.ini"
                // qmllint enable unqualified
            }
            onCampaign: ++campaigns
        }
    }

    Component {
        id: worldComponent

        Item {
            id: box

            required property string file
            property alias map: map
            property alias world: campaign

            anchors.fill: parent

            WorldMap {
                id: map

                property int menus: 0

                anchors.fill: parent
                density: 5
                world: campaign
                onMenu: ++menus
            }

            WorldSession {
                id: campaign

                progressFile: box.file
            }
        }
    }

    Component {
        id: soundsComponent

        Item {
            property alias sounds: sounds
            property alias session: session

            GameSounds {
                id: sounds

                game: session
            }

            GameSession {
                id: session

                Component.onCompleted: startNewGame(":/content/training_ground.json")
            }
        }
    }

    TestCase {
        name: "Menu"
        when: windowShown

        function cleanup() {
            stage.width = 640;
            stage.height = 360;
        }

        // Every test of the world map has a progress of its own.
        function worldFile(number) {
            return saveFolder + "/world_" + number + ".json"; // qmllint disable unqualified
        }

        function createMenu() {
            return createTemporaryObject(menuComponent, stage);
        }

        function openSettings() {
            const menu = createMenu();
            tapCenter(findChild(menu, "settingsButton"));
            return menu;
        }

        function tapCenter(item) {
            verify(item.visible, item.objectName + " is seen");
            mouseClick(item, item.width / 2, item.height / 2);
        }

        function test_campaignLeadsToTheWorld() {
            const menu = createMenu();
            tapCenter(findChild(menu, "campaignButton"));
            compare(menu.campaigns, 1);
        }

        // The signs make room for the settings, one step back brings them again.
        function test_settingsOpenAndCloseAgain() {
            const menu = createMenu();
            const panel = findChild(menu, "settingsPanel");
            verify(!panel.visible);
            verify(!menu.back(), "nothing open, nothing to close");
            tapCenter(findChild(menu, "settingsButton"));
            verify(panel.visible);
            verify(!findChild(menu, "campaignButton").visible);
            verify(menu.back());
            verify(!panel.visible);
            tapCenter(findChild(menu, "settingsButton"));
            tapCenter(findChild(panel, "closeSettings"));
            verify(!panel.visible);
            verify(findChild(menu, "campaignButton").visible);
        }

        function test_theSliderSetsTheVolume() {
            const menu = openSettings();
            const slider = findChild(menu, "volumeSlider");
            mouseClick(slider, slider.width / 4, slider.height / 2);
            fuzzyCompare(menu.settings.volume, 25, 1);
            mousePress(slider, slider.width / 2, slider.height / 2);
            mouseMove(slider, slider.width + 50, slider.height / 2);
            mouseRelease(slider, slider.width + 50, slider.height / 2);
            compare(menu.settings.volume, 100, "beyond the end is the end");
            mouseClick(slider, 1, slider.height / 2);
            fuzzyCompare(menu.settings.volume, 0, 1);
        }

        function test_togglesFlipTheirSetting_data() {
            return [
                        {
                            tag: "hero",
                            toggle: "heroSpeaksToggle",
                            setting: "heroSpeaks"
                        },
                        {
                            tag: "full screen",
                            toggle: "fullScreenToggle",
                            setting: "fullScreen"
                        }
                    ];
        }

        function test_togglesFlipTheirSetting(data) {
            const menu = openSettings();
            const before = menu.settings[data.setting];
            tapCenter(findChild(menu, data.toggle));
            compare(menu.settings[data.setting], !before);
            tapCenter(findChild(menu, data.toggle));
            compare(menu.settings[data.setting], before);
        }

        function test_theHeroGreetsAndCanBeSilenced() {
            const menu = createMenu();
            menu.settings.heroSpeaks = true;
            const sign = findChild(menu, "menuSign");
            verify(sign.visible);
            verify(findChild(menu, "menuLine").text.length > 0);
            menu.settings.heroSpeaks = false;
            verify(!sign.visible);
            verify(!findChild(menu, "menuHero").talking);
        }

        function test_theWorldMapLeadsBackToTheMenu() {
            const made = createTemporaryObject(worldComponent, stage, {
                                                   file: worldFile(1)
                                               });
            tapCenter(findChild(made.map, "menuButton"));
            compare(made.map.menus, 1);
        }

        function test_theHeroSpeaksOfTheChosenSection() {
            const made = createTemporaryObject(worldComponent, stage, {
                                                   file: worldFile(2)
                                               });
            const words = findChild(made.map, "worldLine");
            made.world.selected = 0;
            made.map.comment();
            compare(words.text, made.map.landscapeLines.meadow);
            made.world.selected = 1;
            compare(words.text, "Nobody goes into that fog. Not even me.");
            made.map.heroSpeaks = false;
            verify(!findChild(made.map, "worldWords").visible);
        }

        // The speakers follow the setting, also while they play, on the scale of the ear.
        function test_theSoundsFollowTheVolume() {
            const made = createTemporaryObject(soundsComponent, stage);
            made.sounds.volume = 0.5;
            verify(made.session.buildTower(4, 4));
            const speakers = findChild(made.sounds, "speakers").item;
            verify(speakers !== null, "a cue brings the speakers");
            const half = speakers.sounds[0].audioOutput.volume;
            verify(half > 0 && half < 0.5, "half as loud to the ear is less than half the power");
            made.sounds.volume = 1;
            for (const sound of speakers.sounds) {
                compare(sound.audioOutput.volume, 1);
            }
            made.sounds.volume = 0;
            compare(speakers.sounds[4].audioOutput.volume, 0);
        }

        function test_aWonSectionGetsItsOwnWords() {
            const made = createTemporaryObject(worldComponent, stage, {
                                                   file: worldFile(3)
                                               });
            made.world.recordGame(0, 3);
            made.world.selected = 0;
            compare(findChild(made.map, "worldLine").text, "Done that. Perfectly, thanks to me.");
        }

        function test_theArrowKeysMoveTheSlider() {
            const menu = openSettings();
            menu.settings.volume = 50;
            const slider = findChild(menu, "volumeSlider");
            slider.forceActiveFocus();
            keyClick(Qt.Key_Right);
            compare(menu.settings.volume, 50 + Theme.volumeStep);
            keyClick(Qt.Key_Left);
            keyClick(Qt.Key_Left);
            compare(menu.settings.volume, 50 - Theme.volumeStep);
        }

        function test_aVolumeOutOfRangeComesBack() {
            const menu = createMenu();
            menu.settings.volume = 500;
            compare(menu.settings.volume, 100);
            menu.settings.volume = -3;
            compare(menu.settings.volume, 0);
        }

        // Narrow windows give the settings all their width, the slider keeps room for a finger.
        function test_theSettingsFitASmallWindow() {
            stage.width = 480;
            stage.height = 320;
            const menu = openSettings();
            const panel = findChild(menu, "settingsPanel");
            compare(panel.width, menu.width - 2 * menu.margin);
            verify(findChild(menu, "volumeSlider").width >= menu.unit * Theme.settingsTrack);
        }
    }
}
