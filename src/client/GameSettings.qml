import QtCore
import QtowerDefense.Client

// What the player sets in the menu, the language too, kept in an INI file. Main gives it its place in the data folder
// of the app. The file belongs to the player: a loudness out of range is brought back into it.
Settings {
    property int volume: 80 // in percent
    property bool heroSpeaks: true
    property bool fullScreen: false // only where windows exist
    property int language: Translation.System

    onVolumeChanged: volume = Math.max(0, Math.min(100, volume))
}
