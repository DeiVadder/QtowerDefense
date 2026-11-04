import QtQuick
import QtTest
import QtowerDefense.Client

TestCase {
    id: testCase

    name: "MainScene"

    Component {
        id: sceneComponent

        MainScene {
            width: 100
            height: 100
        }
    }

    function test_fitCameraDistance_data() {
        return [
                    {
                        tag: "right angle",
                        extent: 100,
                        fov: 90,
                        distance: 50
                    },
                    {
                        tag: "sixty degrees",
                        extent: 200,
                        fov: 60,
                        distance: 173.20508
                    },
                ];
    }

    function test_fitCameraDistance(data) {
        const scene = createTemporaryObject(sceneComponent, testCase);
        verify(scene !== null);
        fuzzyCompare(scene.fitCameraDistance(data.extent, data.fov), data.distance, 0.001);
    }

    function test_cameraIsAboveTheGround() {
        const scene = createTemporaryObject(sceneComponent, testCase);
        verify(scene.camera !== null);
        verify(scene.camera.y > 0);
    }

    function test_versionHasThreeNumbers() {
        verify(/^\d+\.\d+\.\d+$/.test(AppInfo.version), AppInfo.version);
    }
}
