# Architektur

Stand Etappe 1: Simulationskern in `qtd_core`, noch ohne Anbindung an den Client. Headless, Replays und Steuerschnittstelle folgen ab Etappe 2.

## Module

| Ziel | Ordner | Darf abhängen von |
|---|---|---|
| `qtd_core` | `src/core` | Qt6::Core |
| `qtd_automation` | `src/automation` | `qtd_core`, Qt6::Core, Qt6::Network |
| `qtd_client` (QML-Modul `QtowerDefense.Client`) | `src/client` | alles darunter, Gui, Qml, Quick, Quick3D |
| `QtowerDefense` | `src/app` | `qtd_client` |
| `qtd_headless` | `src/headless` | `qtd_automation`, `qtd_core`, ohne Gui |

`scripts/check_layers.py` prüft das an Includes und `target_link_libraries`, `scripts/check_determinism.py` die Verbote in Core.

## Simulation (`src/core`)

| Datei | Inhalt |
|---|---|
| `fixed` | `Fixed` (Festkomma, E-14), `Point`, `distance`, `sqrt` |
| `random` | `Random` (PCG32, E-15) |
| `content` | `Content`, `Path`, Turm- und Gegnertypen, Spawn-Gruppen: nach dem Laden unveränderlich |
| `content_loader` | `parseContent`, `loadContentFile`: JSON mit Validierung (E-16) |
| `simulation` | `Simulation`, `SimulationState`, `Command`, `CommandResult` (E-17) |
| `state_hash` | `Fnv1a64`, `contentHash`, `stateHash` (E-18) |

Ablauf einer Partie: `parseContent` liefert `Content` oder einen Fehlertext mit Fundstelle, etwa `towers[0].cost: is missing`. `Simulation(content, seed)` startet die Partie. Der Treiber ruft `execute()` zwischen den Ticks und `tick()` im festen Takt, liest `state()` und `hash()`.

Reihenfolge in einem Tick: Gegner laufen (wer das Pfadende erreicht, kostet Leben), die Welle erzeugt Gegner, Türme zählen ihre Abklingzeit herunter und feuern, das Spielende wird geprüft. Ein neuer Gegner steht im Tick seiner Erzeugung auf Weg 0 und kann schon getroffen werden.

## Inhalte

`content/` enthält die Spieldaten, bisher `training_ground.json`. Felder einer Datei:

- `formatVersion` (1), `startGold`, `startLives`
- `path`: mindestens zwei Punkte `{x, y}` in Feldern
- `buildSlots`: Punkte
- `towers`: `id`, `cost`, `range`, `cooldownTicks`, `damageMin`, `damageMax`
- `enemies`: `id`, `health`, `speedPerTick`, `bounty`, `lifeCost`
- `wave`: Gruppen aus `enemy`, `count`, `delayTicks` (Wartezeit vor dem ersten Gegner), `intervalTicks`

Dezimalwerte stehen als Zeichenkette, ganze Zahlen dürfen auch als Zahl stehen (E-16). Befehle und der Zustand sprechen Typen über Indizes an, die IDs dienen nur den Verweisen in den Daten.

## Client

`qtd_client` ist eine statische Bibliothek mit QML-Modul über `qt_add_qml_module()`. Programme und Tests linken `qtd_clientplugin` und nennen `Q_IMPORT_QML_PLUGIN(QtowerDefenseClientPlugin)`. Es gibt genau eine Engine, in `src/app/main.cpp`. `Main.qml` ist das Fenster, `MainScene.qml` die 3D-Szene.

## Startoptionen

- `qtd_headless --build-info`: Programm und Version als JSON. Mehr kann das Programm noch nicht.

## Tests

| Label | Inhalt |
|---|---|
| `core` | QtTest: `tst_core_version`, `_fixed`, `_random`, `_content`, `_state_hash`, `_simulation`. Tests schalten Ticks von Hand weiter; `tests/core/scenario.h` ist die von Hand nachrechenbare Beispielpartie |
| `automation` | QtTest, dazu `qtd_headless --build-info` |
| `client` | `tst_client_window` (rendert das echte Fenster auf der GPU), `tst_qml` (QuickTest) |
| `asan` | `asan_selfcheck`, nur im Preset `asan`: muss mit einem ASan-Bericht sterben |
