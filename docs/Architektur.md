# Architektur

Stand Etappe 0: nur das Gerüst. Simulation, Steuerschnittstelle und Dateiformate folgen ab Etappe 1.

## Module

| Ziel | Ordner | Darf abhängen von |
|---|---|---|
| `qtd_core` | `src/core` | Qt6::Core |
| `qtd_automation` | `src/automation` | `qtd_core`, Qt6::Core, Qt6::Network |
| `qtd_client` (QML-Modul `QtowerDefense.Client`) | `src/client` | alles darunter, Gui, Qml, Quick, Quick3D |
| `QtowerDefense` | `src/app` | `qtd_client` |
| `qtd_headless` | `src/headless` | `qtd_automation`, `qtd_core`, ohne Gui |

`scripts/check_layers.py` prüft das an Includes und `target_link_libraries`, `scripts/check_determinism.py` die Verbote in Core.

## Client

`qtd_client` ist eine statische Bibliothek mit QML-Modul über `qt_add_qml_module()`. Programme und Tests linken `qtd_clientplugin` und nennen `Q_IMPORT_QML_PLUGIN(QtowerDefenseClientPlugin)`. Es gibt genau eine Engine, in `src/app/main.cpp`. `Main.qml` ist das Fenster, `MainScene.qml` die 3D-Szene.

## Startoptionen

- `qtd_headless --build-info`: Programm und Version als JSON. Mehr kann das Programm noch nicht.

## Tests

| Label | Inhalt |
|---|---|
| `core`, `automation` | QtTest, dazu `qtd_headless --build-info` |
| `client` | `tst_client_window` (rendert das echte Fenster auf der GPU), `tst_qml` (QuickTest) |
| `asan` | `asan_selfcheck`, nur im Preset `asan`: muss mit einem ASan-Bericht sterben |
