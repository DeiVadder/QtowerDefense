# Fortschritt

## Stand

- **Etappe:** 1 (Simulationskern), Branch `feature/1-simulationskern`. Gebaut, `verify` voll grün; wartet auf das unabhängige Review und den Merge nach `dev` durch den Koordinator. Etappe 0 ist in `dev`.
- **Gebaut:** in `qtd_core` Festkomma (E-14), PCG32 (E-15), Inhalte aus JSON mit Validierung (E-16, Beispiel `content/training_ground.json`), Tick, Befehl „Turm bauen", Gegner auf Pfad, Welle, Gold, Leben, Sieg und Niederlage (E-17), Zustands-Hash (E-18). Keine Anbindung an den Client.
- **Nachzügler aus Etappe 0:** Der Fund `double` in `größe.cpp` war die Ausgabe der Testdatei aus `test_non_ascii_file_name_is_checked`, kein falscher Treffer. Bei der Prüfung fielen zwei echte Fehler auf und sind behoben: Ein BOM verbarg ein `#include` auf Zeile 1 vor beiden Checks, ein Backslash am Ende eines Zeilenkommentars galt nicht als Fortsetzung.
- **Letzter Prüfstand (inkrementeller Build, 2026-11-04):** `verify` voll grün in 136 s (10 bestanden, 4 MISSING). CTest: 10/10 debug, 10/10 release, 9/9 asan. QtTest in Core: 199 Einträge bestanden. Skripttests: 57. Coverage: Core 99,4 % (628/632), Automation 100 % (4/4). Stichproben mit absichtlich falschem Code (Zufall nicht im Hash, Rundung, Gleichstand beim Ziel, tote Gegner als Ziel, Zahlenprüfung im Lader): jede ließ Tests scheitern.

## Nächster Schritt

1. Unabhängiges Review von Etappe 1, Squash-Merge nach `dev`.
2. Etappe 2 (Headless und Replays) in `feature/2-headless-replays`.

## Bekannte Mängel und offene Punkte

- **Stufen ohne Inhalt (MISSING in `verify`):** Golden Replays (Etappe 2), Fuzz-Bot (3), Smoke-Test mit Screenshot (5), Balancing (12).
- **Coverage in Automation** misst nur 4 Zeilen und sagt noch wenig aus. Die Schwellen 85 % und 70 % sind unverändert.
- **UBSan:** nicht vorhanden, `clang-cl` fehlt (E-3). Wird ein eigenes Feature zusammen mit einem neueren LLVM. Bis dahin prüft `Fixed` Überläufe in Debug-Builds (E-14).
- **Simulation, bewusst offen:** Doppelte Schlüssel im Inhalts-JSON bleiben unerkannt (E-16). `distance()` rundet schon das Quadrat auf vier Stellen, sehr kurze Pfadstücke unter etwa 0,01 Felder werden dadurch ungenau; abstürzen kann das nicht. Lesbare Texte der Ladefehler sind Englisch und noch nicht übersetzbar, sie richten sich an Autoren der Daten.
- **Hinweise aus dem Review von Etappe 1, nicht behoben:**
  - Kein festgeschriebener Hash-Wert: Tests vergleichen nur Läufe im selben Prozess. Golden Replays in Etappe 2 müssen das schließen; `pacingDoesNotMatter` vergleicht nur `tick()` mit `advance()`.
  - `increment` des Zufallsgenerators ist im Hash, aber kein Test fängt sein Fehlen (der Stream ist fest 54, also kein Verlaufsrisiko).
  - Ein Pfad kann die Länge 0 haben, wenn Stücke unter etwa 0,01 Felder runden; das stürzt nicht ab, sollte der Lader aber ablehnen.
  - Keine Grenze für Listenlängen und Gegnerzahlen: `content_loader.cpp` prüft Bauplätze quadratisch (40.000 Plätze 2,8 s in Debug), Wellen mit 10^9 Gegnern begrenzen weder Speicher noch Tick-Kosten.
  - `gold` ist ungeprüftes `qint64`. Ein Überlauf bräuchte mindestens 9,2e9 Kills, E-14 gilt also praktisch, nicht streng.
  - `Simulation(Content, seed)` prüft nichts, leere `wave` wäre undefiniert; über `parseContent` nicht erreichbar.
