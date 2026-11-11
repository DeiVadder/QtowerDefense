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

- `--quick`: Build und Tests (debug), Format, Quellregeln (Länge, Schichten, Determinismus, Typen, Übersetzungen, Selbsttests der Skripte), qmllint, clang-tidy auf geänderten Dateien, Golden Replays.
- voll: zusätzlich clang-tidy gesamt, cppcheck, Release, ASan, UBSan, Coverage, Fuzz, Balancing (`scripts/balancing.py` mit den Korridoren aus `docs/Balancing.md`, E-39), Smoke-Test.
- Golden Replays laufen als CTest-Label `golden`. Nach einer gewollten Regeländerung: `cmd /c ".\scripts\env.cmd && python scripts\update_golden.py"`, in einem eigenen, begründeten Commit, dazu den Hash in `tests/CMakeLists.txt` anpassen.
- Jede Stufe endet als PASS, FAIL oder MISSING. MISSING heißt: Das Geprüfte gibt es noch nicht. Es lässt den Lauf nicht scheitern, wird aber angezeigt und am Ende gezählt.
- Protokolle je Stufe: `build/verify-logs/`.
- Übersetzungen: Neue Texte mit `cmake --build --preset debug --target update_translations` in `src/client/i18n/*.ts` holen und übersetzen (E-40). `scripts/check_translations.py` meldet, was fehlt.

## Andere Plattformen (E-46, E-50)

iOS und Android sind vorbereitet, aber ohne die Werkzeuge nicht geprüft. Android baut mit der Clang des NDK, nicht mit MSVC; das ist die Ausnahme zur Regel in CLAUDE.md. Für Telefone und den Browser wird nur das Spiel gebaut, ohne Tests und `qtd_headless`; macOS und Linux bauen alles. Warnungen brechen dort den Build nicht ab, bis er einmal sauber durchläuft (`QTD_WARNINGS_AS_ERRORS`).

- **macOS** (auf einem Mac, E-50): Qt 6.12 für macOS, `QTD_QT_DIR` darauf, Ninja im `PATH`. `cmake --preset macos && cmake --build --preset macos && ctest --preset macos` baut ein Universal-Bundle `build/macos/bin/QtowerDefense.app` mit Tests. `cmake --install build/macos --prefix <Ordner>` legt ein Bundle mit Qt darin ab (über `macdeployqt`). Zum Weitergeben ohne Apple-Konto ad hoc signieren (`codesign --force --deep --sign -`), mit Konto signieren und notarisieren.
- **Linux** (E-50): Qt 6.12 für Linux (`gcc_64`), `QTD_QT_DIR` darauf. `cmake --preset linux && cmake --build --preset linux && ctest --preset linux`, Paket mit `cmake --install build/linux --prefix <Ordner>`. Bisher nur auf GitHub Actions (Ubuntu 24.04).
- **Windows-Paket:** `cmake --install build/release --prefix <Ordner>` legt das Spiel mit Qt in `<Ordner>/bin` ab (über `windeployqt`). Die Laufzeit von MSVC kommt als Installer `vc_redist.x64.exe` mit, nicht als einzelne DLLs; die README nennt ihn.
- **iOS** (auf einem Mac mit Xcode): Qt 6.12 für iOS in `QTD_QT_IOS_DIR`, Qt für macOS in `QTD_QT_DIR` (Host-Werkzeuge). `cmake --preset ios` erzeugt ein Xcode-Projekt in `build/ios`. Signieren mit dem eigenen Team in Xcode (`DEVELOPMENT_TEAM`).
- **Android** (auch auf diesem Rechner): Qt 6.12 für Android arm64 in `QTD_QT_ANDROID_DIR`, Qt für den Host in `QTD_QT_DIR`, dazu `ANDROID_SDK_ROOT`, `ANDROID_NDK_ROOT` und ein JDK. `cmake --preset android && cmake --build --preset android` ergibt ein APK im Gradle-Ordner unter `build/android/src/app/` (der genaue Name hängt von der Qt-Version ab, ungeprüft). Ohne Schlüssel ist ein Release-APK unsigniert und lässt sich nicht installieren. Für den Play Store braucht es ein AAB und einen eigenen Schlüssel (`QT_ANDROID_KEYSTORE_*`).
- **Browser (WebAssembly, E-47):** Qt 6.12 für WebAssembly (`wasm_singlethread`) in `QTD_QT_WASM_DIR`, Qt für den Host in `QTD_QT_DIR`, Emscripten 5.0.5 aus `emsdk` im Pfad. Hier liegen sie in `C:\Dev\Qt\6.12.0\wasm_singlethread` und `C:\dev\emsdk`. `emsdk_env.bat` erst nach `scripts\env.cmd` und aus `cmd` aufrufen: Aus Git Bash heraus schreibt es Befehle für die Bash statt sie auszuführen. `cmake --preset wasm && cmake --build --preset wasm --target QtowerDefense qtd_web_page` ergibt `index.html` (die eigene Seite aus `src/app/web/index.html.in`, E-51), `.js`, `.wasm`, `qtloader.js` und `qtowerdefense.png` in `build/wasm/bin`; `QtowerDefense.html` ist die schlichte Seite von Qt. Zum Ansehen über einen Webserver öffnen, etwa `python -m http.server` in diesem Ordner, nicht als Datei. Für GitHub Pages baut der Workflow `.github/workflows/build.yml` auf GitHub selbst, hier ist dafür nichts nötig (E-50).
- Neue Werkzeuge gehören nach `C:\dev`. Für Android kämen dort das SDK, das NDK, ein JDK und Qt für Android hinzu; mit `aqt` nur ergänzen (Qt wird mit QtCraft geteilt).

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
- Nach zwei Stunden ohne Eingabe lieferte Vsync einem Fenster kaum noch Bilder, und Tests, die auf Bilder warten, liefen in ihre Zeitgrenze. Vermutlich schläft dann der Bildschirm, bewiesen ist das nicht. `tests/CMakeLists.txt` setzt deshalb für `tst_client_window`, `tst_client_screen_control` und `tst_qml` `QSG_NO_VSYNC=1`, wie der Smoke-Test.
- `balsam` ist ein Fensterprogramm: Mit falschen Argumenten oder `--help` wartet es unsichtbar auf einen Dialog. Die Form ist `balsam -o <Ordner> <Datei>`, `scripts/import_assets.py` setzt deshalb eine Zeitgrenze.

## Tests ohne sichtbare Fenster

`python scripts/run_hidden.py "cmd /c .\build\x.cmd"` startet einen Befehl auf einem eigenen, unsichtbaren Windows-Desktop und beendet ihn samt Kindern nach 30 Minuten (`--timeout <Sekunden>`). Fenster dort bekommen weder Fokus noch Eingaben, die Ausgabe gehört deshalb in eine Datei. `QT_QPA_PLATFORM=offscreen` reicht nicht, dort rendert Qt Quick 3D nicht und die 3D-Tests schlagen fehl.
