# Architektur

Stand Etappe 2: Simulationskern, Replays und `qtd_headless`, noch ohne Anbindung an den Client. Steuerschnittstelle und Bots folgen ab Etappe 3.

## Module

| Ziel | Ordner | Darf abhängen von |
|---|---|---|
| `qtd_core` | `src/core` | Qt6::Core |
| `qtd_automation` | `src/automation` | `qtd_core`, Qt6::Core, Qt6::Network |
| `qtd_client` (QML-Modul `QtowerDefense.Client`) | `src/client` | alles darunter, Gui, Qml, Quick, Quick3D |
| `QtowerDefense` | `src/app` | `qtd_client` |
| `qtd_headless` | `src/headless` | `qtd_automation`, `qtd_core`, ohne Gui |

Alle Typen stehen im generierten Index `docs/Typen.md`. `scripts/check_layers.py` prüft das an Includes und `target_link_libraries`, `scripts/check_determinism.py` die Verbote in Core.

## Simulation (`src/core`)

| Datei | Inhalt |
|---|---|
| `fixed` | `Fixed` (Festkomma, E-14), `Point`, `distance`, `sqrt` |
| `random` | `Random` (PCG32, E-15) |
| `content` | `Content`, `Path`, Turm- und Gegnertypen, Spawn-Gruppen: nach dem Laden unveränderlich |
| `json_reader` | `ObjectReader`, `parseExactJsonObject`: gemeinsamer Leser für alle Dateiformate (E-16) |
| `content_loader` | `parseContent`, `loadContentFile`: JSON mit Validierung (E-16) |
| `simulation` | `Simulation`, `SimulationState`, `Command`, `CommandResult` (E-17) |
| `state_hash` | `Fnv1a64`, `contentHash`, `stateHash` (E-18) |
| `replay` | `Replay`, `ReplayRecorder`, `ReplayPlayer`, `verifyReplay`, Dateiformat (E-20) |
| `tick_clock` | `TickClock`: Zeit, Tempo und Pause werden zu Ticks, ohne eigene Uhr (E-21) |

Ablauf einer Partie: `parseContent` liefert `Content` oder einen Fehlertext mit Fundstelle, etwa `towers[0].cost: is missing`. `Simulation(content, seed)` startet die Partie. Der Treiber ruft `execute()` zwischen den Ticks und `tick()` im festen Takt, liest `state()` und `hash()`.

Reihenfolge in einem Tick: Gegner laufen (wer das Pfadende erreicht, kostet Leben), die Welle erzeugt Gegner, Türme zählen ihre Abklingzeit herunter und feuern, das Spielende wird geprüft. Ein neuer Gegner steht im Tick seiner Erzeugung auf Weg 0 und kann schon getroffen werden.

## Inhalte

`content/` enthält die Spieldaten: `training_ground.json` und `benchmark_arena.json`, die Standardszene für Messungen (200 Gegner, 30 Bauplätze). Felder einer Datei:

- `formatVersion` (1), `startGold`, `startLives`
- `path`: mindestens zwei Punkte `{x, y}` in Feldern
- `buildSlots`: Punkte
- `towers`: `id`, `cost`, `range`, `cooldownTicks`, `damageMin`, `damageMax`
- `enemies`: `id`, `health`, `speedPerTick`, `bounty`, `lifeCost`
- `wave`: Gruppen aus `enemy`, `count`, `delayTicks` (Wartezeit vor dem ersten Gegner), `intervalTicks`

Dezimalwerte stehen als Zeichenkette, ganze Zahlen dürfen auch als Zahl stehen (E-16). Befehle und der Zustand sprechen Typen über Indizes an, die IDs dienen nur den Verweisen in den Daten.

## Replays

Eine Replay-Datei (`*.replay.json`) enthält `formatVersion` (1), `content` (Dateiname der Inhalte), `seed` (Dezimaltext, 64 Bit), `contentHash`, `commands` und `checkpoints`. Hashes sind 16 kleine Hex-Ziffern.