- **moc** verliert sich an Raw-Strings, die Backslash und Anführungszeichen zusammen enthalten: Der Test kompiliert, das Linken scheitert an `metaObject`. Ein solcher Text steht in `tst_content.cpp` als normaler String.
- **Offen aus E-12:** Die dort für Etappe 1 vorgesehene neue Sicht auf die clang-tidy-Checks mit 0 Treffern ist nicht erfolgt.
- **Kein Benchmark:** `qtd_headless` kann noch keine Simulation laufen lassen, die Tick-Dauer ist ungemessen.
- **clang-tidy 19.1.5 stürzt parallel gelegentlich ab oder hängt**, Ursache unbekannt; `verify` bricht nach 300 s ab und wiederholt die Datei einzeln, ein zweiter Ausfall lässt die Stufe scheitern (E-8). Ein neueres clang-tidy passend zu `clangd` wäre zu bewerten.
- **Restpunkte der Prüfskripte aus dem Nach-Review, niedrig:**
  - Nirgends eingebundene Header sieht clang-tidy nicht; `--quick` prüft geänderte Header ohne gleichnamige Quelle nicht; `.HPP` fällt durch den `HeaderFilterRegex`.
  - Die Endungen `.ipp .tpp .txx .ixx .cppm .c .c++ .h++` kennt die gemeinsame Liste nicht.
  - `check_layers` übersieht `Qt::Gui`, Links auf `qtd_core` aus anderen CMake-Dateien und `<QTGUI/QIMAGE>`. Der Include-Check fängt normale Gui-Nutzung.
  - Falscher Alarm bei `__has_include(<foo/1.2/bar.h>)` und bei auskommentierten Link-Zeilen.
  - Eine getestete Datei mit Nicht-ASCII-Namen sprengt den OpenCppCoverage-Export (`scripts/coverage.py`).
  - Ohne lokalen Branch `dev` scheitert `check_length` laut, der Hook blockiert dann. `BASELINE_NAME` steht in `scripts/check_length.py` doppelt.
- **Plugin `qtd-lsp`:** Am 2026-11-04 vom Menschen in einer neuen Sitzung geprüft: Es lädt, `clangd` fand bei fertigem Index alle Verwendungen von `projectVersion`. Bei laufendem Index fehlten Treffer ohne Warnung (Regel in `CLAUDE.md`). `qmlls` wurde dort nicht abgefragt, nur seine LSP-Initialisierung ist geprüft. clangds Hinweis auf ein unbenutztes `<compare>` in `version.h` ist ein Fehlalarm (`operator<=>`).
- **OpenCppCoverage-Setup** hat einen HKLM-Uninstall-Schlüssel und einen Benutzer-`PATH`-Eintrag außerhalb von `C:\dev` hinterlassen (`docs/Building.md`).
- **QmlPreview MCP** ist nicht installiert (Qt Maintenance Tool, Komponente „Agentic Development Tools", macht der Mensch). `qt-docs` und das LSP standen den Subagenten von Etappe 1 nicht zur Verfügung, weil ihre Sitzung vor dem Plugin gestartet war.
- **Inline-Code in Headern** zählt für Coverage nur, wenn eine Übersetzungseinheit ihn nutzt. Der Abgleich aus E-13 erfasst nur Quelldateien.
- `qtd_headless` kann nur `--build-info`. `QtowerDefense.exe` startet nur mit Qt auf dem `PATH` (kein Deployment).

## Etappen

| Nr. | Etappe | Status |
|---|---|---|
| 0 | Fundament | fertig, in `dev` |
| 1 | Simulationskern | gebaut, `verify` grün, Review offen |
| 2 | Headless und Replays | offen |
| 3 | Steuerschnittstelle und Bots | offen |
| 4 | Dünne 3D-Version | offen |
| 5 | Smoke-Test | offen |
| 6 | Kampfsystem | offen |
| 7 | Inhalte Türme und Gegner | offen |
| 8 | Zauber und Helden | offen |
| 9 | Spielfluss | offen |
| 10 | Assets | offen |
| 11 | Modi | offen |
| 12 | Balancing | offen |
| 13 | Feinschliff | offen |
