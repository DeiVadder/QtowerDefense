# Fortschritt

## Stand

- **Etappe:** 0 bis 11 sind geprüft und in `dev` (Etappe 2: `37fe03f`, Etappe 3: `3bba48d`). `feature/llvm-update` (`87ceb1a`, E-22), `feature/enums-statt-strings` (`5bb0021`) und `feature/typ-index` (`b1e4ad8`) sind geprüft und in `dev`.
- **In `dev`:** in `qtd_core` Festkomma (E-14), PCG32 (E-15), Inhalte aus JSON mit Validierung (E-16), Tick, Befehl „Turm bauen", Gegner auf Pfad, Welle, Gold, Leben, Sieg und Niederlage (E-17), Zustands-Hash (E-18), Replays (E-20), `TickClock` (E-21). `qtd_headless` mit Karte, Seed, Läufen, Skript, Replay-Prüfung, Aufzeichnung und Benchmark. Neun Golden Replays. Keine Anbindung an den Client.
- **Dazu aus `feature/12-guter-bot`:** Der gute Bot (`--bot good`) und `qtd_headless --statistics` mit Gold je Turmtyp und Goldkurve (E-38). Beide Bots gewinnen noch jede Karte.
- **Dazu aus `feature/12-katalog`:** Türme, Gegner und Zauber der Kampagnenkarten in einem gemeinsamen `content/catalog.json` (E-37). Hashes und Golden Replays unverändert.
- **Dazu aus `feature/11-endlos-ui`:** Schild „Endless" im Menü, Weltkarte im Endlosmodus mit Bestwert je Abschnitt, Runde im HUD, Runden und Bestwert auf der Endtafel (E-36).
- **Dazu aus `feature/11-endlos-kern`:** Endlosmodus in Core: die Welle kommt immer wieder und jedes Mal stärker, kein Sieg, gezählt werden Runden. Im Replay, in `qtd_headless --mode` und in `startMap` (E-36).
- **Dazu aus `feature/11-menue`:** Das Spiel öffnet im Hauptmenü wie Demo A mit Kampagne, Einstellungen und Beenden. Einstellungen für Lautstärke, Sprüche des Helden und Vollbild bleiben in `settings.ini`. Der Held kommentiert auf der Weltkarte den gewählten Abschnitt (E-35).
- **Dazu aus `feature/11-portrait`:** Portrait wie Demo A auf dem Eisenschild, Sprüche auf einem Hängeschild, Pause und Tempo im Bogen um das Portrait (E-34).
- **Dazu aus `feature/11-karten-welt`:** Sieben neue Karten, je eine für Wald, Heide, Ödland, Hochland, Hügelgräber, Frost und Aschenland. Alle neun Abschnitte der Weltkarte sind spielbar (E-32). Noch zu leicht, Balancing folgt in Etappe 12.
- **Dazu aus `feature/11-weltkarte`:** Das Spiel öffnet auf einer Weltkarte im Nebel mit neun Abschnitten, Sterne und Freischalten nach E-32, Fortschritt in `progress.json`.
- **Dazu aus `feature/11-dice`:** Hand mit höchstens fünf bezahlten Karten, Würfel, Laden und Papierkorb, Bonusmünzen auf der Karte, alles als Befehle in Core (E-34). Die Hand beginnt leer.
- **Dazu aus `feature/11-karten`:** Türme als aufgefächerte Karten mit Werten, Ziehen auf die Karte oder Tippen, Raster beim Ziehen deutlich, Karte fällt auf ihr Feld (E-34).
- **Dazu aus `feature/11-held`:** Portrait mit Blinzeln, Blick aufs Brett, Stimmungen und Sprechblase, der Held hält den Zauber in der Hand (E-34). Die Sprachausgabe fehlt noch.
- **Dazu aus `feature/11-theme`:** `Theme.qml` mit Neon als Akzent, HUD mit Icons und Rahmen aus Holz und Eisen (E-34). Ein Menü gibt es noch nicht.
- **Dazu aus `feature/11-spielwelt`:** Gelände, Nebel, Schatten, Pflanzen und Wege statt Schachbrett, Landschaften Wiese und Sumpf (E-34). Die übrigen sieben Landschaften kommen mit ihren Karten.
- **Dazu aus Etappe 10:** Türme, Hindernisse und Gegner als Modelle (Kenney, CC0), Schüsse, Einschlag der Zauber, Sound für Bauen, Upgrade, Verkauf, Zauber, Welle und Spielende (E-33). Hintergrundmusik fehlt.
- **Dazu aus Etappe 9:** Wellen vorziehen gegen Bonusgold, Speichern und Laden eines laufenden Spiels als Replay, von Hand in der Pause und beim Gang in den Hintergrund, auch über die Steuerschnittstelle (E-31).
- **Dazu aus Etappe 8:** Zauber `meteor` und `deep_freeze` mit Abklingzeit, im HUD wählbar, auf Bots und Steuerschnittstelle (E-30).
- **Dazu aus Etappe 7:** Sieben Turmtypen mit je zwei Upgrades, Verkauf, Flächenschaden (`catapult`), acht Gegnertypen und zwei Bosse, Regeneration, neue Karte `mire_siege`, Upgrade und Verkauf im HUD (E-29).
- **Dazu aus `feature/bildzeit`:** Der Smoke-Test misst die Kosten je Bild auf CPU und GPU, die 167 Bilder je Sekunde sind als Takt von Qt erklärt (`docs/Performance-Verlauf.md`).
- **Dazu aus Etappe 6:** Schadensarten, Resistenzen, Verlangsamung und Gift, Zielprioritäten je Turm, im Client wählbar (E-28).
- **Dazu aus `feature/turm-drehen`:** Türme lassen sich beim Bauen in Vierteln drehen, das HUD zeigt die Form (E-27).
- **Dazu aus Etappe 5:** `QtowerDefense --control` spielt das echte Spiel, Smoke-Test als Stufe der Vollprüfung, Bildrate als Baseline (E-26).
- **Dazu aus `feature/raster`:** Schachbrett-Raster statt Bauplätze, Türme mit Formen, Hindernisse, Pfad auf dem Raster, Turmauswahl im HUD (E-25).
- **Dazu aus Etappe 4:** Eine spielbare Karte im 3D-Client mit Platzhaltern: Kamera, Bauen per Maus und Touch, HUD, Pause, Tempo, Interpolation (E-24).
- **Dazu aus Etappe 3:** `naive` und `fuzz` als Bots, Invarianten, JSON-Steuerschnittstelle über `qtd_headless --control`, Fuzz-Stufe in `verify` (E-23).
- **Letzter voller `verify`:** grün, 14 bestanden, 1 MISSING (Balancing). Coverage Core 99,3 %, Automation 95,2 % (Etappe 11, 2026-11-07). Hängt die Coverage-Stufe, bremst oft die Speicherintegrität von Windows OpenCppCoverage aus (2026-11-07). Benchmark rund 4.400-fache Echtzeit (`docs/Performance-Verlauf.md`).
- **Review von Etappe 2:** Erst nicht bestanden, sieben Befunde (`--max-ticks` zog Befehle vor, Seed des Skripts wurde ignoriert, dazu kleinere). Alle behoben, Nachprüfung bestanden, gemergt ist der geprüfte Commit `075137a`.
- **Review von Etappe 3:** Zwei Runden nicht bestanden: `saveReplay` hing bei NUL und bei rund 32.760 Zeichen im Dateinamen, Ctrl-Z beendete die Sitzung, Invarianten liefen zu selten, der Fehlerpfad hatte keinen Test, dazu doppelte Logik. Alles behoben oder in E-23 und unten begründet, die dritte Runde bestand mit Mutanten (`285e8a2`). Die Coverage von Core maß das Review im frischen Klon mit 882/888 statt 881/888; ungeklärt, ohne Folgen für die Schwelle.
- **Review von Etappe 4:** Erst nicht bestanden: Klicks gingen durch das HUD auf die Karte, im Hintergrund lief das Spiel auf dem Desktop weiter, das HUD überlappte in kleinen Fenstern, die Interpolation fehlte. Alles behoben, Nachprüfung mit Mutanten bestanden (`68a99ac`).
- **cppcheck** analysiert wirklich alle Dateien, siehe E-19.

