# Performance-Verlauf

Benchmarks je Etappe, Vorgabe aus Abschnitt 4 des Auftrags. Ziel Simulation: mindestens das 100-fache der Echtzeit in der Standardszene. Eine Verschlechterung über 10 % wird untersucht.

Standardszene: `content/benchmark_arena.json`, Skript `tests/golden/benchmark_arena.replay.json`, 30 Türme, Welle aus 200 Gegnern, die einzeln nachrücken. Es stehen also nicht ständig alle 200 auf dem Pfad. Messung im Preset `release`, jeweils 5 Läufe, Mittel aus 3 Aufrufen:

```bash
qtd_headless --map content/benchmark_arena.json --seed 7 --script tests/golden/benchmark_arena.replay.json --runs 5 --benchmark
```

| Etappe | Datum | ns je Tick | Ticks je Sekunde | Vielfaches der Echtzeit |
|---|---|---|---|---|
| 2 | 2026-11-04 | 7.021 | 142.400 | 4.748 |
