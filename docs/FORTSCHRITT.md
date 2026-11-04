# Fortschritt

## Stand

- **Etappe:** 0 (Fundament), Branch `feature/0-fundament`. Gebaut, die Befunde des ersten Reviews sind behoben; wartet auf das Nach-Review und den Merge nach `dev` durch den Koordinator.
- **Review:** erstes Review erfolgt. Behoben: Coverage-Lücke bei ungelinkten Dateien (E-13), Gleitkomma-Literale, relative Includes, Groß- und Kleinschreibung von CMake-Befehlen, gemeinsame Endungsliste, laut scheiterndes `git()`, Zifferntrenner, `bugprone-narrowing-conversions` und begründete Ausschlüsse (E-12), Doku zu `Qt6TaskTree` und `docs/Performance-Verlauf.md`. Das Nach-Review ergab merge-bereit mit Vorbehalt. Danach kamen dazu: Nicht-ASCII-Dateinamen in den Dateichecks, Timeout für `clang-tidy` (ein Prozess hing über 35 Minuten auf einem Kern), Ausgabe ohne Encoding-Abbruch. Jeder Fix hat eine Probe in `scripts/tests/test_checks.py` (51 Tests).
- **Letzter Prüfstand (inkrementeller Build, 2026-11-04):** `verify` voll grün in 68 s (10 bestanden, 4 MISSING). CTest: 5/5 debug, 5/5 release, 4/4 asan. Coverage: Core 100 % (26/26), Automation 100 % (4/4). Probe mit `untested.cpp` (40-Zeilen-Funktion, in und außerhalb von CMake): Gate scheitert in Core und Automation mit rc=1.

## Nächster Schritt

1. Nach-Review von Etappe 0, Squash-Merge nach `dev`.
2. Etappe 1 (Simulationskern) in `feature/1-simulationskern`.

## Bekannte Mängel und offene Punkte

- **Stufen ohne Inhalt (MISSING in `verify`):** Golden Replays (Etappe 2), Fuzz-Bot (3), Smoke-Test mit Screenshot (5), Balancing (12).
- **Coverage ist am Gerüst messbar, aber kaum aussagekräftig:** Core 26 Zeilen, Automation 4 Zeilen. Die Schwellen 85 % und 70 % sind unverändert.
- **UBSan:** nicht vorhanden, `clang-cl` fehlt (E-3). In Etappe 1 neu bewerten.
- **clang-tidy 19.1.5 stürzt parallel gelegentlich ab oder hängt**, Ursache unbekannt; `verify` bricht nach 300 s ab und wiederholt die Datei einzeln, ein zweiter Ausfall lässt die Stufe scheitern (E-8). Ein neueres clang-tidy passend zu `clangd` wäre zu bewerten.
- **Restpunkte der Prüfskripte aus dem Nach-Review, niedrig:**
  - Nirgends eingebundene Header sieht clang-tidy nicht; `--quick` prüft geänderte Header ohne gleichnamige Quelle nicht; `.HPP` fällt durch den `HeaderFilterRegex`.
  - Die Endungen `.ipp .tpp .txx .ixx .cppm .c .c++ .h++` kennt die gemeinsame Liste nicht.
  - `check_layers` übersieht `Qt::Gui`, Links auf `qtd_core` aus anderen CMake-Dateien und `<QTGUI/QIMAGE>`. Der Include-Check fängt normale Gui-Nutzung.
  - Falscher Alarm bei `__has_include(<foo/1.2/bar.h>)` und bei auskommentierten Link-Zeilen.
  - Eine getestete Datei mit Nicht-ASCII-Namen sprengt den OpenCppCoverage-Export (`scripts/coverage.py`).
  - Ohne lokalen Branch `dev` scheitert `check_length` laut, der Hook blockiert dann. `BASELINE_NAME` steht in `scripts/check_length.py` doppelt.
- **Ungeprüft:** Ob Claude Code das Plugin `qtd-lsp` wirklich lädt. Geprüft sind `clangd --check` ohne `vcvarsall` (Qt-Typen und MSVC-STL, 0 Fehler) und eine LSP-Initialisierung von `qmlls`.
- **OpenCppCoverage-Setup** hat einen HKLM-Uninstall-Schlüssel und einen Benutzer-`PATH`-Eintrag außerhalb von `C:\dev` hinterlassen (`docs/Building.md`).
- **QmlPreview MCP** ist nicht installiert (Qt Maintenance Tool, Komponente „Agentic Development Tools", macht der Mensch). Das MCP `qt-docs` stand in der Etappe nicht zur Verfügung.
- **Inline-Code in Headern** zählt für Coverage nur, wenn eine Übersetzungseinheit ihn nutzt. Der Abgleich aus E-13 erfasst nur Quelldateien.
- `qtd_headless` kann nur `--build-info`. `QtowerDefense.exe` startet nur mit Qt auf dem `PATH` (kein Deployment).

## Etappen

| Nr. | Etappe | Status |
|---|---|---|
| 0 | Fundament | gebaut, Review-Befunde behoben, Nach-Review offen |
| 1 | Simulationskern | offen |
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