- `commands`: `tick`, `type` (`buildTower`), `slot`, `towerType`, `result` (`accepted`, `gameOver`, `unknownSlot`, `unknownTowerType`, `slotOccupied`, `notEnoughGold`). Auch abgelehnte Befehle stehen darin.
- `checkpoints`: `tick`, `hash`: der Zustand nach allen Befehlen dieses Ticks. Einer je Sekunde Spielzeit ab Tick 0, der letzte ist das Ende der Aufzeichnung.

Ein **Skript** hat dasselbe Format, `contentHash`, `result` und `checkpoints` dürfen fehlen und bedeuten nichts. `seed` gilt, solange `--seed` ihn nicht ersetzt; `content` dient nur der Orientierung, gespielt wird `--map`. Jedes Replay ist auch ein Skript.

**Golden Replays** liegen in `tests/golden`. Neue Datei: nur `formatVersion`, `content`, `seed` und `commands` schreiben, dann `scripts/update_golden.py` ausführen. Das Skript überschreibt alle Checkpoints und gehört deshalb in einen eigenen, begründeten Commit.

## Client

`qtd_client` ist eine statische Bibliothek mit QML-Modul über `qt_add_qml_module()`. Programme und Tests linken `qtd_clientplugin` und nennen `Q_IMPORT_QML_PLUGIN(QtowerDefenseClientPlugin)`. Es gibt genau eine Engine, in `src/app/main.cpp`. `Main.qml` ist das Fenster, `MainScene.qml` die 3D-Szene.

## Startoptionen

`qtd_headless` schreibt einen JSON-Bericht auf die Standardausgabe. Exit-Code 0 heißt Erfolg, 1 ein Problem im Lauf (Bericht `{"error": ...}` oder `"matches": false`), 2 eine falsche Kommandozeile.

| Option | Bedeutung |
|---|---|
| `--map <Datei>` | Inhaltsdatei, Pflicht |
| `--seed <Zahl>` | Seed des ersten Spiels. Standard: der Seed des Skripts, ohne Skript 1 |
| `--runs <Zahl>` | Anzahl Spiele, die Seeds zählen hoch |
| `--bot <Name>` | Wer spielt, bisher nur `idle` |
| `--script <Datei>` | Spielt die Befehle der Datei |
| `--replay <Datei>` | Prüft ein Replay gegen `--map` bis zu seinem Ende, schließt `--seed`, `--runs`, `--script`, `--record`, `--max-ticks` und `--benchmark` aus |
| `--record <Datei>` | Schreibt das Spiel als Replay, nur für einen Lauf |
| `--output <Datei>` | Schreibt den Bericht zusätzlich in eine Datei |
| `--max-ticks <Zahl>` | Beendet ein Spiel spätestens bei diesem Tick, spätere Befehle entfallen. Standard 1000000 |
| `--benchmark` | Ergänzt gemessene Tick-Dauern, sonst ist der Bericht je Eingabe gleich |
| `--build-info` | Programm und Version als JSON |

## Tests

| Label | Inhalt |
|---|---|
| `core` | QtTest: `tst_core_version`, `_fixed`, `_random`, `_content`, `_state_hash`, `_simulation`, `_tick_clock`, `_replay`. Tests schalten Ticks von Hand weiter; `tests/core/scenario.h` ist die von Hand nachrechenbare Beispielpartie |
| `automation` | QtTest, dazu `qtd_headless --build-info` |
| `golden` | `tst_golden` (festgeschriebene Hashes, Tempo 1x bis 3x mit ungleichen Frames und Pause, Neuaufzeichnung Byte für Byte), `headless_golden_defence` (eigener Prozess, Hash zusätzlich in `tests/CMakeLists.txt`) |
| `client` | `tst_client_window` (rendert das echte Fenster auf der GPU), `tst_qml` (QuickTest) |
| `asan` | `asan_selfcheck`, nur im Preset `asan`: muss mit einem ASan-Bericht sterben |
