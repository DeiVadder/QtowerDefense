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
| `Choice` | class | `good_bot.cpp` | The best plan, and the best one the gold pays for now. |
| `Damage` | struct | `good_bot.cpp` | Damage a tower of this level does per tick on each point of the path it reaches, before the resistances: its hits, and its poison apart, because the poisons of two towers do not add up. |
| `GoodBot` | class | `good_bot.cpp` |  |
| `Plan` | struct | `good_bot.cpp` | Something to spend gold on and what it is worth, in damage on the path per gold. |
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
| `GameSession::Cue` | enum class | `game_session.h` | Something a player should hear. |
| `GroundField` | class | `ground_field.h` | The shape and the colors of the ground, as functions of a place in fields: the middle of the cell (x, y) is at (x, y). |
| `GroundField::Exit` | struct | `ground_field.h` | Where a path leaves the grid: it goes straight on from the cell, in the direction. |
| `GroundField::Surface` | struct | `ground_field.h` | What decides the color of a place, without the fine grain. |
| `GroundField::Tint` | struct | `ground_field.h` |  |
| `Landmark` | struct | `landscape.h` | A model that marks a landscape on the world map. |
| `Landscape` | struct | `landscape.h` | How a map looks. |
| `SceneryItem` | struct | `landscape.h` | One kind of scenery. |
| `SceneryZone` | enum class | `landscape.h` | Where a kind of scenery may stand. |
| `LandscapeView` | class | `landscape_view.h` | The landscape of the running map, for the scene. |
| `Piece` | struct | `map_instances.cpp` |  |
| `MapInstances` | class | `map_instances.h` | Draws one layer of a game with a single model: a map has hundreds of cells, and the number of enemies changes with every wave. |
| `MapInstances::Layer` | enum class | `map_instances.h` |  |
| `Cells` | using | `scenery_instances.cpp` |  |
| `Plant` | struct | `scenery_instances.cpp` |  |
| `SceneryInstances` | class | `scenery_instances.h` | The places of one kind of scenery. |
| `FrameCosts` | class | `screen_control.cpp` | What frames cost, apart from how often the window draws them. |
| `ScreenControl::Mailbox` | struct | `screen_control.cpp` | The way from the thread that reads to the control. |
| `ScreenControl::ScreenGame` | class | `screen_control.cpp` | The game on the screen, as the control interface sees it. |
| `ScreenControl` | class | `screen_control.h` | The control interface in the real game: requests play the game on the screen. |
| `Vertex` | struct | `terrain_mesh.cpp` |  |
| `TerrainMesh` | class | `terrain_mesh.h` | The ground as one mesh, in the coordinates of the map: the board and the land around it. |
| `GridCellPicture` | class | `terrain_picture.h` | One cell of the grid that shows where towers go: a thin frame, the rest is clear. |
| `TerrainPicture` | class | `terrain_picture.h` | The colors of the ground. |
| `WorldSession` | class | `world_session.h` | The campaign world for QML: its sections, which are open and the stars won (E-32). |

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
| `BonusCoin` | struct | `simulation.h` | What a defeated enemy leaves behind, for whoever collects it in time. |
| `BuildTowerCommand` | struct | `simulation.h` | Takes a card of this type from the hand if there is one, else pays the full price. |
| `BuyCardCommand` | struct | `simulation.h` | The card of this type, at its full price. |
| `CallWaveCommand` | struct | `simulation.h` | Lets the next wave start at the next tick, for gold. |
| `Card` | struct | `simulation.h` | A tower that is already paid for, whatever the card cost is what selling it counts. |
| `CastSpellCommand` | struct | `simulation.h` |  |
| `CollectCoinCommand` | struct | `simulation.h` |  |
| `Command` | using | `simulation.h` | What a player, human or bot, can ask for. |
| `CommandResult` | enum class | `simulation.h` |  |
| `DiscardCardCommand` | struct | `simulation.h` | Throws a card away, its gold is lost. |
| `Enemy` | struct | `simulation.h` |  |
| `GameMode` | enum class | `simulation.h` | Campaign: the wave comes once, a game is won when it is over. |
| `GameStatus` | enum class | `simulation.h` |  |
| `RollCardCommand` | struct | `simulation.h` | A random card for less than any tower costs. |
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
| `CampaignProgress` | struct | `world.h` | What a player has done in the world: the best stars of each section, 0 if not won yet, and the most rounds of its endless game (E-36), 0 if none. |
| `World` | struct | `world.h` | The sections of the campaign and the ways between them. |
| `WorldLoadResult` | struct | `world.h` | Holds the world, or an error text that names the place of the problem. |
| `WorldSection` | struct | `world.h` | One part of the campaign world (E-32). |
