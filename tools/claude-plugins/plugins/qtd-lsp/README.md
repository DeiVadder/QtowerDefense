# qtd-lsp

clangd (C++) und qmlls (QML) als LSP-Werkzeuge von Claude Code.

- Beide brauchen einen Debug-Build: `.clangd` zeigt auf `build/debug`, qmlls liest über `-b build/debug` und die von `QT_QML_GENERATE_QMLLS_INI` erzeugte `.qmlls.ini` die QML-Module.
- Claude Code startet clangd ohne `vcvarsall`. Die Pfade zu MSVC-STL und Windows-SDK stehen deshalb in `.clangd` (`/vctoolsdir`, `/winsdkdir`, `/winsdkversion`). Bei einem MSVC-Update dort anpassen.
- Die Pfade zu `clangd.exe` und `qmlls.exe` stehen voll in `.lsp.json`, weil beide nicht auf dem `PATH` liegen.
- Kein Hintergrund-Index in `.claude/worktrees/` und für rcc/qmlcachegen-Ausgaben (`.clangd`). In QtCraft kostete das Stunden CPU.
- Prüfung von Hand: `clangd --check=src/client/app_info.cpp` im Wurzelverzeichnis, ohne `vcvarsall`.
