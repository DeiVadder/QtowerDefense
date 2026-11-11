# QtowerDefense

Fantasy-Tower-Defense mit Qt 6.12, CMake und Qt Quick 3D. Verbindlich ist [QtowerDefense-Agent-Auftrag.md](QtowerDefense-Agent-Auftrag.md). Den aktuellen Stand und den nächsten Schritt nennt `docs/FORTSCHRITT.md`, Entscheidungen stehen in `docs/Entscheidungen.md`.

## Git und Arbeitsablauf
- Hauptlinie ist `dev`. Nach `main` merged nur der Mensch, Releases sind Tags `v<Version>` auf `main`. Öffentlich auf GitHub als `DeiVadder/QtowerDefense` (E-50). Vom Windows-Rechner geht `dev` nur zum Mac (Remote `mac`, als `desktop/dev`, E-49), nach GitHub pusht nur der Mac mit Freigabe des Menschen.
- **Jede Etappe und jedes Feature bekommt einen eigenen Branch:** `feature/<Etappe>-<kurzname>` oder `feature/<kurzname>`. Nie direkt auf `dev` committen.
- **Kein Merge ohne unabhängiges Review** durch einen neuen Subagenten, der den Code nicht geschrieben hat. Das gilt auch für kleine Änderungen.
- Merge nach `dev` als Squash, ein Commit je Etappe (`Stage <Etappe>: ...`). Der Branch bleibt erhalten.
- **Höchstens ein Subagent, eine Etappe, ein Feature gleichzeitig.** Tokens sparsam einsetzen.
- Den git-Autor nicht ändern. Hooks nicht mit `--no-verify` umgehen.

## Architektur-Regeln
- Schichten: `qtd_core` (nur Qt6::Core) → `qtd_automation` → `qtd_client` → Programme `QtowerDefense` und `qtd_headless`.
- Der Client liest den Zustand und schickt Befehle. Bots und Mensch nutzen dieselben Befehle.
- **Simulation bleibt deterministisch:** fester Tick, Festkomma, eigener Zufallsgenerator, einsträngig. In Core verboten: `float`, `double`, `QHash`, `QRandomGenerator`, `QDateTime`, `std::chrono`.
- Content ist datengetrieben (JSON). Gegner laufen auf festen Pfaden, Türme stehen auf freien Feldern des Rasters (E-25).
- Plattformneutral auch für Mobile: Qt-Funktionalität bevorzugen, alles per Touch bedienbar.
- Eine einzige QML-Engine, QML nur in Modulen über `qt_add_qml_module()`.
- Eigene Inhalte: nichts aus bestehenden Spielen übernehmen. Assets nur CC0, jedes in `docs/ASSETS.md`.
- Code und UI-Texte Englisch, übersetzbar; Prozess-Doku Deutsch.

## Stil
- **`auto` nur, wenn der Typ ohne LSP lesbar ist** (Cast, `make_unique<T>`, Konstruktor in derselben Zeile) oder als Template-Ersatz bei Parametern (abgekürzte Templates, generische Lambdas). Sonst den Typ ausschreiben, auch bei Iteratoren und in `for`-Schleifen. Kein „Almost Always Auto". Wo die Sprache `auto` verlangt (Lambda-Variable, strukturierte Bindung), bleibt es.

- **Erst nachsehen, dann neu anlegen:** Vor jedem neuen Typ `docs/Typen.md` und die LSP-Symbolsuche prüfen. Der Index ist generiert: `python scripts/check_types.py --write`, Hook und `verify` scheitern, wenn er veraltet ist oder ein Typname doppelt vorkommt. Gleiche Bedeutung unter anderem Namen sieht kein Skript, deshalb fragt jedes Review ausdrücklich: Gibt es das schon?
- **Enums statt Strings:** Eine feste Menge von Werten ist im Code ein `enum class`. Strings gibt es nur an der Grenze nach außen (Dateiformat, Kommandozeile, Anzeige), dort werden sie sofort umgewandelt.

## Tests
- Tests ohne Echtzeit: Ticks von Hand weiterschalten. Keine Tautologien.
- Absichtliche Regeländerungen aktualisieren Golden Replays in einem eigenen, begründeten Schritt.
- Coverage-Schwellen werden nie gesenkt, um eine Etappe abzuschließen. Findings werden behoben oder einzeln begründet unterdrückt.

## Werkzeuge
- Alles liegt unter `C:\dev`, auch neue Werkzeuge. Auf diesem Rechner wird **nur** gebaut mit MSVC und der CMake aus `C:\Dev\Qt\Tools`, nicht mit `g++` oder der CMake aus Strawberry.
- Qt: `C:\Dev\Qt\6.12.0\msvc2022_64`. Module nur ergänzen (`aqt` mit `--noarchives`), das Qt teilt sich das Projekt mit QtCraft.
- `clang-tidy`, `clang-format`, `clang-cl`: `C:\dev\LLVM\clang+llvm-23.1.5-x86_64-pc-windows-msvc\bin` (E-22). `cppcheck`: `C:\Dev\Strawberry\c\bin`. `qmllint`, `qmlformat`, `qmlls`: `bin` von Qt.
- **Navigation:** erst LSP (`clangd`, `qmlls`), dann `grep`, ganze Dateien nur wenn nötig. Das LSP gibt es nur in Sitzungen, die nach dem Einrichten des Plugins `qtd-lsp` gestartet wurden.
- **LSP-Referenzsuche kann unvollständig sein:** Der Index entsteht im Hintergrund (`build/debug/.cache`) und verschwindet mit `build/`. Solange er läuft, fehlen Treffer ohne Warnung. Vor Änderungen an einer API deshalb mit `grep` gegenprüfen.
- **Qt-APIs** im MCP `qt-docs` nachschlagen, wenn verfügbar, statt sie zu raten. Für UI-Arbeit gibt es optional den QmlPreview MCP.
- `C:\dev\Claude\QtCraft` nur lesen, nie ändern.

## Befehle
Aus PowerShell mit `cmd /c`, die `.cmd`-Dateien brauchen `.\`. Details: `docs/Building.md`.
- Build und Tests: `cmd /c ".\scripts\env.cmd && cmake --preset debug && cmake --build --preset debug && ctest --preset debug"`
- Einzelner Test: `cmd /c ".\scripts\env.cmd && ctest --preset debug -R tst_core_version"`
- Spiel starten: `cmd /c ".\scripts\env.cmd && build\debug\bin\QtowerDefense.exe"`
- Schnellprüfung (vor jedem Commit-Stapel): `cmd /c ".\scripts\verify.cmd --quick"`
- Vollprüfung (vor Etappen-Abschluss): `cmd /c ".\scripts\verify.cmd"`. Protokolle: `build/verify-logs/`.
- Formatieren: `clang-format -i <Dateien>`, `qmlformat -i <Dateien>` (beide nach `env.cmd` auf dem `PATH`).
- Hook einrichten (einmal je Klon): `cmd /c ".\scripts\setup-hooks.cmd"`
- Presets: `debug`, `release`, `asan`, `ubsan`, `coverage`.
