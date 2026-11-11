pragma ComponentBehavior: Bound

import QtQuick
import QtTest
import QtQuick3D
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
            property int endlessGames: 0

            anchors.fill: parent
            settings: GameSettings {
                // qmllint disable unqualified
                location: "file:///" + saveFolder + "/menu_settings.ini"
                // qmllint enable unqualified
            }
            onCampaign: ++campaigns
            onEndless: ++endlessGames
        }
    }

    Component {
        id: translationComponent

        Translation {}
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

        // Moves the rows of the settings until ITEM is in view, as a finger would.
        function tapRow(menu, item) {
            const rows = findChild(menu, "settingsRows");
            const top = item.mapToItem(rows.contentItem, 0, 0).y;
            rows.contentY = Math.max(0, Math.min(top, rows.contentHeight - rows.height));
            tapCenter(item);
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

        function test_endlessLeadsToTheWorld() {
            const menu = createMenu();
            tapCenter(findChild(menu, "endlessButton"));
            compare(menu.endlessGames, 1);
            compare(menu.campaigns, 0);
        }

        // Four signs still hang inside a wide, low window.
        function test_theLastSignHangsInTheWindow() {
            stage.width = 1280;
            stage.height = 720;
            const menu = createMenu();
            const quit = findChild(menu, "quitButton");
            const last = quit.visible ? quit : findChild(menu, "settingsButton");
            verify(last.mapToItem(menu, 0, last.height).y <= menu.height);
        }

        // In the endless mode only a won section can be played, it shows its best (E-36).
        // With the translation, as in the game: English needs its file for the plurals.
        function test_theEndlessWorldOffersTheWonSections() {
            createTemporaryObject(translationComponent, stage);
            const made = createTemporaryObject(worldComponent, stage, {
                                                   file: worldFile(4)
                                               });
            made.map.endless = true;
            made.world.selected = 0;
            const play = findChild(made.map, "playButton");
            verify(!play.available);
            compare(findChild(made.map, "sectionStatus").text, "Not won yet");
            made.world.recordGame(0, 1);
            made.world.recordRounds(0, 7);
            made.world.selected = 0;
            verify(play.available);
            compare(findChild(made.map, "sectionStatus").text, "Best: 7 rounds");
            made.map.endless = false;
            verify(play.available);
            compare(findChild(made.map, "sectionStatus").text, "");
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
            mouseMove(slider, slider.width + 50, slider.height / 2, -1, Qt.LeftButton);
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
            tapRow(menu, findChild(menu, data.toggle));
            compare(menu.settings[data.setting], !before);
            tapRow(menu, findChild(menu, data.toggle));
            compare(menu.settings[data.setting], before);
        }

        function test_theQualityButtonStepsThroughTheSteps() {
            const menu = openSettings();
            menu.settings.quality = GameSettings.High;
            const button = findChild(menu, "qualityButton");
            compare(button.text, "High");
            tapRow(menu, button);
            compare(menu.settings.quality, GameSettings.Low);
            compare(button.text, "Low");
            tapRow(menu, button);
            compare(menu.settings.quality, GameSettings.Medium);
            compare(button.text, "Medium");
            tapRow(menu, button);
            compare(menu.settings.quality, GameSettings.High);
        }

        function test_aQualityOutOfRangeIsBroughtBack() {
            const menu = createMenu();
            menu.settings.quality = 9;
            compare(menu.settings.quality, GameSettings.High);
            menu.settings.quality = -2;
            compare(menu.settings.quality, GameSettings.Low);
            menu.settings.quality = GameSettings.High;
        }

        // The world map has no shadows, only its edges follow the step.
        function test_theWorldMapFollowsTheQuality() {
            const made = createTemporaryObject(worldComponent, stage, {
                                                   file: worldFile(6)
                                               });
            const environment = findChild(made.map, "worldScene").environment;
            made.map.quality = GameSettings.Low;
            compare(environment.antialiasingMode, SceneEnvironment.NoAA);
            made.map.quality = GameSettings.High;
            compare(environment.antialiasingMode, SceneEnvironment.MSAA);
            compare(environment.antialiasingQuality, SceneEnvironment.High);
        }

        function test_theSizeButtonStepsThroughTheSizes() {
            const menu = openSettings();
            menu.settings.controlSize = GameSettings.Normal;
            const button = findChild(menu, "controlSizeButton");
            compare(button.text, "Normal");
            tapRow(menu, button);
            compare(menu.settings.controlSize, GameSettings.Large);
            compare(button.text, "Large");
            tapRow(menu, button);
            compare(menu.settings.controlSize, GameSettings.Small);
            compare(button.text, "Small");
            tapRow(menu, button);
            compare(menu.settings.controlSize, GameSettings.Normal);
        }

        // A finger moves the rows of a low window, the last one comes into view.
        function test_aLowWindowScrollsTheSettings() {
            stage.width = 480;
            stage.height = 320;
            const menu = openSettings();
            const rows = findChild(menu, "settingsRows");
            verify(rows.interactive, "more rows than room");
            const last = findChild(menu, "fullScreenToggle");
            verify(last.mapToItem(rows, 0, last.height).y > rows.height,
                   "the last row starts below");
            const middle = Qt.point(rows.width / 2, rows.height - 5);
            mouseDrag(rows, middle.x, middle.y, 0, -rows.height + 10);
            rows.cancelFlick();
            verify(rows.contentY > 0, "the rows moved");
        }

        // Up or down on the slider moves the rows and leaves the volume, sideways it sets it.
        function test_aSwipeOverTheSliderMovesTheRows() {
            stage.width = 480;
            stage.height = 320;
            const menu = openSettings();
            menu.settings.volume = 50;
            const rows = findChild(menu, "settingsRows");
            const slider = findChild(menu, "volumeSlider");
            // Step by step, as a finger moves.
            const x = slider.width / 2;
            const finger = touchEvent(slider);
            finger.press(0, slider, x, slider.height / 2).commit();
            for (let step = 1; step <= 10; ++step) {
                finger.move(0, slider, x, slider.height / 2 - step * rows.height / 20).commit();
            }
            finger.release(0, slider, x, slider.height / 2 - rows.height / 2).commit();
            rows.cancelFlick();
            compare(menu.settings.volume, 50);
            verify(rows.contentY > 0, "the rows moved");
            rows.contentY = 0;
            mouseDrag(slider, slider.width / 2, slider.height / 2, slider.width / 4, 0);
            fuzzyCompare(menu.settings.volume, 75, 1);
        }

        // The file belongs to the player, a size out of range comes back into it.
        function test_aSizeOutOfRangeIsBroughtBack() {
            const menu = createMenu();
            menu.settings.controlSize = 7;
            compare(menu.settings.controlSize, GameSettings.Large);
            menu.settings.controlSize = -1;
            compare(menu.settings.controlSize, GameSettings.Small);
            menu.settings.controlSize = GameSettings.Normal;
        }

        // A language names itself in its own words, the system choice in the language that speaks.
        function test_theLanguageButtonStepsThroughTheChoices() {
            const menu = openSettings();
            menu.settings.language = Translation.System;
            const button = findChild(menu, "languageButton");
            compare(button.text, "System");
            tapRow(menu, button);
            compare(menu.settings.language, Translation.English);
            compare(button.text, "English");
            tapRow(menu, button);
            compare(menu.settings.language, Translation.German);
            compare(button.text, "Deutsch");
            tapRow(menu, button);
            compare(menu.settings.language, Translation.System);
        }

        // Texts that are already there change with the language, and back.
        function test_germanTextsReplaceTheEnglishOnes() {
            const translation = createTemporaryObject(translationComponent, stage);
            compare(translation.language, "en", "the system of the tests speaks English");
            const menu = openSettings();
            const close = findChild(menu, "closeSettings");
            compare(close.text, "Close");
            translation.choice = Translation.German;
            compare(translation.language, "de");
            compare(close.text, "Schließen");
            translation.choice = Translation.English;
            compare(close.text, "Close");
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

        // Large controls in a wide and low window: the buttons stay inside the map.
        function test_largeControlsFitALowWorldMap() {
            stage.width = 900;
            stage.height = 320;
            const made = createTemporaryObject(worldComponent, stage, {
                                                   file: worldFile(5)
                                               });
            const normal = made.map.unit;
            made.map.controlScale = Theme.controlScales[GameSettings.Large];
            verify(made.map.unit > normal, made.map.unit + " after " + normal);
            made.world.selected = 0;
            for (const name of ["menuButton", "playButton", "worldSign"]) {
                const item = findChild(made.map, name);
                tryVerify(() => item.visible && item.width > 0, 2000, name);
                const corner = item.mapToItem(made.map, item.width, item.height);
                verify(corner.x <= made.map.width && corner.y <= made.map.height, name);
            }
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

        // In the smallest window the settings stay whole, the slider keeps its width.
        function test_theSettingsFitALowWindow() {
            stage.width = 480;
            stage.height = 320;
            const menu = openSettings();
            const panel = findChild(menu, "settingsPanel");
            verify(panel.y + panel.height <= menu.height);
            verify(findChild(menu, "volumeSlider").width >= menu.unit * Theme.settingsTrack);
            const close = findChild(menu, "closeSettings");
            const corner = close.mapToItem(menu, close.width, close.height);
            verify(corner.x <= menu.width && corner.y <= menu.height);
            // Inside the frame, not on its iron, once the rows are moved to their end.
            const rows = findChild(menu, "settingsRows");
            rows.contentY = Math.max(0, rows.contentHeight - rows.height);
            const last = findChild(menu, "fullScreenToggle");
            const bottom = last.mapToItem(panel, 0, last.height).y;
            verify(bottom <= panel.height - menu.unit * Theme.inset, bottom + " of "
                   + panel.height);
        }

        // Narrow windows give the settings all their width, the slider keeps room for a finger.
        function test_theSettingsFitANarrowWindow() {
            stage.width = 480;
            stage.height = 480;
            const menu = openSettings();
            const panel = findChild(menu, "settingsPanel");
            compare(panel.width, menu.width - 2 * menu.margin);
            verify(findChild(menu, "volumeSlider").width >= menu.unit * Theme.settingsTrack);
        }
    }
}