## Nächster Schritt

1. Etappe 12 (Balancing) mit gemeinsamem Katalog aller Karten (E-32), dabei Wachstum und Kopfgeld des Endlosmodus prüfen (E-36).
2. Wunsch des Menschen (2026-11-06), bei Gelegenheit: ein Icon für das Spiel und ein Lade- oder Splash-Screen mit kleinem Intro, Sprachausgabe des Helden, Achteck-Knöpfe wie Demo A.

## Bekannte Mängel und offene Punkte

- **Stufen ohne Inhalt (MISSING in `verify`):** Balancing (12).
- **Steuerschnittstelle:** Läuft in `qtd_headless` und im echten Spiel auf Standardein- und -ausgabe (E-26). Die Aktionen zu Regeln, die es noch nicht gibt, fehlen. Schadensarten, Effekte und Resistenzen stehen nicht in der Beobachtung, ein Agent liest sie aus der Kartendatei. Ein localhost-Transport fehlt; falls er kommt, müssen `startMap`, `saveReplay` und `screenshot` auf erlaubte Pfade beschränkt werden. Raster und Kosten stehen nicht in der Beobachtung, ein Agent liest sie aus der Kartendatei.
- **Aus dem Review von Etappe 4, nicht behoben:** Kein Test merkt, wenn `MapInstances` nicht mehr an `frameChanged` hängt (die Gegner sprängen dann wieder mit 30 Hz). `frameChanged` kommt auch in der Pause und nach Spielende.
- **Aus dem Review von `feature/raster`, nicht behoben:** Kein Test sichert, dass der Fuzz-Bot genau einen Wert eines Befehls bricht und dass das Brett keine Fugen hat; beides ist nur von Hand gemessen. Felder sind auf kleinen Displays klein (etwa 3 mm bei 800x360 ohne Zoom). Die Form ist nur im HUD zu sehen, nicht auf der Karte, ein abgelehnter Tipp bleibt stumm, das HUD nennt die rohe ID statt eines übersetzbaren Namens, alle Türme haben eine Farbe. Das gehört zu den Menüs und Assets (Etappen 10 und 11), spätestens zur Bedienung auf Mobile.
- **Review von `feature/11-spielwelt`:** Erst nicht bestanden: Absturz auf Karten von 1 Feld Breite, Pfade am Rand wurden zu Sandflächen, drei Mutanten überlebten, nach einem Kartenwechsel blieb im fernen Boden das alte Bild, der Rand des Geländes war sichtbar, Wasser sah aus wie Schlamm, Baumkronen nahmen Taps der Nachbarfelder. Alles behoben, Nachprüfung mit Mutanten bestanden (`1011d4d`).
- **Aus dem Review von `feature/11-spielwelt`, nicht behoben:** Im Sumpf zeigt ganz verschobene Kamera bei größtem Zoom eine gerade Küste, weil das Land am Rand auf Höhe 0 ausläuft. Eine Landschaft ohne `water` und ohne `lakes: false`, auch der Standard bei kaputter `landscapes.json`, hat jenseits des Geländes weder Ebene noch Meer; heute trifft das keine Karte. Der GPU-Speicher bei vielen Kartenwechseln ist ungeprüft, der Arbeitsspeicher blieb stabil. Die Vollprüfung scheiterte zweimal an einem internen Fehler von MSVC im Coverage-Build, an wechselnden Dateien, Ursache offen, danach nicht wieder.
- **Aus dem Review von Etappe 9, nicht behoben:** Fehlertexte beim Laden kommen aus Core und erscheinen englisch im HUD, wie die Fehler einer Karte. Übersetzbar werden sie mit Etappe 13. `resumeGame` vergleicht ähnlich wie `ReplayPlayer`, `loadGame` liest die Datei zweimal.
- **Aus dem Review von Etappe 8, nicht behoben:** Das HUD zeigt die rohen IDs der Zauber (`deep_freeze`), wie bei den Türmen. Anzeigenamen gehören zum UI-Thema vor Etappe 11.
- **Aus dem Review von Etappe 7, nicht behoben:** `mire_siege` ist zu leicht, der naive Bot gewinnt ohne Upgrades (Etappe 12). Über die Steuerschnittstelle verkauft, bleibt die ID des gewählten Turms in `GameSession` stehen. Das ist harmlos, weil alles über `chosen()` geht.
- **Aus dem Review von `feature/bildzeit`, nicht behoben:** Die Arbeit des GUI-Threads (`GameSession::refresh`, `MapInstances`, QML-Bindings) misst nichts, sie fällt erst auf, wenn die Bildrate unter 60 sinkt. Die GPU-Zeit prüft nur der Smoke-Test in der Vollprüfung, nicht `tst_client_screen_control`.
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
- **MSVC-Absturz C1001, ungeklärt, dreimal gesehen (zuletzt in Etappe 9 in `tst_game_session.cpp`, ein zweiter Build lief durch):** Interner Compilerfehler in frischen Klonen, jeweils in Dateien, die das Feature nicht änderte: im Review von `feature/llvm-update` im Coverage-Build (`src/app/main.cpp`, Header `ranges`), im Review von `feature/enums-statt-strings` in `tests/qml/tst_qml.cpp`. Eine Wiederholung ohne Änderung lief jeweils durch, 36 Einzelläufe blieben ohne Absturz. Beide Dateien gehören zum Client, QtCraft lief parallel. Ein Zusammenhang mit LLVM im `PATH` ist nicht ausgeschlossen. Tritt es wieder auf: Lauf wiederholen und hier ergänzen. Am 2026-11-07 traten C1001, LNK1136 und frisch gelinkte, sofort abstürzende Programme gehäuft auf. Der Mensch nannte als mögliche Ursache eine zu hohe CPU-Spannung im alten BIOS und hat es aktualisiert. Kommen die Fehler danach wieder, ist die CPU vermutlich weiter instabil.
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
| 7 | Inhalte Türme und Gegner | fertig, in `dev` |
| 8 | Zauber und Helden | fertig, in `dev` |
| 9 | Spielfluss | fertig, in `dev` |
| 10 | Assets | fertig bis auf Musik und Geländekacheln, in `dev` |
| 11 | Modi | fertig |
| 12 | Balancing | offen |
| 13 | Feinschliff | offen |
