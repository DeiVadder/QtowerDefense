import QtCore
import QtowerDefense.Client

// What the player sets in the menu, the language too, kept in an INI file. Main gives it its place in the data folder
// of the app. The file belongs to the player: a loudness out of range is brought back into it.
Settings {
    enum Quality {
        Low,
        Medium,
        High
    }

    enum ControlSize {
        Small,
        Normal,
        Large
    }

    property int volume: 80 // in percent
    property bool heroSpeaks: true
    property bool fullScreen: false // only where windows exist
    property int language: Translation.System
    // Shadows and smooth edges cost a phone more than a desktop (E-45).
    property int quality: AppInfo.windowed ? GameSettings.High : GameSettings.Medium
    // The buttons and texts of the game and the world map.
    property int controlSize: GameSettings.Normal
    property bool guided: false // the first game showed the player around (E-53)

    onVolumeChanged: volume = Math.max(0, Math.min(100, volume))
    onQualityChanged: quality = Math.max(GameSettings.Low, Math.min(GameSettings.High, quality))
    onControlSizeChanged: controlSize = Math.max(GameSettings.Small, Math.min(GameSettings.Large,
                                                                              controlSize))
}
