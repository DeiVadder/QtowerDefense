pragma ComponentBehavior: Bound

import QtQuick

// The settings on a board: the loudness of the sounds, the words of the hero, the language, the
// size of the controls, the quality of the pictures and, where windows exist, the full screen.
// Every change is kept at once.
FramedPanel {
    id: root

    required property GameSettings settings
    readonly property real row: unit * Theme.settingsRow

    signal close

    seed: Theme.settingsSeed

    component Label: Text {
        color: Theme.text
        font.family: Theme.face
        font.bold: true
        font.pixelSize: root.unit * Theme.bodyText
        elide: Text.ElideRight
    }

    // A plate that says On or Off.
    component Toggle: HudButton {
        property bool on: false

        unit: root.unit
        width: root.unit * Theme.settingsToggle
        text: on ? qsTr("On") : qsTr("Off")
        highlighted: on
        Accessible.checkable: true
        Accessible.checked: on
    }

    Label {
        id: title

        anchors.horizontalCenter: parent.horizontalCenter
        y: root.unit * Theme.gap * 2
        text: qsTr("Settings")
        color: Theme.gold
        font.pixelSize: root.unit * Theme.titleText
    }

    HudButton {
        objectName: "closeSettings"
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: root.unit * Theme.gap * 2
        unit: root.unit
        icon: "close"
        text: qsTr("Close")
        onClicked: root.close()
    }

    // Rows placed by hand, they stand where they belong before the first frame. In a low window
    // they scroll, a finger moves them up and down.
    Flickable {
        id: rows

        objectName: "settingsRows"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: title.bottom
        anchors.bottom: parent.bottom
        anchors.margins: root.unit * Theme.endMargin * 2
        contentWidth: width
        contentHeight: (AppInfo.windowed ? 6 : 5) * (root.row + root.unit * Theme.gap)
        clip: true
        interactive: contentHeight > height
        flickableDirection: Flickable.VerticalFlick
        boundsBehavior: Flickable.StopAtBounds

        Item {
            width: parent.width
            height: root.row

            Label {
                id: soundLabel

                anchors.verticalCenter: parent.verticalCenter
                width: Math.min(root.unit * Theme.settingsLabel, parent.width - root.unit * (Theme.settingsTrack
                                                                                             + Theme.settingsPercent
                                                                                             + Theme.gap))
                text: qsTr("Sound")
            }

            Rectangle {
                id: track

                objectName: "volumeSlider"
                anchors.left: soundLabel.right
                anchors.right: percent.left
                anchors.rightMargin: root.unit * Theme.gap
                anchors.verticalCenter: parent.verticalCenter
                height: root.unit * Theme.sliderTrack
                radius: height / 2
                color: Theme.ironDark
                border.color: Theme.iron
                activeFocusOnTab: true
                Accessible.role: Accessible.Slider
                Accessible.name: soundLabel.text + " " + percent.text
                Keys.onLeftPressed: root.settings.volume = Math.max(0, root.settings.volume
                                                                    - Theme.volumeStep)
                Keys.onRightPressed: root.settings.volume = Math.min(100, root.settings.volume
                                                                     + Theme.volumeStep)

                Rectangle {
                    width: parent.width * root.settings.volume / 100
                    height: parent.height
                    radius: parent.radius
                    color: Theme.accent
                }

                Rectangle {
                    x: parent.width * root.settings.volume / 100 - width / 2
                    anchors.verticalCenter: parent.verticalCenter
                    width: root.unit * Theme.sliderKnob
                    height: width
                    radius: width / 2
                    color: Theme.iron
                    border.color: Theme.ironLight
                    border.width: Math.max(Theme.thinnestChosenBorder, root.unit
                                           * Theme.chosenBorder)
                }

                // A finger needs more than the thin track. The drag takes only sideways moves:
                // up or down, the rows of a low window move (E-45).
                Item {
                    function take(x: real) {
                        root.settings.volume = Math.round(Math.max(0, Math.min(1, x / track.width))
                                                          * 100);
                    }

                    anchors.fill: parent
                    anchors.topMargin: -root.row / 2
                    anchors.bottomMargin: -root.row / 2

                    TapHandler {
                        onTapped: eventPoint => parent.take(eventPoint.position.x)
                    }

                    DragHandler {
                        target: null
                        yAxis.enabled: false
                        onActiveChanged: {
                            if (active) {
                                parent.take(centroid.position.x);
                            }
                        }
                        onCentroidChanged: {
                            if (active) {
                                parent.take(centroid.position.x);
                            }
                        }
                    }
                }
            }

            Label {
                id: percent

                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                width: root.unit * Theme.settingsPercent
                horizontalAlignment: Text.AlignRight
                text: qsTr("%1 %").arg(root.settings.volume)
            }
        }

        Item {
            y: (root.row + root.unit * Theme.gap)
            width: parent.width
            height: root.row

            Label {
                anchors.left: parent.left
                anchors.right: heroToggle.left
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Words of the hero")
            }

            Toggle {
                id: heroToggle

                objectName: "heroSpeaksToggle"
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                on: root.settings.heroSpeaks
                onClicked: root.settings.heroSpeaks = !root.settings.heroSpeaks
            }
        }

        Item {
            y: 2 * (root.row + root.unit * Theme.gap)
            width: parent.width
            height: root.row

            Label {
                anchors.left: parent.left
                anchors.right: languageButton.left
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Language")
            }

            // Steps through the choices. A language names itself in its own words.
            HudButton {
                id: languageButton

                objectName: "languageButton"
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                unit: root.unit
                width: root.unit * Theme.settingsChoice
                text: [qsTr("System"), "English", "Deutsch"][root.settings.language] ?? qsTr(
                    "System")
                onClicked: root.settings.language = (root.settings.language + 1) % 3
            }
        }

        Item {
            y: 3 * (root.row + root.unit * Theme.gap)
            width: parent.width
            height: root.row

            Label {
                anchors.left: parent.left
                anchors.right: sizeButton.left
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Size of the controls")
            }

            // The game and the world map, the menu always fits its window.
            HudButton {
                id: sizeButton

                objectName: "controlSizeButton"
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                unit: root.unit
                width: root.unit * Theme.settingsChoice
                text: [qsTr("Small"), qsTr("Normal"), qsTr("Large")][root.settings.controlSize]
                onClicked: root.settings.controlSize = (root.settings.controlSize + 1)
                           % Theme.controlScales.length
            }
        }

        Item {
            y: 4 * (root.row + root.unit * Theme.gap)
            width: parent.width
            height: root.row

            Label {
                anchors.left: parent.left
                anchors.right: qualityButton.left
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Quality of the pictures")
            }

            // Shadows and smooth edges on the map and the world map (E-45).
            HudButton {
                id: qualityButton

                objectName: "qualityButton"
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                unit: root.unit
                width: root.unit * Theme.settingsChoice
                text: [qsTr("Low"), qsTr("Medium"), qsTr("High")][root.settings.quality]
                onClicked: root.settings.quality = (root.settings.quality + 1) % (GameSettings.High
                                                                                  + 1)
            }
        }

        Item {
            y: 5 * (root.row + root.unit * Theme.gap)
            width: parent.width
            height: root.row
            visible: AppInfo.windowed

            Label {
                anchors.left: parent.left
                anchors.right: screenToggle.left
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Full screen")
            }

            Toggle {
                id: screenToggle

                objectName: "fullScreenToggle"
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                on: root.settings.fullScreen
                onClicked: root.settings.fullScreen = !root.settings.fullScreen
            }
        }
    }
}
