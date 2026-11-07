import QtCore
import QtQuick
import QtowerDefense.Client

Window {
    id: window

    enum Page {
        Menu,
        World,
        Game
    }

    // The section of the world that is played, -1 for a map started by other means.
    property int playing: -1
    property bool recorded: false
    property int page: Main.Page.Menu

    // One step back: from the settings to the menu, from the world map to the menu.
    function back() {
        if (page === Main.Page.Menu) {
            menu.back();
        } else if (page === Main.Page.World) {
            page = Main.Page.Menu;
        }
    }

    function applyFullScreen() {
        if (AppInfo.windowed && visible) {
            visibility = gameSettings.fullScreen ? Window.FullScreen : Window.Windowed;
        }
    }

    width: 1280
    height: 720
    minimumWidth: 480
    minimumHeight: 320
    visible: true
    title: AppInfo.windowTitle

    // The desktop has no application states, there a game stops with its window.
    onVisibilityChanged: {
        if (visibility === Window.Minimized || visibility === Window.Hidden) {
            session.paused = true;
        }
    }
    Component.onCompleted: applyFullScreen()

    FrameAnimation {
        running: true
        onTriggered: session.advanceTime(Math.round(frameTime * 1000000))
    }

    // At the top, Back belongs to the platform: on Android it leaves the app.
    Shortcut {
        sequences: ["Esc", "Back"]
        enabled: window.page !== Main.Page.Menu || menu.settingsOpen
        onActivated: window.back()
    }

    MainScene {
        id: gameScene

        anchors.fill: parent
        visible: window.page === Main.Page.Game
        game: session
        coinUnit: hud.unit
    }

    Hud {
        id: hud

        objectName: "hud"
        anchors.fill: parent
        visible: window.page === Main.Page.Game
        game: session
        scene: gameScene
        heroSpeaks: gameSettings.heroSpeaks
        onLeave: window.page = Main.Page.World
    }

    WorldMap {
        objectName: "worldMap"
        anchors.fill: parent
        visible: window.page === Main.Page.World
        world: campaign
        heroSpeaks: gameSettings.heroSpeaks
        onPlay: section => {
            window.playing = section;
            session.startNewGame(campaign.mapFile(section));
        }
        onMenu: window.page = Main.Page.Menu
    }

    MainMenu {
        id: menu

        objectName: "mainMenu"
        anchors.fill: parent
        visible: window.page === Main.Page.Menu
        settings: gameSettings
        onCampaign: window.page = Main.Page.World
        onQuit: Qt.quit()
    }

    GameSounds {
        objectName: "gameSounds"
        game: session
        volume: gameSettings.volume / 100
    }

    GameSettings {
        id: gameSettings

        objectName: "gameSettings"

        location: StandardPaths.writableLocation(StandardPaths.AppDataLocation) + "/settings.ini"
        onFullScreenChanged: window.applyFullScreen()
    }

    // Last, as objects go in the order of their creation: the views must not outlive their game.
    WorldSession {
        id: campaign
    }

    GameSession {
        id: session

        // Whoever starts a map, the window shows it. Another map is no section of the world.
        onMapChanged: {
            window.page = Main.Page.Game;
            window.recorded = false;
            if (window.playing >= 0 && campaign.mapFile(window.playing) !== mapFile) {
                window.playing = -1;
            }
        }
        onStateChanged: {
            if (won && !window.recorded && window.playing >= 0) {
                window.recorded = true;
                campaign.recordGame(window.playing, stars);
            }
        }
    }
}
