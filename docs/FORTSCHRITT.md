# Fortschritt

## Stand

- **Etappe:** 0 bis 6 sind geprüft und in `dev` (Etappe 2: `37fe03f`, Etappe 3: `3bba48d`). `feature/llvm-update` (`87ceb1a`, E-22), `feature/enums-statt-strings` (`5bb0021`) und `feature/typ-index` (`b1e4ad8`) sind geprüft und in `dev`.
- **In `dev`:** in `qtd_core` Festkomma (E-14), PCG32 (E-15), Inhalte aus JSON mit Validierung (E-16), Tick, Befehl „Turm bauen", Gegner auf Pfad, Welle, Gold, Leben, Sieg und Niederlage (E-17), Zustands-Hash (E-18), Replays (E-20), `TickClock` (E-21). `qtd_headless` mit Karte, Seed, Läufen, Skript, Replay-Prüfung, Aufzeichnung und Benchmark. Fünf Golden Replays. Keine Anbindung an den Client.
- **Dazu aus Etappe 6:** Schadensarten, Resistenzen, Verlangsamung und Gift, Zielprioritäten je Turm, im Client wählbar (E-28).
- **Dazu aus `feature/turm-drehen`:** Türme lassen sich beim Bauen in Vierteln drehen, das HUD zeigt die Form (E-27).
- **Dazu aus Etappe 5:** `QtowerDefense --control` spielt das echte Spiel, Smoke-Test als Stufe der Vollprüfung, Bildrate als Baseline (E-26).
- **Dazu aus `feature/raster`:** Schachbrett-Raster statt Bauplätze, Türme mit Formen, Hindernisse, Pfad auf dem Raster, Turmauswahl im HUD (E-25).
- **Dazu aus Etappe 4:** Eine spielbare Karte im 3D-Client mit Platzhaltern: Kamera, Bauen per Maus und Touch, HUD, Pause, Tempo, Interpolation (E-24).
- **Dazu aus Etappe 3:** `naive` und `fuzz` als Bots, Invarianten, JSON-Steuerschnittstelle über `qtd_headless --control`, Fuzz-Stufe in `verify` (E-23).
- **Letzter voller `verify`:** grün, 14 bestanden, 1 MISSING (Balancing). Coverage Core 99,4 %, Automation 94 %. Benchmark rund 4.800-fache Echtzeit (`docs/Performance-Verlauf.md`).
- **Review von Etappe 2:** Erst nicht bestanden, sieben Befunde (`--max-ticks` zog Befehle vor, Seed des Skripts wurde ignoriert, dazu kleinere). Alle behoben, Nachprüfung bestanden, gemergt ist der geprüfte Commit `075137a`.
- **Review von Etappe 3:** Zwei Runden nicht bestanden: `saveReplay` hing bei NUL und bei rund 32.760 Zeichen im Dateinamen, Ctrl-Z beendete die Sitzung, Invarianten liefen zu selten, der Fehlerpfad hatte keinen Test, dazu doppelte Logik. Alles behoben oder in E-23 und unten begründet, die dritte Runde bestand mit Mutanten (`285e8a2`). Die Coverage von Core maß das Review im frischen Klon mit 882/888 statt 881/888; ungeklärt, ohne Folgen für die Schwelle.
- **Review von Etappe 4:** Erst nicht bestanden: Klicks gingen durch das HUD auf die Karte, im Hintergrund lief das Spiel auf dem Desktop weiter, das HUD überlappte in kleinen Fenstern, die Interpolation fehlte. Alles behoben, Nachprüfung mit Mutanten bestanden (`68a99ac`).
- **cppcheck** analysiert wirklich alle Dateien, siehe E-19.

## Nächster Schritt

1. Etappe 7.

## Bekannte Mängel und offene Punkte

