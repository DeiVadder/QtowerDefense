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
| 6 | 2026-11-05 | 7.523 | 132.922 | 4.430 |
| 7 | 2026-11-05 | 7.594 | 131.668 | 4.388 |
| 8 | 2026-11-06 | 7.595 | 131.652 | 4.388 |
| 9 | 2026-11-06 | 7.495 | 1.334 | 4.447 |

## Echtes Spiel

`scripts/smoke_test.py` misst im Fenster, 1280x720, Release, ohne Vsync, in der Standardszene mit 30 Türmen und 200 Gegnern, 5 Sekunden. Beide Grenzen lassen die Stufe scheitern.

- **Kosten je Bild:** Mittel je Bild, CPU vom Beginn bis zum Ende des Bildes im Render-Thread, GPU aus den Zeitstempeln von Qt (`QSG_RHI_PROFILE=1`, `lastCompletedGpuTime`). Grenze: 2.500 µs je Seite, das entspricht 400 Bildern je Sekunde, soweit es den Render-Thread betrifft. Diese Zahl ist die Vergleichsgröße. Die CPU-Zahl ist Wanduhrzeit des Render-Threads, sie enthält also das Warten in `beginFrame` und beim Present. Ein Bild, das an der GPU hängt, erhöht auch sie.
- **Nicht gemessen: der GUI-Thread.** Er läuft parallel und zählt in den Kosten je Bild nicht mit. Der Benchmark oben deckt nur die Simulation ab, nicht `GameSession::refresh`, `MapInstances` und die QML-Bindings. Wird diese Arbeit teurer, bleibt die Grenze von 2.500 µs still, bis die Bildrate unter 60 fällt.
- **Bildrate:** Ziel aus dem Auftrag: 60 Bilder je Sekunde. Über dem Takt von Qt sagt sie nichts: Die 167 hier sind ein Timer, kein Messwert der Szene, gleich auf D3D11, D3D12 und Vulkan.

Woher der Takt kommt, steht im Quelltext von Qt 6.12: Ohne Vsync treibt die threaded Render-Schleife ihre Animationen mit einem Timer von `int(1000 / Bildwiederholrate)` ms an, bei 143 Hz also 6 ms (`startOrStopAnimationTimer` in `qsgthreadedrenderloop.cpp`). `FrameAnimation` und damit jedes Bild hängen daran. Im Spiel mit Vsync spielt das keine Rolle. Gemessen in der Standardszene, Etappe 6:

| Einstellung | Bilder je Sekunde |
|---|---|
| Standard | 167 |
| `QSG_USE_SIMPLE_ANIMATION_DRIVER=1` (dann begrenzt `requestUpdate` mit 2 ms) | 408 |
| dazu `QT_QPA_UPDATE_IDLE_TIME=0` | 3.975 |

Der Smoke-Test bleibt beim Standard: Bei fast 4.000 Bildern je Sekunde sinkt das Mittel der GPU-Zeit auf 36 µs, vermutlich weil nicht jedes Bild schon einen Zeitstempel hat. Die Zahl ist dort nicht verlässlich.

| Etappe | Datum | CPU je Bild (µs) | GPU je Bild (µs) | Bilder je Sekunde |
|---|---|---|---|---|
| 5 | 2026-11-05 | | | 167 |
| 6 | 2026-11-05 | 204 | 128 | 167 |
| 7 | 2026-11-05 | 210 | 127 | 167 |
| 8 | 2026-11-06 | 228 | 154 | 103 (nachts ohne Eingabe gemessen, Ursache offen) |
| 9 | 2026-11-06 | 261 | 155 | 104 (wie Etappe 8, Ursache offen) |
