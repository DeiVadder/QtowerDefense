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
    property bool endless: false // the world map offers the endless games of won sections
    property int best: 0 // the most rounds of the played section before this game
    property int page: Main.Page.Menu
    readonly property real controlScale: Theme.controlScales[gameSettings.controlSize]

    // One step back: from the settings to the menu, from the world map to the menu.
    function back() {
        if (page === Main.Page.Menu) {
            menu.back();
        } else if (page === Main.Page.World) {
            page = Main.Page.Menu;
        }
    }

    // Keeps the rounds of an endless game once, when it is lost or left (E-36).
    function keepRounds() {
        if (session.endless && !recorded && playing >= 0) {
            recorded = true;
            campaign.recordRounds(playing, session.round);
        }
    }

    // Without windows of its own (phones, the browser) the game fills the screen or the page (E-47).
    function applyFullScreen() {
        if (!visible) {
            return;
        }
        if (!AppInfo.windowed) {
            visibility = Window.FullScreen;
        } else {
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

    // First of all objects, so that every text is made in the chosen language (E-40).
    Translation {
        objectName: "translation"
        choice: gameSettings.language
    }

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

        objectName: "gameScene"
        anchors.fill: parent
        visible: window.page === Main.Page.Game && !curtain.hidesScene
        game: session
        coinUnit: hud.unit
        quality: gameSettings.quality
    }

    Hud {
        id: hud

        objectName: "hud"
        anchors.fill: parent
        // The controls keep clear of the notch and the rounded corners of a phone, the map not (E-55).
        anchors.leftMargin: window.contentItem.SafeArea.margins.left
        anchors.rightMargin: window.contentItem.SafeArea.margins.right
        visible: window.page === Main.Page.Game
        game: session
        scene: gameScene
        heroSpeaks: gameSettings.heroSpeaks
        guiding: guide.visible
        controlScale: window.controlScale
        best: window.best
        onLeave: {
            window.keepRounds();
            window.page = Main.Page.World;
        }
    }

    // Over the HUD, beside it: what lies under a dropped card stays the HUD's own question.
    Guide {
        id: guide

        anchors.fill: parent
        game: session
        unit: hud.unit
        targets: hud.guideTargets
        // A program plays by itself under --control.
        wanted: !gameSettings.guided && !AppInfo.controlled && hud.visible && !hud.gameOver &&
                !hud.failed

        onFinished: gameSettings.guided = true
    }

    WorldMap {
        objectName: "worldMap"
        anchors.fill: parent
        visible: window.page === Main.Page.World
        world: campaign
        endless: window.endless
        heroSpeaks: gameSettings.heroSpeaks
        controlScale: window.controlScale
        quality: gameSettings.quality
        onPlay: section => {
            window.playing = section;
            session.startNewGame(campaign.mapFile(section), window.endless);
        }
        onMenu: window.page = Main.Page.Menu
    }

    MainMenu {
        id: menu

        objectName: "mainMenu"
        anchors.fill: parent
        visible: window.page === Main.Page.Menu
        settings: gameSettings
        onCampaign: {
            window.endless = false;
            window.page = Main.Page.World;
        }
        onEndless: {
            window.endless = true;
            window.page = Main.Page.World;
        }
        onQuit: Qt.quit()
    }

    // Over all pages.
    SplashScreen {
        id: curtain

        objectName: "splash"
        anchors.fill: parent
        intro: !AppInfo.controlled
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
            if (!AppInfo.controlled) {
                curtain.cover();
            }
            window.page = Main.Page.Game;
            window.recorded = false;
            if (window.playing >= 0 && campaign.mapFile(window.playing) !== mapFile) {
                window.playing = -1;
            }
            window.best = window.playing >= 0 ? campaign.sections[window.playing].best : 0;
        }
        onStateChanged: {
            if (won && !window.recorded && window.playing >= 0) {
                window.recorded = true;
                campaign.recordGame(window.playing, stars);
            }
            if (lost) {
                window.keepRounds();
            }
        }
    }
}
