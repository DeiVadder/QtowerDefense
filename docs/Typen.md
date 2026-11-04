# Typen

Erzeugt von `scripts/check_types.py --write`, nicht von Hand ändern. Vor jedem neuen Typ hier und mit
der LSP-Symbolsuche nachsehen, ob es ihn schon gibt.

## automation

| Typ | Art | Datei | Zweck |
|---|---|---|---|
| `CommandLine` | struct | `headless_runner.cpp` | The options of the command line. |
| `HeadlessArguments` | struct | `headless_runner.h` | Holds the options, or an error text. |
| `HeadlessBot` | enum class | `headless_runner.h` | Who plays a game without a script. |
| `HeadlessOptions` | struct | `headless_runner.h` | What a run of qtd_headless should do, see docs/Architektur.md. |
| `HeadlessOutcome` | struct | `headless_runner.h` | The report as JSON, and the exit code of the program. |

## client

| Typ | Art | Datei | Zweck |
|---|---|---|---|
| `AppInfo` | class | `app_info.h` | Build information for the QML side. |

## core

| Typ | Art | Datei | Zweck |
|---|---|---|---|
| `Content` | struct | `content.h` | Everything the data files decide. |
| `EnemyType` | struct | `content.h` |  |
| `Path` | class | `content.h` | A fixed route for enemies: straight segments through at least two different points. |
| `SpawnGroup` | struct | `content.h` | Enemies of one type that enter the path one after the other. |
| `TowerType` | struct | `content.h` |  |
| `ContentLoadResult` | struct | `content_loader.h` | Holds the content, or an error text that names the place of the problem. |
| `Fixed` | class | `fixed.h` | Decimal fixed point number: the raw integer counts ten-thousandths, so 1.5 is raw 15000. |
| `FixedRangeError` | class | `fixed.h` | Thrown in Debug builds when a Fixed operation has no valid result. |
| `Point` | struct | `fixed.h` | A point on the map, in tiles. |
| `ObjectReader` | class | `json_reader.h` | Reads one JSON object. |
| `ObjectReader::Items` | enum class | `json_reader.h` |  |
| `Random` | class | `random.h` | PCG32 (XSH-RR, 64 bit state), the generator of pcg-random.org. |
| `Kind` | enum class | `replay.cpp` |  |
| `ResultNames` | using | `replay.cpp` |  |
| `Checkpoint` | struct | `replay.h` | The state hash at the end of a tick number, after the commands of that tick. |
| `RecordedCommand` | struct | `replay.h` | A command as it was given. |
| `Replay` | struct | `replay.h` | A game as data: same content, same seed and same commands must give the same checkpoints (E-20). |
| `ReplayLoadResult` | struct | `replay.h` | Holds the replay, or an error text that names the place of the problem. |
| `ReplayPlayer` | class | `replay.h` | Repeats a replay in steps of any size and compares results and checkpoints on the way. |
| `ReplayRecorder` | class | `replay.h` | Plays a game and writes down all that is needed to repeat it, rejected commands too. |
| `BuildTowerCommand` | struct | `simulation.h` |  |
| `Command` | using | `simulation.h` | What a player, human or bot, can ask for. |
| `CommandResult` | enum class | `simulation.h` |  |
| `Enemy` | struct | `simulation.h` |  |
| `GameStatus` | enum class | `simulation.h` |  |
| `Simulation` | class | `simulation.h` | One game. |
| `SimulationState` | struct | `simulation.h` | All that changes during a game. |
| `Tower` | struct | `simulation.h` |  |
| `Fields` | class | `state_hash.cpp` | Adds the fields of a hash in the given order. |
| `Fnv1a64` | class | `state_hash.h` | FNV-1a with 64 bits. |
| `TickClock` | class | `tick_clock.h` | Turns the time between two frames into ticks. |
| `Version` | struct | `version.h` | A semantic version, ordered by major, minor, then patch number. |
