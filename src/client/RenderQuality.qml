import QtQuick
import QtQuick3D

// What a step of the setting for the quality of the pictures turns on (E-45). Both scenes read it, so
// that a step means the same on the map and on the world map.
QtObject {
    property int quality: GameSettings.High

    readonly property bool shadows: quality !== GameSettings.Low
    readonly property int shadowMapQuality: quality === GameSettings.High
                                            ? Light.ShadowMapQualityHigh :
                                              Light.ShadowMapQualityMedium
    // Hard edges cost one look into the shadow map, the soft ones of High four.
    readonly property int softShadowQuality: quality === GameSettings.High ? Light.PCF4 : Light.Hard
    readonly property int antialiasingMode: quality === GameSettings.Low ? SceneEnvironment.NoAA :
                                                                           SceneEnvironment.MSAA
    readonly property int antialiasingQuality: quality === GameSettings.High
                                               ? SceneEnvironment.High : SceneEnvironment.Medium
}
