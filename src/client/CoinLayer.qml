pragma ComponentBehavior: Bound

import QtQuick
import QtowerDefense.Client

// The bonus coins on the map. They jump out of a defeated enemy and lie where it fell, for a tap,
// until the game takes them away. It lies over the map and takes no taps beside the coins.
Item {
    id: root

    required property GameSession game
    required property MainScene scene
    required property real unit

    // After the last enemy no coin counts any more.
    visible: !game.won && !game.lost

    function coinNamed(id: int): Item {
        return children.find(child => child.coinId === id) ?? null;
    }

    Connections {
        target: root.game

        function onCoinDropped(id: int, x: real, y: real, value: int) {
            coin.createObject(root, {
                                  coinId: id,
                                  spotX: x,
                                  spotY: y,
                                  value: value
                              });
        }

        function onCoinGone(id: int) {
            root.coinNamed(id)?.destroy();
        }
    }

    Component {
        id: coin

        Item {
            id: self

            property int coinId
            property real spotX
            property real spotY
            property int value
            property real hop

            function place() {
                const spot = root.scene.blockTopOnScreen(spotX, spotY, 0);
                x = spot.x - width / 2;
                y = spot.y - height / 2 - hop * root.unit * Theme.coinHop;
            }

            objectName: "coin"
            width: root.unit * Theme.coinTap
            height: width
            Component.onCompleted: place()

            // The camera moves, the coin stays on its spot of the map.
            FrameAnimation {
                running: true
                onTriggered: self.place()
            }

            NumberAnimation on hop {
                from: 1
                to: 0
                duration: Theme.coinMillis
                easing.type: Easing.OutBounce
            }

            Rectangle {
                anchors.centerIn: parent
                width: root.unit * Theme.coinSize
                height: width
                radius: width / 2
                color: Theme.gold
                border.color: Theme.burnt
                border.width: Math.max(1, width * Theme.coinRim)

                GameIcon {
                    anchors.centerIn: parent
                    width: parent.width * Theme.coinMark
                    height: width
                    name: "coin"
                    fill: Theme.burnt
                }
            }

            MouseArea {
                anchors.fill: parent
                onPressed: {
                    const spot = Qt.point(self.x + self.width / 2, self.y);
                    if (root.game.collectCoin(self.coinId)) {
                        reward.createObject(root, {
                                                x: spot.x,
                                                y: spot.y,
                                                value: self.value
                                            });
                    }
                }
            }
        }
    }

    Component {
        id: reward

        Text {
            id: gain

            property int value

            objectName: "coinReward"
            text: qsTr("+%1").arg(value)
            color: Theme.gold
            style: Text.Outline
            styleColor: Theme.burnt
            font.family: Theme.face
            font.bold: true
            font.pixelSize: root.unit * Theme.titleText

            NumberAnimation on y {
                to: gain.y - root.unit
                duration: Theme.coinMillis
                onFinished: gain.destroy()
            }
        }
    }
}
