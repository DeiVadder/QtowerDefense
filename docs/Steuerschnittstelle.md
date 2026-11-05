# Control interface

A local, machine-readable way to play. It is off by default. `qtd_headless --control` turns it on: one JSON request per line on standard input, one JSON answer per line on standard output, until the input ends. The real game will offer the same protocol on localhost.

Every request is an object with an `action`. Every answer has `ok`. With `"ok": false` it also has `error`, a text for people, and nothing has changed. A wrong line never ends the session.

Unknown keys are errors. Numbers in requests must be exact integers. Numbers that JSON cannot carry exactly are text: `seed` in requests, `hash`, `health`, `travelled`, `x` and `y` in answers.

## Actions

| Action | Fields | Effect |
|---|---|---|
| `startMap` | `map` (file), `seed` (decimal digits as text) | Starts a game and drops the current one. If it fails, the current game stays |
| `buildTower` | `x`, `y` (a cell of the grid), `towerType` (index) | Runs the command. The answer adds `result` |
| `advance` | `ticks` (0 to 1000000) | Runs that many ticks, it stops at the end of the game |
| `observe` | none | Reports the state, changes nothing |
| `saveReplay` | `file`, up to 4096 characters | Writes the game so far as a replay |

Everything but `startMap` needs a game, which may have ended.

A rejected command is not an error: the answer has `"ok": true`, a `result` such as `outsideMap`, `cellBlocked`, `cellOccupied` or `notEnoughGold`, and the state stays as it was.

## Observation

All answers with `"ok": true` report the state:

```json
{"ok": true, "status": "running", "tick": 61, "gold": 70, "lives": 5, "hash": "00ab...",
 "wave": {"group": 0, "spawnedInGroup": 1, "countdownTicks": 45},
 "towers": [{"id": 1, "type": 0, "x": 4, "y": 4, "cooldownTicks": 0}],
 "enemies": [{"id": 2, "type": 0, "health": "12", "travelled": "0", "x": "0", "y": "2"}]}
```

`status` is `running`, `won` or `lost`.

## Example

```
{"action":"startMap","map":"content/training_ground.json","seed":"7"}
{"action":"buildTower","x":4,"y":4,"towerType":0}
{"action":"advance","ticks":300}
{"action":"saveReplay","file":"game.replay.json"}
```

## Not yet there

These arrive with their rules and return `unknown action` until then: upgrade, sell, target priority, spells, calling a wave early, saving and loading a game. The screenshot in the real game arrives with the transport on localhost.