- **Stufen ohne Inhalt (MISSING in `verify`):** Balancing (12).
- **Steuerschnittstelle:** Läuft in `qtd_headless` und im echten Spiel auf Standardein- und -ausgabe (E-26). Die Aktionen zu Regeln, die es noch nicht gibt, fehlen. Schadensarten, Effekte und Resistenzen stehen nicht in der Beobachtung, ein Agent liest sie aus der Kartendatei. Ein localhost-Transport fehlt; falls er kommt, müssen `startMap`, `saveReplay` und `screenshot` auf erlaubte Pfade beschränkt werden. Raster und Kosten stehen nicht in der Beobachtung, ein Agent liest sie aus der Kartendatei.
- **Aus dem Review von Etappe 4, nicht behoben:** Kein Test merkt, wenn `MapInstances` nicht mehr an `frameChanged` hängt (die Gegner sprängen dann wieder mit 30 Hz). `frameChanged` kommt auch in der Pause und nach Spielende.
- **Aus dem Review von `feature/raster`, nicht behoben:** Kein Test sichert, dass der Fuzz-Bot genau einen Wert eines Befehls bricht und dass das Brett keine Fugen hat; beides ist nur von Hand gemessen. Felder sind auf kleinen Displays klein (etwa 3 mm bei 800x360 ohne Zoom). Die Form ist nur im HUD zu sehen, nicht auf der Karte, ein abgelehnter Tipp bleibt stumm, das HUD nennt die rohe ID statt eines übersetzbaren Namens, alle Türme haben eine Farbe. Das gehört zu den Menüs und Assets (Etappen 10 und 11), spätestens zur Bedienung auf Mobile.
- **Aus dem Review von Etappe 6, nicht behoben:** Die Farben von Schadensart, Status und gewähltem Turm prüft kein Test, nur der Blick auf die Screenshots.
- **Client:** Die Verbindung von `GameSession` zu `applicationStateChanged` läuft in keinem Test mit echtem Signal, Windows sendet es nicht. Die Reaktion darauf ist getestet.
- **Fuzz-Prüfung:** Die Zeilen für „abgelehnter Befehl änderte den Zustand" und „Befehl lief nach Spielende" in `headless_runner.cpp` laufen in keinem Test, weil dafür die Simulation fehlerhaft sein müsste. Das Review hat die erste mit einem Mutanten geprüft.
- **Simulation, bewusst offen:** Doppelte Schlüssel im Inhalts-JSON bleiben unerkannt (E-16). `distance()` rundet schon das Quadrat auf vier Stellen, sehr kurze Pfadstücke unter etwa 0,01 Felder werden dadurch ungenau; abstürzen kann das nicht. Lesbare Texte der Ladefehler sind Englisch und noch nicht übersetzbar, sie richten sich an Autoren der Daten.
- **Aus dem Review von Etappe 2, nicht behoben:** `--replay --runs 1` und `--control --runs 1` werden angenommen, obwohl die Doku `--runs` ausschließt; wirkungslos, weil genau ein Lauf gemeint ist. `count_if` in `headless_runner.cpp` ist für Linux und Android gecastet, dort aber nicht kompiliert.
- **Hinweise aus dem Review von Etappe 1, nicht behoben:**
  - Keine Grenze für Listenlängen und Gegnerzahlen: Wellen mit 10^9 Gegnern begrenzen weder Speicher noch Tick-Kosten.
  - `gold` ist ungeprüftes `qint64`. Ein Überlauf bräuchte mindestens 9,2e9 Kills, E-14 gilt also praktisch, nicht streng.
  - `Simulation(Content, seed)` prüft nichts, leere `wave` wäre undefiniert; über `parseContent` nicht erreichbar.
