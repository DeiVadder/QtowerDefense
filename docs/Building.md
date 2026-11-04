# Bauen und Werkzeuge

## Schnellstart

Aus PowerShell (die `.cmd`-Dateien brauchen `.\`):

```
cmd /c ".\scripts\env.cmd && cmake --preset debug && cmake --build --preset debug && ctest --preset debug"
cmd /c ".\scripts\verify.cmd --quick"
cmd /c ".\scripts\verify.cmd"
cmd /c ".\scripts\setup-hooks.cmd"
```

`scripts/env.cmd` setzt MSVC x64 (`vcvarsall`), Qt, die CMake und Ninja aus `C:\Dev\Qt\Tools` und die `QTD_*`-Variablen. Ohne diese Umgebung funktionieren die Presets nicht. Programme liegen in `build/<preset>/bin`; sie finden die Qt-DLLs über den `PATH` aus `env.cmd`.

| Preset | Build-Typ | Zweck |
|---|---|---|
| `debug` | Debug | Alltag, Grundlage für clangd, clang-tidy, qmlls |
| `release` | Release | Optimierter Stand |
| `asan` | RelWithDebInfo | `/fsanitize=address`; CTest nur Labels `core`, `automation`, `asan` |
| `ubsan` | RelWithDebInfo | `clang-cl` mit UBSan im Trap-Modus, nur Core, Automation und `qtd_headless` (E-22) |
| `coverage` | Debug | OpenCppCoverage über `scripts/coverage.py` |

## `scripts/verify`

- `--quick`: Build und Tests (debug), Format, Quellregeln (Länge, Schichten, Determinismus, Selbsttests der Skripte), qmllint, clang-tidy auf geänderten Dateien, Golden Replays.
- voll: zusätzlich clang-tidy gesamt, cppcheck, Release, ASan, UBSan, Coverage, Fuzz, Balancing, Smoke-Test.
- Golden Replays laufen als CTest-Label `golden`. Nach einer gewollten Regeländerung: `cmd /c ".\scripts\env.cmd && python scripts\update_golden.py"`, in einem eigenen, begründeten Commit, dazu den Hash in `tests/CMakeLists.txt` anpassen.
- Jede Stufe endet als PASS, FAIL oder MISSING. MISSING heißt: Das Geprüfte gibt es noch nicht (Fuzz, Balancing, Smoke-Test). Es lässt den Lauf nicht scheitern, wird aber angezeigt und am Ende gezählt.
- Protokolle je Stufe: `build/verify-logs/`.

## Installierte Werkzeuge

Downloads liegen in `C:\dev\Tools\downloads-qtd`. Das LLVM-Archiv (859 MB) wurde nach dem Entpacken entfernt, seine Prüfsumme stammt aus dem GitHub-Release und stimmte beim Herunterladen.

| Werkzeug | Version | Pfad | Quelle | SHA-256 |
|---|---|---|---|---|
| clangd | 23.1.0 | `C:\dev\clangd\clangd_23.1.0\bin\clangd.exe` | https://github.com/clangd/clangd/releases/download/23.1.0/clangd-windows-23.1.0.zip | `23412A240756A162E7B98A282F36AA2A23A88DB5CE16A0CBC4FEF7253768C810` |
| OpenCppCoverage | 0.9.9.0 | `C:\dev\OpenCppCoverage\OpenCppCoverage.exe` | https://github.com/OpenCppCoverage/OpenCppCoverage/releases/download/release-0.9.9.0/OpenCppCoverageSetup-x64-0.9.9.0.exe | `2295A733DA39412C61E4F478677519DD0BB1893D88313CE56B468C9E50517888` |
| LLVM | 23.1.5 | `C:\dev\LLVM\clang+llvm-23.1.5-x86_64-pc-windows-msvc\bin` | https://github.com/llvm/llvm-project/releases/download/llvmorg-23.1.5/clang+llvm-23.1.5-x86_64-pc-windows-msvc.tar.xz | `62FDADA7F6FA2B73856486DFBBDBA3D2C1882991621615BCB55C60207611C6D4` |

clangd und LLVM wurden entpackt und ändern nichts außerhalb ihrer Ordner. Das Setup von OpenCppCoverage ist nicht signiert (offizielles GitHub-Release von 2020).

**Nebenwirkungen des OpenCppCoverage-Setups außerhalb von `C:\dev`:** Der Aufruf mit `/VERYSILENT /CURRENTUSER /DIR=C:\dev\OpenCppCoverage` lief trotzdem als Administrator-Installation und hat
- den Uninstall-Schlüssel `HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\{74933D3C-7641-4FA4-840E-313A4D076D87}_is1` angelegt,
- `C:\dev\OpenCppCoverage` an den Benutzer-`PATH` angehängt.

Der mitgelieferte `vc_redist.x64` endete mit 1638 (neuere Version vorhanden), hat also nichts geändert. Das Projekt braucht beide Einträge nicht, `env.cmd` nennt den vollen Pfad. Beide sind noch vorhanden; ob sie bleiben, entscheidet der Mensch.

**clazy:** weggelassen. Das Projekt hat auf GitHub keine Releases mit Windows-Paketen; es müsste gegen eine passende LLVM-Version selbst gebaut werden.

## Stolperstellen dieser Maschine

- clang-tidy aus den Build Tools hat keine Clang-Ressourcen-Header. Deshalb steht `-D_CRT_USE_BUILTIN_OFFSETOF` in `.clang-tidy`, sonst scheitert `qplugin.h`.
- cppcheck aus Strawberry sucht `std.cfg` in einem Pfad seines Paketierers. `verify.py` startet es deshalb im Ordner `C:\Dev\Strawberry\c\share\Cppcheck\cfg`.
- `.clangd` nennt MSVC- und SDK-Version ausdrücklich. Nach einem MSVC-Update dort anpassen.
- `cmake --preset ...` meldet bei jedem Konfigurieren `Could NOT find Qt6TaskTree` und eine Warnung zu `Qt6QmlAssetDownloaderPrivate`. **Bekannt und harmlos:** `find_package(Qt6 ... Qml)` lädt automatisch die CMake-Pakete aller QML-Plugins, und dieses eine Plugin hängt am Modul TaskTree, das in dieser Qt-Installation fehlt. Das Paket wird nur übersprungen. Das Projekt nutzt weder TaskTree noch den Asset Downloader (kein Treffer in `src` und `tests`), Build und Tests laufen ohne das Modul. Es wird nicht nachinstalliert. Erst wenn das Projekt den Asset Downloader bräuchte, wäre die Warnung ernst zu nehmen.
