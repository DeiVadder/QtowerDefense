# Control interface

A local, machine-readable way to play. It is off by default. `qtd_headless --control` and `QtowerDefense --control` turn it on: one JSON request per line on standard input, one JSON answer per line on standard output, until the input ends.

Every request is an object with an `action`. Every answer has `ok`. With `"ok": false` it also has `error`, a text for people, and nothing has changed. A wrong line never ends the session.

Unknown keys are errors. Numbers in requests must be exact integers. Numbers that JSON cannot carry exactly are text: `seed` in requests, `hash`, `health`, `travelled`, `strength`, `x` and `y` in answers.

## Actions

| Action | Fields | Effect |
|---|---|---|
| `startMap` | `map` (file), `seed` (decimal digits as text) | Starts a game and drops the current one. If it fails, the current game stays |
| `buildTower` | `x`, `y` (a cell of the grid), `towerType` (index), optional `rotation` (clockwise quarter turns around that cell, 0 to 3, default 0) | Runs the command. The answer adds `result` |
| `setTargetPriority` | `tower` (the id of a tower in the game), `priority` (`first`, `last`, `strongest` or `nearest`) | Runs the command. The answer adds `result` |
| `upgradeTower` | `tower` (the id of a tower in the game) | Runs the command. The answer adds `result`, `highestLevel` if there is no further level |
| `sellTower` | `tower` (the id of a tower in the game) | Runs the command. The answer adds `result` |
| `castSpell` | `spell` (an index into the spells of the map), `x`, `y` (the cell at the centre) | Runs the command. The answer adds `result`, `spellCooling` while the spell is not ready |
| `advance` | `ticks` (0 to 1000000) | Runs that many ticks, it stops at the end of the game |
| `observe` | none | Reports the state, changes nothing |
| `callWave` | none | Runs the command: the next wave comes at the next tick, for gold. The answer adds `result`, `noWaveWaiting` while no wave waits |
| `rollCard` | none | Runs the command: a random card for `rollCost` gold. The answer adds `result`, `handFull` with five cards |
| `buyCard` | `towerType` (index) | Runs the command: that card at the full price of the tower. The answer adds `result` |
| `discardCard` | `slot` (an index into `hand`) | Runs the command: the card is gone, its gold too. The answer adds `result`, `unknownCard` without such a card |
| `collectCoin` | `coin` (the id of a coin) | Runs the command: the coin pays its `value`. The answer adds `result`, `unknownCoin` once it is gone |
| `saveReplay` | `file`, up to 4096 characters | Writes the game so far as a replay. This is also how a game is saved |
| `loadGame` | `map` (file), `file` | Goes on with a saved game and drops the current one. Needs no running game. If it fails, the current game stays |
| `screenshot` | `file`, up to 4096 characters | Real game only. Saves the window as a PNG. The answer adds `screenshot` with `width`, `height` and `colors`, the number of different colours at 32 x 32 points |
| `pause`, `resume` | none | Real game only. Stops or starts real time |
| `quit` | none | Real game only. Ends the program after the answer |

Everything but `startMap`, `loadGame` and `quit` needs a game, which may have ended.

A rejected command is not an error: the answer has `"ok": true`, a `result` such as `outsideMap`, `cellBlocked`, `cellOccupied`, `notEnoughGold` or `unknownTower`, and the state stays as it was.

## Observation

All answers with `"ok": true` report the state:

```json
{"ok": true, "status": "running", "tick": 61, "gold": 70, "lives": 5, "hash": "00ab...",
 "wave": {"group": 0, "spawnedInGroup": 1, "countdownTicks": 45, "callBonus": 0},
 "towers": [{"id": 1, "type": 0, "x": 4, "y": 4, "cooldownTicks": 0, "rotation": 0, "priority": "first", "level": 0, "sellValue": 35}],
 "spells": [{"cooldownTicks": 0}, {"cooldownTicks": 0}],
 "enemies": [{"id": 2, "type": 0, "health": "12", "travelled": "0", "x": "0", "y": "2",
              "slow": {"ticks": 0, "strength": "0"}, "poison": {"ticks": 0, "strength": "0"}}]}
```

`status` is `running`, `won` or `lost`.

## In the real game

The requests play the game on the screen. The game starts with time standing still, and `startMap` stops it again: only `advance` moves the game until `resume` lets it run. The observation adds `frameCosts` with `frames`, `cpuMicroseconds` and `gpuMicroseconds`, sums over all frames so far (the GPU time needs `QSG_RHI_PROFILE=1`, the CPU time is the wall clock of the render thread, including its waits, and a screenshot adds GPU time without a frame, so compare sums from before a screenshot), and `paused` and `frames`, the number of frames shown so far. As without a window, everything but `startMap` and `quit` needs a game from `startMap`: the game that the window opened with has a seed of its own. The end of the input ends the program. Qt writes its messages to the debugger unless `QT_FORCE_STDERR_LOGGING=1` is set.

## Example

```
{"action":"startMap","map":"content/training_ground.json","seed":"7"}
{"action":"buildTower","x":4,"y":4,"towerType":0}
{"action":"advance","ticks":300}
{"action":"saveReplay","file":"game.replay.json"}
```

## Not yet there

These arrive with their rules and return `unknown action` until then: upgrade, sell, spells, calling a wave early, saving and loading a game.
