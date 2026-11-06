# Typen

Erzeugt von `scripts/check_types.py --write`, nicht von Hand ändern. Vor jedem neuen Typ hier und mit
der LSP-Symbolsuche nachsehen, ob es ihn schon gibt.

## automation

| Typ | Art | Datei | Zweck |
|---|---|---|---|
| `FuzzBot` | class | `bot.cpp` |  |
| `IdleBot` | class | `bot.cpp` |  |
| `NaiveBot` | class | `bot.cpp` |  |
| `Bot` | class | `bot.h` | Decides what to do. |
| `BotKind` | enum class | `bot.h` | The players that need no script. |
| `Action` | enum class | `control_session.cpp` |  |
| `HeadlessGame` | class | `control_session.cpp` | A game without a window, for qtd_headless. |
| `ControlSession` | class | `control_session.h` | The control interface without its transport: one JSON request in, one JSON answer out. |
| `ControlledGame` | class | `control_session.h` | Where a controlled game lives: on its own in qtd_headless, on the screen in the real game. |
| `CommandLine` | struct | `headless_runner.cpp` | The options of the command line. |
| `HeadlessArguments` | struct | `headless_runner.h` | Holds the options, or an error text. |
| `HeadlessOptions` | struct | `headless_runner.h` | What a run of qtd_headless should do, see docs/Architektur.md. |
| `HeadlessOutcome` | struct | `headless_runner.h` | The report as JSON, and the exit code of the program. |
| `StateCheck` | using | `invariants.h` | Judges a state: a text for the first problem, or nothing. |

## client

| Typ | Art | Datei | Zweck |
|---|---|---|---|
| `AppInfo` | class | `app_info.h` | Build information for the QML side. |
| `GameSession` | class | `game_session.h` | The running game for the QML side. |
| `Look` | struct | `map_instances.cpp` |  |
| `Piece` | struct | `map_instances.cpp` |  |
| `MapInstances` | class | `map_instances.h` | Draws one layer of a game with a single model: a map has hundreds of cells, and the number of enemies changes with every wave. |
| `MapInstances::Layer` | enum class | `map_instances.h` |  |
| `FrameCosts` | class | `screen_control.cpp` | What frames cost, apart from how often the window draws them. |
| `ScreenControl::Mailbox` | struct | `screen_control.cpp` | The way from the thread that reads to the control. |
| `ScreenControl::ScreenGame` | class | `screen_control.cpp` | The game on the screen, as the control interface sees it. |
| `ScreenControl` | class | `screen_control.h` | The control interface in the real game: requests play the game on the screen. |

## core

| Typ | Art | Datei | Zweck |
|---|---|---|---|
| `Cell` | struct | `content.h` | A field of the chessboard. |
| `Content` | struct | `content.h` | Everything the data files decide. |
| `DamageType` | enum class | `content.h` |  |
| `EnemyType` | struct | `content.h` |  |
| `Grid` | class | `content.h` | The chessboard of a map, row by row. |
| `Path` | class | `content.h` | A fixed route for enemies: straight segments through at least two different points. |
| `Rotation` | enum class | `content.h` | How far a tower is turned clockwise, around the cell it stands on. |
| `SpawnGroup` | struct | `content.h` | Enemies of one type that enter the path one after the other. |
| `SpellType` | struct | `content.h` | What a player casts at a cell of the map, when it has cooled down. |
| `StatusEffect` | struct | `content.h` | What a hit leaves behind, on top of its damage. |
| `StatusKind` | enum class | `content.h` |  |
| `TargetPriority` | enum class | `content.h` | Which enemy in range a tower fires at. |
| `Terrain` | enum class | `content.h` | What lies on a cell. |
| `TowerLevel` | struct | `content.h` | What a tower does at one level. |
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
| `ResumeResult` | struct | `replay.h` | A game that goes on, or the reason why it cannot. |
| `ActiveStatus` | struct | `simulation.h` | A status effect on an enemy, for the ticks that are left. |
| `BuildTowerCommand` | struct | `simulation.h` |  |
| `CallWaveCommand` | struct | `simulation.h` | Lets the next wave start at the next tick, for gold. |
| `CastSpellCommand` | struct | `simulation.h` |  |
| `Command` | using | `simulation.h` | What a player, human or bot, can ask for. |
| `CommandResult` | enum class | `simulation.h` |  |
| `Enemy` | struct | `simulation.h` |  |
| `GameStatus` | enum class | `simulation.h` |  |
| `SellTowerCommand` | struct | `simulation.h` |  |
| `SetTargetPriorityCommand` | struct | `simulation.h` |  |
| `Simulation` | class | `simulation.h` | One game. |
| `SimulationState` | struct | `simulation.h` | All that changes during a game. |
| `Tower` | struct | `simulation.h` |  |
| `UpgradeTowerCommand` | struct | `simulation.h` |  |
| `Fields` | class | `state_hash.cpp` | Adds the fields of a hash in the given order. |
| `Fnv1a64` | class | `state_hash.h` | FNV-1a with 64 bits. |
| `TickClock` | class | `tick_clock.h` | Turns the time between two frames into ticks. |
| `Version` | struct | `version.h` | A semantic version, ordered by major, minor, then patch number. |
