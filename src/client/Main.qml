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

    MainScene {
        id: gameScene

        anchors.fill: parent
        game: session
        coinUnit: hud.unit
    }

    Hud {
        id: hud

        anchors.fill: parent
        game: session
        scene: gameScene
    }

    GameSounds {
        game: session
    }

    // Last, as objects go in the order of their creation: the views must not outlive their game.
    GameSession {
        id: session

        Component.onCompleted: startNewGame(":/content/mire_siege.json")
    }
}
