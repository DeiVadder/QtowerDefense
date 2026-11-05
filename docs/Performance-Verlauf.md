# Performance-Verlauf

Benchmarks je Etappe, Vorgabe aus Abschnitt 4 des Auftrags. Ziel Simulation: mindestens das 100-fache der Echtzeit in der Standardszene. Eine Verschlechterung über 10 % wird untersucht.

Standardszene: `content/benchmark_arena.json`, Skript `tests/golden/benchmark_arena.replay.json`, 30 Türme, Welle aus 200 Gegnern, die einzeln nachrücken. Es stehen also nicht ständig alle 200 auf dem Pfad. Messung im Preset `release`, jeweils 5 Läufe, Mittel aus 3 Aufrufen:

```bash
qtd_headless --map content/benchmark_arena.json --seed 7 --script tests/golden/benchmark_arena.replay.json --runs 5 --benchmark
```

| Etappe | Datum | ns je Tick | Ticks je Sekunde | Vielfaches der Echtzeit |
|---|---|---|---|---|
| 2 | 2026-11-04 | 7.021 | 142.400 | 4.748 |
| 3 | 2026-11-04 | 6.961 | 143.640 | 4.787 |
| 4 | 2026-11-05 | 6.925 | 144.400 | 4.813 |
| 5 | 2026-11-05 | 7.111 | 140.620 | 4.687 |

## Echtes Spiel

`scripts/smoke_test.py` misst die Bildrate im Fenster, 1280x720, Release, ohne Vsync, in der Standardszene mit 30 Türmen und 200 Gegnern, 5 Sekunden. Ziel aus dem Auftrag: 60 Bilder je Sekunde, darunter scheitert die Stufe.

| Etappe | Datum | Bilder je Sekunde |
|---|---|---|
| 5 | 2026-11-05 | 167 |