- **moc** verliert sich an Raw-Strings, die Backslash und Anführungszeichen zusammen enthalten: Der Test kompiliert, das Linken scheitert an `metaObject`. Ein solcher Text steht in `tst_content.cpp` als normaler String.
- **Restpunkte der Prüfskripte aus dem Nach-Review, niedrig:**
  - Nirgends eingebundene Header sieht clang-tidy nicht; `--quick` prüft geänderte Header ohne gleichnamige Quelle nicht; `.HPP` fällt durch den `HeaderFilterRegex`.
  - Die Endungen `.ipp .tpp .txx .ixx .cppm .c .c++ .h++` kennt die gemeinsame Liste nicht.
  - `check_layers` übersieht `Qt::Gui`, Links auf `qtd_core` aus anderen CMake-Dateien und `<QTGUI/QIMAGE>`. Der Include-Check fängt normale Gui-Nutzung.
  - Falscher Alarm bei `__has_include(<foo/1.2/bar.h>)` und bei auskommentierten Link-Zeilen.
  - Eine getestete Datei mit Nicht-ASCII-Namen sprengt den OpenCppCoverage-Export (`scripts/coverage.py`).
  - Ohne lokalen Branch `dev` scheitert `check_length` laut, der Hook blockiert dann. `BASELINE_NAME` steht in `scripts/check_length.py` doppelt.
- **MSVC-Absturz C1001, ungeklärt, zweimal gesehen:** Interner Compilerfehler in frischen Klonen, jeweils in Dateien, die das Feature nicht änderte: im Review von `feature/llvm-update` im Coverage-Build (`src/app/main.cpp`, Header `ranges`), im Review von `feature/enums-statt-strings` in `tests/qml/tst_qml.cpp`. Eine Wiederholung ohne Änderung lief jeweils durch, 36 Einzelläufe blieben ohne Absturz. Beide Dateien gehören zum Client, QtCraft lief parallel. Ein Zusammenhang mit LLVM im `PATH` ist nicht ausgeschlossen. Tritt es wieder auf: Lauf wiederholen und hier ergänzen.
- **Auftrag, Sache des Menschen:** `QtowerDefense-Agent-Auftrag.md` nennt für `clang-tidy` und `clang-format` noch den Pfad der VS Build Tools. Gültig ist jetzt E-22.
- **Plugin `qtd-lsp`:** Am 2026-11-04 vom Menschen in einer neuen Sitzung geprüft: Es lädt, `clangd` fand bei fertigem Index alle Verwendungen von `projectVersion`. Bei laufendem Index fehlten Treffer ohne Warnung (Regel in `CLAUDE.md`). `qmlls` wurde dort nicht abgefragt, nur seine LSP-Initialisierung ist geprüft. clangds Hinweis auf ein unbenutztes `<compare>` in `version.h` ist ein Fehlalarm (`operator<=>`).
- **OpenCppCoverage-Setup** hat einen HKLM-Uninstall-Schlüssel und einen Benutzer-`PATH`-Eintrag außerhalb von `C:\dev` hinterlassen (`docs/Building.md`).
- **QmlPreview MCP** ist nicht installiert (Qt Maintenance Tool, Komponente „Agentic Development Tools", macht der Mensch). `qt-docs` und das LSP standen den Subagenten von Etappe 1 nicht zur Verfügung, weil ihre Sitzung vor dem Plugin gestartet war.
- **Inline-Code in Headern** zählt für Coverage nur, wenn eine Übersetzungseinheit ihn nutzt. Der Abgleich aus E-13 erfasst nur Quelldateien.
- `QtowerDefense.exe` startet nur mit Qt auf dem `PATH` (kein Deployment).

## Etappen

| Nr. | Etappe | Status |
|---|---|---|
| 0 | Fundament | fertig, in `dev` |
| 1 | Simulationskern | fertig, in `dev` |
| 2 | Headless und Replays | fertig, in `dev` |
| 3 | Steuerschnittstelle und Bots | fertig, in `dev` |
| 4 | Dünne 3D-Version | fertig, in `dev` |
| 5 | Smoke-Test | fertig, in `dev` |
| 6 | Kampfsystem | fertig, in `dev` |
| 7 | Inhalte Türme und Gegner | offen |
| 8 | Zauber und Helden | offen |
| 9 | Spielfluss | offen |
| 10 | Assets | offen |
| 11 | Modi | offen |
| 12 | Balancing | offen |
| 13 | Feinschliff | offen |
