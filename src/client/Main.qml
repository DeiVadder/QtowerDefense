import QtQuick
import QtowerDefense.Client

Window {
    id: window

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

    FrameAnimation {
        running: true
        onTriggered: session.advanceTime(Math.round(frameTime * 1000000))
    }

    // The section of the world that is played, -1 for a map started by other means.
    property int playing: -1
    property bool recorded: false
    property bool atWorld: true

    MainScene {
        id: gameScene

        anchors.fill: parent
        visible: !window.atWorld
        game: session
        coinUnit: hud.unit
    }

    Hud {
        id: hud

        objectName: "hud"
        anchors.fill: parent
        visible: !window.atWorld
        game: session
        scene: gameScene
        onLeave: window.atWorld = true
    }

    WorldMap {
        objectName: "worldMap"
        anchors.fill: parent
        visible: window.atWorld
        world: campaign
        onPlay: section => {
            window.playing = section;
            session.startNewGame(campaign.mapFile(section));
        }
    }

    GameSounds {
        game: session
    }

    // Last, as objects go in the order of their creation: the views must not outlive their game.
    WorldSession {
        id: campaign
    }

    GameSession {
        id: session

        // Whoever starts a map, the window shows it. Another map is no section of the world.
        onMapChanged: {
            window.atWorld = false;
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
