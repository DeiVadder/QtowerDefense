# Entscheidungen

Je Eintrag: Was, Warum, verworfene Alternative.

## E-1 Skripte in Python
- **Was:** `scripts/*.py`, Aufruf über dünne `.cmd`-Dateien. Einzige Shell-Zeile ist `scripts/hooks/pre-commit`, weil git Hooks über sein `sh` startet.
- **Warum:** läuft unter Windows ohne Bash, auch auf anderen Plattformen.
- **Verworfen:** PowerShell (nicht plattformneutral), Bash (Vorgabe).

## E-2 ASan-Preset als RelWithDebInfo
- **Was:** `asan` baut mit `/MD` gegen die Release-DLLs von Qt und nutzt `clang_rt.asan_dynamic-x86_64.dll`. `_DISABLE_VECTOR_ANNOTATION` und `_DISABLE_STRING_ANNOTATION` sind gesetzt, weil Qt nicht instrumentiert ist. `asan_selfcheck` beweist, dass der Sanitizer wirkt.
- **Warum:** `/RTC1` aus Debug schließt ASan aus.
- **Verworfen:** Debug ohne `/RTC1` (eigene Flag-Sonderwege, langsamer).

## E-3 UBSan mit clang-cl: in Etappe 0 nicht möglich
- **Was:** Kein UBSan-Preset. Die VS Build Tools enthalten unter `VC\Tools\Llvm\x64\bin` nur `clang-tidy` und `clang-format`, kein `clang-cl` und keine Sanitizer-Laufzeit. Die Bewertung endete deshalb ohne Versuch; ob Core mit clang-cl und Qt zusammen baut, ist ungeprüft.
- **Warum:** Der Weg bräuchte ein weiteres Werkzeug (offizielles LLVM-Release unter `C:\dev\LLVM` oder die VS-Komponente „C++ Clang tools"). Das war nicht im Etappenumfang, und das Gerüst hat noch keine Arithmetik, die UBSan prüfen könnte.
- **Überholt durch E-22.** Empfehlung damals: In Etappe 1 mit dem Festkomma-Code erneut bewerten. Aussichtsreich ist ein LLVM-Release und `-fsanitize=undefined -fsanitize-trap=undefined` nur für Core-Tests; der Trap-Modus braucht keine Laufzeit. Bis dahin gilt der Ersatz aus Auftrag Abschnitt 4 (Warnstufe, clang-tidy, Überlaufprüfungen in Debug).
- **Verworfen:** LLVM jetzt ungefragt nachinstallieren.

## E-4 clazy weggelassen
- **Was:** Kein clazy.
- **Warum:** kein offizielles Windows-Paket auf GitHub, Eigenbau gegen LLVM wäre unverhältnismäßig. Vom Auftrag so vorgesehen.
- **Verworfen:** Inoffizielle Builds (Vorgabe: nur offizielle Quellen).

## E-5 `/wd4702` nur für qmlcachegen-Ausgaben
- **Was:** `src/client/CMakeLists.txt` schaltet C4702 nur für die generierten `.rcc/qmlcache/*.cpp` ab.
- **Warum:** Die Warnung entsteht in Qt-Vorlagen, die generierter Code instanziiert; das fällt unter „Fremdcode ausgenommen".
- **Verworfen:** `/WX` für ganz `qtd_client` abschalten.

## E-6 Funktionslänge zählt Codezeilen
- **Was:** `check_length.py` zählt nicht leere Zeilen ohne reine Kommentare von `{` bis `}`, für C++ und QML. Die Dateilänge zählt alle Zeilen (auch `.py`). Der Parser ist eine Klammer-Heuristik, kein Compiler.
- **Warum:** Kommentare sollen nicht bestraft werden; gleiche Zählweise wie QtCraft.
- **Verworfen:** `readability-function-size` (zählt Kommentare mit).

## E-7 Determinismus-Liste weiter als die sechs genannten Namen
- **Was:** Zusätzlich verboten in Core: Gleitkomma-Literale (auch Exponent- und Hex-Form), `qreal`, `QMultiHash`, `QSet`, `std::unordered_*`, `rand`, `<random>`-Typen, `QTimer`, `QElapsedTimer`, `QDate`, `QTime`, `time()`, `clock()`, Threads.
- **Warum:** Folgt aus den Regeln in Abschnitt 2 (Zufall, Reihenfolge, Uhr, Einsträngigkeit). Eine Ausnahme ist im Skript nicht vorgesehen und bräuchte hier einen Eintrag.
- **Verworfen:** Nur die sechs Namen (`QSet` wäre eine Lücke).

## E-8 Wiederholung nach clang-tidy-Absturz
- **Was:** `verify.py` wiederholt eine Datei einzeln, wenn clang-tidy hängt (Timeout 300 s) oder mit einem Windows-Ausnahmecode ab `0xC0000000` endet. Beobachtet wurden `0xC0000005`, `0xC000001D` und ein Hänger über 35 Minuten, jeweils an harmlosen Dateien. Andere Fehler zählen sofort, ein zweiter Ausfall lässt die Stufe scheitern. Das Protokoll nennt die Zahl der Ausfälle.
- **Warum:** clang-tidy 19.1.5 stürzte bei sechs parallelen Läufen in etwa jedem zweiten Lauf bei einer Datei ohne Ausgabe ab, wechselnde Dateien. Acht serielle Läufe blieben stabil. Ursache unbekannt. Ein Absturz sagt nichts über die Datei, und die Wiederholung prüft sie wirklich.
- **Nachtrag E-22:** Mit clang-tidy 23.1.5 gab es in drei vollen Läufen keinen Ausfall mehr. Die Wiederholung bleibt als Schutz, sie kostet nichts.
- **Verworfen:** Absturz als grün werten; ganz seriell laufen (mehrfache Laufzeit).

## E-9 cppcheck mit `--std=c++20`
- **Was:** cppcheck 2.14 läuft mit `--std=c++20` und eigener `scripts/cppcheck-qtd.cfg` für Qt-Makros, die `qt.cfg` nicht kennt. Unterdrückt ist nur `missingIncludeSystem` (Begründung in `scripts/cppcheck-suppressions.txt`).
- **Warum:** Das Projekt ist C++23, aber cppcheck ist hier nur eine Zweitmeinung; der Compiler und clang-tidy kennen die Sprache.
- **Seit Etappe 1:** `--check-level=exhaustive`, denn die Normalstufe meldet bei verzweigtem Code nur „Analyse begrenzt" und scheitert damit an `--error-exitcode`. Der Lauf bleibt unter einer Sekunde.
- **Verworfen:** `unknownMacro` unterdrücken (würde auch echte Treffer verbergen).

## E-10 Fenster-Test gehört zu den normalen Tests
- **Was:** `tst_client_window` rendert `Main.qml` einmal auf der GPU und prüft, dass das Bild mehrere Farben hat. Er läuft in `--quick`.
- **Warum:** Beweist die 3D-Szene bei jedem Lauf, Dauer unter 2 s. Er ersetzt nicht den Smoke-Test aus Etappe 5, der die Steuerschnittstelle braucht. Darum bleibt jener MISSING.
- **Verworfen:** Fenster nur von Hand prüfen.

## E-11 QML-Dateien flach in `src/client`
- **Was:** QML liegt neben dem C++ des Clients. `Main.qml` importiert sein eigenes Modul, damit qmllint den Singleton `AppInfo` auch im Quellbaum findet.
- **Warum:** Kein `QT_RESOURCE_ALIAS` nötig, solange es wenige Dateien sind. Mit dem HUD in Etappe 4 neu ansehen.
- **Verworfen:** Unterordner `qml/` ab sofort.

## E-12 Ausschlüsse in `.clang-tidy`
- **Was:** `bugprone-narrowing-conversions` ist seit dem Review aktiv, das Gerüst ist damit sauber (eine Probe mit `long long` nach `int` meldet den Fehler). Die Zahl in Klammern nennt die Treffer nach Etappe 2, eindeutig je Datei und Zeile über alle Übersetzungseinheiten, gemessen am 2026-11-04 mit clang-tidy 23.1.5. Ein Selbsttest verlangt für jeden Ausschluss einen Eintrag hier.
- **Warum, je Ausschluss:**
  - `bugprone-easily-swappable-parameters` (1): Trifft jede Funktion mit zwei gleichartigen Parametern, in der Simulation also Koordinaten und Festkomma-Paare. Die Schwelle ließe sich nur raten; Verwechslungen fangen eigene Typen und Tests besser.
  - `modernize-use-trailing-return-type` (244): Reiner Stil, der Qt-Code und das Projekt schreiben Rückgabetypen vorne.
  - `modernize-use-nodiscard` (18): Würde fast jede const-Methode markieren. `[[nodiscard]]` steht dort von Hand, wo Ignorieren wirklich ein Fehler ist.
  - `modernize-avoid-c-arrays` (1): Der Treffer ist das absichtliche Feld in `tests/asan/asan_selfcheck.cpp`. Der Test braucht genau diesen Überlauf eines rohen Felds.
  - `modernize-use-designated-initializers` (43): Bei kleinen Wertetypen wie `Version{1, 2, 3}` bringt es nur Länge, die Reihenfolge ist durch Tests gesichert.
  - `modernize-use-auto` (2): Widerspricht der Regel des Menschen in `CLAUDE.md`: `auto` nur, wenn der Typ ohne LSP lesbar ist.
  - `modernize-return-braced-init-list` (1): Reiner Stil, nennt den Typ im `return` nicht mehr.
  - `misc-include-cleaner` (259): Verlangt für jeden Typ einen eigenen Include, auch für `QChar` oder `QStringView`, die `<QString>` ohnehin mitbringt, und für alles aus einem eigenen Header. Das bläht die Include-Listen auf, ohne dass ein Fehler verhindert wird.
  - `misc-non-private-member-variables-in-classes` (5): Trifft reine Wertetypen wie `Version`, deren öffentliche Felder gewollt sind.
- **Verworfen:** Ausschlüsse ohne Begründung lassen. Mit Etappe 2 neu bewertet: `performance-enum-size`, `misc-no-recursion` und `misc-use-anonymous-namespace` hatten 0 Treffer und sind jetzt aktiv, Rekursion braucht also ein begründetes NOLINT. `misc-use-internal-linkage` prüft keine Typen (`AnalyzeTypes: false`), weil Testklassen mit `Q_OBJECT` wegen moc nicht in einen anonymen Namensraum können.

## E-13 Coverage gleicht die Dateiliste ab
- **Was:** `coverage.py` vergleicht die Quelldateien von `src/core` und `src/automation` mit den Berichten. Eine Datei ohne gemessene Zeile lässt das Gate scheitern und wird namentlich genannt.
- **Warum:** Aus einer statischen Bibliothek landet nur im Test-Programm, was ein Test referenziert; eine ungetestete Datei fehlte deshalb im Bericht und drückte die Quote nicht. Der Abgleich erfasst auch Dateien, die in keiner `CMakeLists.txt` stehen. Die ausführbaren Zeilen einer nie gelinkten Datei sind nicht bekannt, deshalb Scheitern statt 0 %.
- **Folge:** Eine `.cpp` ohne ausführbaren Code in diesen Schichten scheitert ebenfalls; solcher Inhalt gehört in einen Header.
- **Verworfen:** Objektbibliothek oder `/WHOLEARCHIVE`: Greift nur für Dateien, die CMake kennt, und ändert das Bauen aller Presets oder braucht einen Sonderweg für `coverage`.

## E-14 Festkomma: dezimal, vier Nachkommastellen
- **Was:** `core::Fixed` ist ein `qint64`, der Zehntausendstel zählt (1,5 = 15000). `*`, `/` und `sqrt` runden zum nächsten Rohwert, bei Gleichstand von null weg. Grenzen: Addition bis etwa ±9,2e14; Produkt, Dividend und Wurzelargument bis etwa 9,2e10, weil der exakte Zwischenwert in `qint64` passen muss. Der Rohwert `INT64_MIN` kommt nicht vor, Negation ist also immer sicher.
- **Warum Dezimal statt Zweierpotenz:** Datenwerte wie `0.1` sind mit Skalierung 2^16 nicht darstellbar; der Lader müsste runden oder Autoren zu Brüchen wie `0.125` zwingen. Mit 10^4 ist jeder Wert mit höchstens vier Nachkommastellen exakt, Erwartungswerte in Tests lassen sich von Hand rechnen. Die Division durch 10^4 statt einer Verschiebung ist bei 200 Gegnern ohne Bedeutung.
- **Warum diese Rundung:** symmetrisch, also `-(a*b) == (-a)*b`. Reines Abschneiden würde wiederholte Rechnungen in eine Richtung ziehen. Die Wurzel ist ganzzahlig (Binärziffern-Verfahren), `distance()` nutzt sie. Reichweiten vergleichen quadrierte Abstände und brauchen keine Wurzel.
- **Überlauf (Ersatz für UBSan, Abschnitt 4):** Bereichsfehler, Division durch null und Wurzel aus negativen Zahlen werfen in Debug-Builds (`QT_NO_DEBUG` nicht gesetzt) `FixedRangeError`. Sonst sättigt das Ergebnis, die Wurzel aus Negativem gibt 0: kein undefiniertes Verhalten, auf jeder Plattform gleich. `debug` und `coverage` prüfen also, `release` und `asan` sättigen; die Tests decken beide Wege ab.
- **Folge:** Ein Lauf mit Überlauf endet in Debug mit einem Fehler und liefe in Release weiter. Die Grenzen im Lader (E-16) halten alle Spielwerte weit davon entfernt.
- **Verworfen:** Q16.16 in `qint32`: zu kleiner Bereich für Bosse. Q32.32: braucht 128-Bit-Zwischenwerte, die MSVC nicht hat. `qMulOverflow<qint64>`: Der Qt-Header löst unter MSVC C4702 aus, das `/external:W0` nicht abfängt; die eigene Prüfung sind drei Zeilen.

## E-15 Zufall: PCG32
- **Was:** `core::Random` ist PCG32 (XSH-RR, 64 Bit Zustand). `nextBelow` verwirft die Werte, die das Ergebnis verzerren würden, wie `pcg32_boundedrand`. Alle Partien nutzen Stream 54, nur der Seed unterscheidet sie.
- **Warum:** Klein, schnell, gut untersucht, nur Ganzzahlen, und es gibt veröffentlichte Referenzausgaben, gegen die der Test prüft. Beide Zustandswörter gehen in den Hash.
- **Einsatz in Etappe 1:** Der Schaden je Schuss wird im Rohwert zwischen `damageMin` und `damageMax` gewürfelt. Das ist auch bei `damageMin == damageMax` ein Zug, damit eine Datenänderung den Zufallsstrom nicht verschiebt.
- **Verworfen:** xorshift und SplitMix64: ebenfalls möglich, aber PCG hat die Referenzwerte direkt im Demo-Programm der Autoren.

## E-16 Zahlen in Inhalts-JSON
- **Was:** `parseContent` lässt im JSON-Text nur ganze Zahlen mit höchstens 18 Ziffern zu. Dezimalwerte stehen als Zeichenkette (`"3.5"`) und gehen über `Fixed::parse` direkt in Festkomma. Mehr als vier Nachkommastellen sind ein Fehler, gerundet wird nie. Unbekannte Schlüssel sind Fehler, Grenzen je Feld stehen in `content_loader.cpp`.
- **Warum:** `QJsonValue` speichert Brüche und große Zahlen als Gleitkomma, schon der Parser rundet also. `toInteger()` allein sieht das nicht: `1.00000000000000000001` wird zu `1.0` und dann zu `1`. Deshalb prüft ein eigener Durchgang über den Text des gültigen Dokuments jedes Zahl-Token, bevor ein Wert gelesen wird. Ganze Zahlen bis 18 Ziffern hält Qt exakt als `qint64`, dann ist `toInteger()` sicher.
- **Einheiten:** Zeiten in Ticks, Tempo in Feldern je Tick. So entsteht beim Laden keine Umrechnung, die rundet; ein geänderter Tick-Takt verlangt dafür eine Datenanpassung.
- **Grenzen:** Koordinaten ±10000, Reichweite 30000, Tempo 1000, Schaden 100000, Ganzzahlen und Leben der Gegner 10^9. Damit bleiben quadrierte Abstände im Bereich von `Fixed`, und die Schadensspanne passt als Rohwert in 32 Bit.
- **Lücke:** Doppelte Schlüssel in einem Objekt erkennt `QJsonDocument` nicht, der letzte gewinnt.
- **Verworfen:** Zahlen über `toDouble()` und Prüfung auf Ganzzahligkeit: Gleitkomma in Core ist verboten, und der Schaden ist da schon passiert. `QCborValue::isInteger`: hängt davon ab, wie Qt intern speichert.

## E-17 Tick und Befehle
- **Was:** `Simulation::tick()` führt genau einen Tick aus: Gegner bewegen, Gegner erzeugen, Türme feuern, Spielende prüfen. `advance(n)` läuft `n` Ticks. `kTicksPerSecond` ist 30, die Simulation selbst rechnet nur in Ticks; wer sie antreibt (Client, Headless, Test), bestimmt das Tempo. Befehle laufen über `execute()` zwischen zwei Ticks und geben ihr Ergebnis sofort zurück, abgelehnte Befehle ändern nichts.
- **Warum:** Ein Replay braucht nur Paare aus Tick und Befehl (Etappe 2). Bots sehen das Ergebnis sofort. Fällt das letzte Leben im selben Tick wie der letzte Gegner, ist die Partie verloren: Die Niederlage hat Vorrang.
- **Reihenfolge:** Gegner und Türme liegen in `std::vector` in ID-Reihenfolge. Ziel ist der Gegner mit dem weitesten Weg in Reichweite, bei Gleichstand die niedrigste ID. Tote Gegner fallen erst am Ende des Turm-Schritts heraus und sind bis dahin kein Ziel.
- **Offen für später:** `Command` ist ein `std::variant`, `SimulationState` ein reiner Wert (kopierbar, für Speichern und Laden). `wave` ist eine Liste von Gruppen; mehrere Wellen, Schadensarten und Verkauf kommen als neue Felder und Alternativen dazu, mit neuer `formatVersion`.
- **Verworfen:** Warteschlange je Tick: Das Ergebnis eines Befehls käme erst später, ohne dass der Determinismus gewinnt.

## E-18 Zustands-Hash
- **Was:** FNV-1a mit 64 Bit. `stateHash()` schreibt jedes Feld einzeln als acht Bytes, niedrigstes zuerst, Listen mit vorangestellter Länge. Dazu gehören Tick, Status, Gold, Leben, beide Wörter des Zufallsgenerators, Spawn-Stand, alle Gegner und Türme. Der Hash der Inhalte wird einmal beim Start gebildet und ist ein Feld des Zustands.
- **Warum:** Weil nie Speicher am Stück in den Hash geht, spielen Padding, Zeiger, Kapazität und Plattform keine Rolle. Listen sind nach ID geordnet, eine Iterationsreihenfolge gibt es nicht. FNV hat veröffentlichte Testvektoren. Kryptografische Sicherheit braucht es nicht: Der Hash soll Abweichungen finden, keine Angriffe abwehren.
- **Folge:** Ein neues Feld in `SimulationState` muss in `stateHash()` und in die Tabelle von `everyFieldOfTheStateCounts`. Die IDs der Inhalte zählen nicht, die Simulation kennt nur Indizes.
- **Verworfen:** `qHash` (je Prozess gesalzen), `QCryptographicHash` (langsamer, ohne Nutzen hier).

## E-19 cppcheck: kurze Laufzeit ist echt
- **Was:** `verify` ruft cppcheck ohne `--quiet` auf, das Log nennt jede Datei.
- **Befund:** Rund 0,4 s für 22 Übersetzungseinheiten sind korrekt. In einer Kopie des Baums fand cppcheck mit denselben Optionen einen eingepflanzten Nullzeiger in allen 33 Quell- und Header-Dateien, dazu Fehler, die Datenfluss brauchen (Zugriff auf leeren Vektor, Nullzeiger über einen Zweig). Der Exit-Code war 1. Schnell ist es, weil es nur rund 3.100 Zeilen sind und Qt- und STL-Header bewusst nicht geparst werden.
- **Grenze:** Ohne diese Header kennt cppcheck Qt-Typen nur aus `qt.cfg`, Datenfluss durch Qt-Aufrufe sieht es also kaum. Die `.qml`-Dateien liest es wegen `--language=c++` als C++ mit, das ist wirkungslos.
- **Verworfen:** Qt-Header mitgeben: Jeder Lauf würde ganz Qt parsen, die Tiefe liefert clang-tidy schon.

## E-20 Replays und festgeschriebene Hashes
- **Was:** Ein Replay besteht aus Seed, Inhalts-Hash, allen Befehlen samt Ergebnis und Checkpoints (Tick, Zustands-Hash) einmal je Sekunde Spielzeit. Ein Checkpoint gilt nach den Befehlen seines Ticks. Der Rekorder schreibt ihn deshalb erst unmittelbar vor dem nächsten Tick. `ReplayPlayer` meldet die erste Abweichung mit Tick und beiden Werten.
- **Warum Checkpoints statt nur Endhash:** Eine Abweichung lässt sich auf eine Sekunde eingrenzen. Abgelehnte Befehle gehören dazu, weil sie der Fuzz-Bot braucht und ihr Ergebnis eine Regel festschreibt.
- **Zahlen:** Seed und Hashes stehen als Text in der Datei, 64 Bit übersteht `QJsonValue` nicht ohne Weiteres (E-16). `slot` und `towerType` dürfen beliebige 18-stellige Zahlen sein, ungültige Befehle sind Teil einer Aufzeichnung.
- **Unabhängige Werte:** `tests/core/state_hash_reference.py` rechnet FNV-1a und PCG32 ohne Projektcode nach, `tst_state_hash` prüft den Zustands- und den Inhalts-Hash gegen diese Werte. Die Golden-Dateien schreiben ganze Partien fest und gelten in `debug`, `release`, `asan` und `coverage`. Damit eine versehentliche Neuerzeugung auffällt, steht ein Endhash zusätzlich in `tests/CMakeLists.txt`.
- **Skripte:** Ein Skript ist ein Replay ohne Pflicht zu Hashes. So reicht ein Format, und `scripts/update_golden.py` erzeugt Golden-Dateien aus sich selbst.
- **Verworfen:** Binärformat: nicht diffbar. Nur Endhash: Fehlersuche über die ganze Partie. Inhalte ins Replay kopieren: Eine Regeländerung in den Daten fiele nicht als abweichender Inhalts-Hash auf.

## E-21 TickClock in Core
- **Was:** `TickClock` macht aus vergangenen Mikrosekunden, Tempo und Pause eine Anzahl Ticks, in reiner Ganzzahl-Rechnung ohne Verlust von Bruchteilen. Die Zeit misst der Aufrufer.
- **Warum:** Der Tempo-Test aus Abschnitt 3 soll den Weg prüfen, den der Client später nimmt: ungleiche Frames, Stapel von Ticks, Pause. Der alte Vergleich von `tick()` mit `advance()` zeigte das nicht. In Core darf die Klasse liegen, weil sie keine Uhr kennt.
- **Verworfen:** Klasse im Client: Dann wäre sie ohne GUI-Abhängigkeit nicht testbar und der Golden-Test bliebe ohne echten Tempo-Pfad.

## E-22 LLVM 23.1.5, UBSan mit clang-cl
- **Was:** `C:\dev\LLVM` enthält das offizielle Archiv von LLVM 23.1.5. `clang-tidy`, `clang-format` und `clang-cl` kommen von dort, `scripts/env.cmd` und `qtd_common.py` zeigen darauf. Das Preset `ubsan` baut Core, Automation, `qtd_headless` und ihre Tests mit `clang-cl` und `-fsanitize=undefined -fsanitize-trap=undefined`. `verify` führt es in der vollen Stufe aus, `ubsan_selfcheck` beweist, dass der Sanitizer wirkt.
- **Warum Archiv statt MSI:** Es wird nur entpackt, nichts außerhalb von `C:\dev` ändert sich.
- **Warum Trap-Modus:** Die UBSan-Laufzeit von LLVM gibt es nur statisch (`MT_StaticRelease`), Qt braucht die dynamische Laufzeit. Der Link scheitert daran an `/failifmismatch`. Im Trap-Modus endet das Programm bei einem Fund mit einem unerlaubten Befehl. Die Stelle zeigt der Debugger, einen Text gibt es nicht.
- **Umfang:** Ohne Client: Dort steckt keine Regelarithmetik, und die Laufzeit des Builds bliebe klein. RelWithDebInfo, weil die Debug-Laufzeit keinen Gewinn bringt; in diesem Build sättigt `Fixed` also, statt zu werfen (E-14).
- **Ergebnis clang-tidy (E-8):** Drei volle Läufe mit sechs parallelen Prozessen, 90 Dateiläufe, kein Absturz und kein Hänger. Ein Lauf dauert rund 10 s statt rund 130 s mit 19.1.5. Dass es trotzdem prüft, zeigten eingepflanzte Fehler (`use-after-move`, identische Zweige, Narrowing). Neue Treffer waren funktionale Casts wie `qint64(1)`, sie sind behoben.
- **Nebeneffekt:** Die Golden Replays bestehen auch mit `clang-cl`, also mit einem zweiten Compiler. Das stützt die Bitgleichheit aus Abschnitt 2.
- **clang-format:** Version 23 formatiert Lambdas in Listen anders, der Bestand ist einmal neu formatiert.
- **Verworfen:** UBSan mit `/MT`: Qt liegt nur als DLL mit dynamischer Laufzeit vor. Nur `clang-tidy` tauschen: Zwei Werkzeugstände für eine Aufgabe, und `clang-format` passte dann nicht mehr zu `clangd` 23.

## E-23 Bots und Steuerschnittstelle
- **Was:** `Bot::nextCommand` liest das Spiel und gibt `core::Command` zurück, also dieselben Befehle wie ein Mensch. `ControlSession` übersetzt eine JSON-Zeile in dieselben Befehle und zeichnet das Spiel mit `ReplayRecorder` auf. Der Transport ist getrennt: `--control` auf Standard-Ein- und -Ausgabe. Das echte Spiel nutzt ab Etappe 4 oder 5 dieselbe Sitzung über localhost.
- **Warum nur fünf Aktionen:** `startMap`, `buildTower`, `advance`, `observe`, `saveReplay` decken ab, was die Regeln heute können. Verbessern, Verkaufen, Zielpriorität, Zauber, Welle vorziehen, Spielstand speichern und laden kommen mit den Regeln dazu (`docs/Steuerschnittstelle.md`). Die Aktionen vorher anzulegen hieße, Antworten für Regeln zu erfinden, die es nicht gibt.
- **Warum `std::getline` statt `QFile`, und binär:** `QFile::readLine` auf dem Text-Modus-`stdin` von Windows lieferte nach CRLF je nach Eingabe Leerzeilen oder fremde Bytes (der Prozesstest fand das). `QTextStream` kam nicht in Frage, weil es blockweise liest; ein Agent wartet aber nach jeder Zeile auf die Antwort. `stdin` steht unter Windows auf binär, weil der Text-Modus die Eingabe an einem Ctrl-Z beendet (Fund aus dem Review).
- **Invarianten:** Bots prüfen sie am Start, nach jedem Befehl und nach jedem Tick, außerdem darf kein abgelehnter Befehl den Hash ändern. `--replay` prüft sie Tick für Tick, sonst wäre ein gespeichertes Fehler-Replay beim Abspielen unauffällig. Sie liegen in Automation, nicht in Core, damit der Spielablauf sie nicht bezahlt. `HeadlessOptions::check` lässt Tests den Fehlerpfad auslösen, ohne die Simulation zu beschädigen.
- **Grenze von `--replay`:** Es findet Invarianten, nicht aber „abgelehnter Befehl änderte den Zustand" oder „Befehl lief nach Spielende": Das braucht den Vergleich rund um jeden einzelnen Befehl, den `ReplayPlayer` nicht anbietet, und das aufgezeichnete Replay stimmt mit dem fehlerhaften Programm überein. Der Bericht des Fuzz-Laufs nennt den Fehler, das Replay wiederholt die Befehle.
- **Reichweite des Fuzz-Bots:** Ein Spiel zieht, wie oft Werte gültig sind, ein Drittel der Spiele baut nie. So kommen auch Niederlagen und sinkende Leben vor. Nach Spielende gibt der Bot noch einen Schub Befehle ab, sie müssen `GameOver` ergeben.
- **Absturz im Fuzz-Lauf:** Ein Replay lässt sich dann nicht mehr schreiben. Der Lauf nennt deshalb jeden Seed vorab auf der Fehlerausgabe, `--seed` wiederholt das Spiel. Das Spiel ist deterministisch, der Seed reicht also.
- **Fuzz in `verify`:** 40 Läufe je Karte unter ASan und UBSan, knapp zwei Minuten. 200 Läufe kosteten 272 s und damit mehr als alle anderen Stufen zusammen. Die Seeds bleiben fest, damit `verify` wiederholbar ist; wechselnde Seeds können eine Prüfung ohne Codeänderung rot werden lassen.
- **Verworfen:** Eigenes Befehlsformat für Bots: Dann könnte ein Bot Dinge, die ein Mensch nicht kann. Fuzz nur über das JSON-Protokoll: Langsamer und ohne Gewinn, der Parser hat eigene Tests.

## E-24 Dünne 3D-Version
- **Was:** `GameSession` im Client hält die Simulation, zeigt den Zustand als Eigenschaften und nimmt Befehle an. `FrameAnimation` gibt die Zeit eines Frames an `TickClock`. `EnemyInstances` zeichnet alle Gegner mit einem Modell. Karten liegen als Qt-Ressource im Programm. Eingabe: Tippen baut, Ziehen verschiebt, Pinch und Mausrad zoomen.
- **Warum Instancing schon jetzt:** Gegner wechseln mit jedem Tick. Ein `Repeater3D` über eine Liste würde alle Objekte je Tick neu bauen, und der Auftrag verlangt Instancing für gleichartige Objekte. Türme ändern sich selten und bleiben QML-Objekte.
- **`instanceRoot`:** Ohne ihn gelten Instanzen global, der Versatz der Karte fehlt. Mit ihm darf das Modell nicht über `visible` ein- und ausgeschaltet werden, sonst bleibt der Ursprung stehen und die Gegner laufen neben dem Pfad; Qt zeichnet bei null Instanzen ohnehin nichts. Das fiel erst auf dem Bild des echten Fensters auf. `test_anEnemyIsDrawnOnThePath` prüft jetzt die Pixel, nachdem Frames ohne Gegner liefen.
- **Interpolation:** `GameSession::enemyPlaces` zeichnet Gegner zwischen ihren letzten beiden Ticks, den Anteil liefert `TickClock::pendingMillionths`. Das Bild läuft dadurch bis zu einen Tick hinter dem Spiel her. Türme stehen, dort gibt es nichts zu interpolieren.
- **Hintergrund:** Windows meldet keine Anwendungszustände (`ApplicationHidden`, `ApplicationSuspended` kommen dort nie). Deshalb pausiert `Main.qml` auch beim Minimieren und Verstecken des Fensters, die Anwendungszustände gelten für Mobile. Nach einer Pause geht es erst weiter, wenn der Spieler es will.
- **HUD:** Knöpfe nehmen `MouseArea`, weil ein `TapHandler` nur passiv mithört und die Karte darunter denselben Klick bekäme. Die Größe folgt der Pixeldichte und schrumpft, wenn die Breite nicht reicht.
- **Lange Frames:** Mehr als 250 ms zählen als 250 ms. Sonst springt das Spiel nach einem Hänger oder aus dem Hintergrund, statt langsamer zu werden.
- **Eigene Knöpfe:** `HudButton` aus Quick, weil Controls nicht zu den Modulen des Auftrags gehört.
- **Ein Turmtyp:** Der Client baut immer den ersten Typ, mehr gibt es erst mit Etappe 7. Die Auswahl kommt dann mit.
- **Seed:** Jede Partie im Client bekommt einen Seed aus `QRandomGenerator`. Das ist außerhalb von Core erlaubt, und die Simulation bleibt davon unberührt.
- **Verworfen:** `ControlSession` als Grundlage des Clients: Der Weg über JSON brächte nichts, die Befehle sind dieselben. Der Client zeichnet noch kein Replay auf; das kommt mit dem Smoke-Test, wenn die Steuerschnittstelle ins echte Spiel einzieht.

## E-25 Raster statt Bauplätze
- **Was:** Wunsch des Menschen vom 2026-11-05. Eine Karte ist ein Schachbrett aus `gridWidth` mal `gridHeight` Feldern (`Grid`, `Cell`, `Terrain`). Der Pfad läuft in geraden Stücken über die Felder, Hindernisse (`rock`, `tree`, `water`) belegen je ein Feld. `TowerType::footprint` nennt die Felder eines Turms relativ zu dem Feld, auf dem er steht, und muss `0, 0` enthalten. `BuildTowerCommand` nennt Feld und Typ, `Simulation::check` sagt vorab, was `execute` antworten würde.
- **Auftrag:** `QtowerDefense-Agent-Auftrag.md` nennt noch Bauplätze. Der spätere Wunsch des Menschen geht vor, die Datei bleibt seine.
- **Annahmen:** Gegner bleiben auf festen Pfaden, Türme sperren keine Wege. Pfad- und Hindernisfelder sind nicht bebaubar. Ein Turm schießt von seinem Standfeld aus, nicht aus der Mitte seiner Form. Formen lassen sich noch nicht drehen, das wäre ein weiterer Wert im Befehl.
- **Mittelpunkte:** Der Mittelpunkt von Feld `(x, y)` ist der Kartenpunkt `(x, y)`. Pfade, Reichweiten und Gegnerpositionen rechnen dadurch wie zuvor. Die Spiele der Golden Replays bleiben bis auf die Hashes gleich: gleiche Endticks, gleiches Gold, gleiche Leben.
- **Gelände als Enum:** Die Menge steht fest, Regeln werden daran hängen. Die Namen gibt es nur in der Kartendatei. Der Inhaltshash nimmt jedes Feld auf.
- **Ablehnungen:** `outsideMap`, `cellBlocked` (Gelände) und `cellOccupied` (Turm) ersetzen `unknownSlot` und `slotOccupied`. Geprüft wird in der Reihenfolge Spielende, Turmtyp, Karte, Gelände, Türme, Gold.
- **Formatversion:** Bleibt 1. Es gibt keine ausgelieferten Dateien, die alten Golden Replays sind umgestellt.
- **Client:** `MapInstances` ersetzt `EnemyInstances` und zeichnet drei Ebenen (Brett, Türme, Gegner). Die Ebene `Board` zeichnet auch Pfad und Hindernisse, damit gilt der Satz aus E-24 zu Türmen als QML-Objekte nicht mehr. Getippt wird auf die Blöcke des Bretts, der Index der getroffenen Instanz nennt das Feld. So meint ein Tipp auf einen hohen Block diesen Block und nicht das Feld dahinter. Die Turmauswahl aus E-24 kommt damit schon jetzt, weil die Formen sonst nicht erreichbar wären.
- **Verworfen:** Belegte Felder als eigene Tabelle im Zustand: Sie wäre abgeleitet und müsste in den Hash oder mit den Türmen übereinstimmen. Ein Modell je Feld: Es wären Hunderte von QML-Objekten.

## E-26 Steuerung im echten Spiel und Smoke-Test
- **Was:** `QtowerDefense --control` spielt das Spiel im Fenster über dieselben JSON-Zeilen wie `qtd_headless`. `ControlSession` arbeitet auf `ControlledGame`: ohne Fenster auf einem eigenen Spiel, im Client über `ScreenControl` auf `GameSession`. `GameSession` zeichnet deshalb jetzt auch auf. `scripts/smoke_test.py` spielt damit und ist die Stufe der Vollprüfung.
- **Standardein- und -ausgabe statt localhost:** Der Auftrag erlaubt beides. Ohne Socket gibt es nichts abzusichern: Wer die Pipes hat, hat das Programm gestartet. Die Beschränkung auf erlaubte Pfade entfällt damit, ebenso Qt Network. Ein localhost-Transport bleibt möglich, wenn ein Agent sich an ein laufendes Spiel hängen soll.
- **Lesefaden:** `std::getline` blockiert, unter Windows meldet kein Notifier Daten auf einer Pipe. Ein abgekoppelter Faden liest und reicht Zeilen über eine Postfach-Struktur an den GUI-Faden. Er kann nicht geweckt werden und kann die Sperre von stdin halten, deshalb endet das Programm mit `--control` über `std::_Exit`, nachdem alles von Qt abgebaut ist. Das ist Vorsicht: Ein Hänger ohne `_Exit` ließ sich im Release nicht zeigen. Der Smoke-Test prüft das Ende bei offener und bei geschlossener Eingabe.
- **Zeit:** Unter Steuerung steht die Echtzeit, bis `resume` kommt. So bleibt ein gesteuertes Spiel eine Frage von Ticks. Der Smoke-Test schickt dieselben Anfragen an `qtd_headless --control` und vergleicht die Hashes Schritt für Schritt. Das Spiel, mit dem das Fenster öffnet, zählt nicht: Es hat einen eigenen Seed, unter Steuerung beginnt ein Spiel mit `startMap`.
- **Prüfung des Bilds:** Das Spiel zählt Farben an 32 x 32 Punkten, wie `tst_client_window`. Keine pixelgenauen Vergleiche, aber die Bilder einer Partie müssen sich unterscheiden. Das Log muss leer sein, jede Ausgabe gilt als Fehler.
- **Bildrate:** Gemessen wird ohne Vsync (`QSG_NO_VSYNC`) mit allen 200 Gegnern auf der Karte, sonst setzt der Bildschirm die Zahl. Unter 60 Bildern je Sekunde scheitert die Stufe. Über etwa 167 sagt die Zahl nichts, deshalb misst der Smoke-Test zusätzlich die Kosten je Bild auf CPU und GPU, Grenze 2.500 µs (`docs/Performance-Verlauf.md`). Für die GPU-Zeit nutzt der Client `rhi/qrhi.h` und damit `Qt6::GuiPrivate`, sonst gibt es keinen Zugang zu den Zeitstempeln.
- **Mobile:** Der Auftrag verlangt, dass die Steuerung dort abschaltbar ist. Sie ist nur eine Startoption; ein Schalter für den Build kommt mit der ersten Mobile-Etappe.
- **Verworfen:** Screenshots per Betriebssystem von außen: Sie wären plattformabhängig und zeigten auch fremde Fenster. Eine Messung über `grabWindow`: Sie rendert anders als der normale Frame.

## E-27 Türme drehen

- **Was:** Wunsch des Menschen vom 2026-11-05, vor Etappe 6. `BuildTowerCommand` und `Tower` tragen eine `Rotation` in Vierteldrehungen im Uhrzeigersinn um das Standfeld. Auf der Karte wächst `y` zum Betrachter, deshalb dreht `(x, y)` zu `(-y, x)`.
- **Standfeld:** Es bleibt beim Drehen liegen. Von dort schießt der Turm, und dorthin zielt der Tipp.
- **Formate:** In Replays und Anfragen ist `rotation` eine Zahl von 0 bis 3 und darf fehlen, dann steht der Turm ungedreht. Andere Werte sind ein Formfehler, kein abgelehnter Befehl, weil die Menge fest ist. Der Fuzz-Bot dreht zufällig.
- **Client:** Ein Knopf dreht die Auswahl, daneben zeigt das HUD die Form, wie sie stehen wird, das Standfeld dunkel. Ohne diese Vorschau wäre Drehen im Blindflug. Die Drehung bleibt beim Wechsel des Turmtyps, ein neues Spiel setzt sie zurück.
- **Inhalt:** `horseshoe` in `training_ground` zeigt eine U-Form.
- **Formatversion:** Bleibt 1, wie in E-25. Der Hash nimmt die Drehung auf, alte Replays mit Türmen passen also nicht mehr, ausgeliefert ist noch keins.
- **Turmliste:** Sie scrollt einzeilig, statt umzubrechen, und deckt so nie mehr von der Karte zu. Damit das geht, darf der `DragHandler` der Karte Items die Geste nicht mehr wegnehmen: Handler sehen jeden Druck, auch auf dem HUD. Ein Ziehen, das auf dem HUD beginnt, verschiebt die Karte deshalb nicht mehr.

## E-28 Kampfsystem

- **Schaden:** Jeder Turm hat eine `DamageType`. Der gewürfelte Schaden zählt mit `1 - resistance`. Eine Resistenz liegt zwischen -1 (doppelter Schaden) und 1 (immun), fehlende Einträge sind 0. Der Würfel läuft immer, auch gegen Immune, sonst hinge der Zufallsstrom an der Resistenz.
- **Status:** `slow` und `poison` hängen an Treffern. Der Hitanteil gilt auch für ihre Stärke, ein Immuner bekommt nichts. Jede Art hat am Gegner einen festen Platz (`ActiveStatus`), keine Liste, weil die Menge fest und klein ist. Das spart Speicher im heißesten Pfad.
- **Kein Stapeln:** Der stärkere Effekt bleibt, von gleich starken der längere. Sonst wären zehn Frosttürme zehnmal so stark.
- **Grenze der Verlangsamung:** 90 %, im Inhalt wie nach einer Schwäche. Dazu bewegt sich jeder Gegner mindestens um einen Rohwert je Tick. Ohne das könnte ein immuner oder sehr langsamer Gegner ein Spiel für immer aufhalten. Ein echtes Einfrieren kommt mit den Zaubern (Etappe 8), dort endet es von selbst.
- **Reihenfolge im Tick:** Bewegen, Nachrücken, Gift, Schießen, Spielende. Gift wirkt erst ab dem Tick nach dem Treffer, genau `durationTicks` Mal. Tod durch Gift zahlt das Kopfgeld wie ein Schuss.
- **Zielpriorität:** Sie gehört zum Turm und beginnt mit `First`, dem bisherigen Verhalten. `SetTargetPriorityCommand` nennt die ID des Turms, unbekannte IDs ergeben `UnknownTower`. Gleichstand geht an die niedrigste ID.
- **Client:** Ein Tipp auf einen Turm wählt ihn, das HUD zeigt dann die vier Prioritäten. Der nächste Tipp daneben lässt ihn nur los und baut nichts. Türme tragen die Farbe ihrer Schadensart, Gegner die ihres Status.
- **Inhalt:** `training_ground` bekommt `frost_spire`, `venom_spire`, `stone_troll` und Schadensarten für die bestehenden Türme. Der Rest kommt in Etappe 7. Die Golden Replays ändern sich damit auch im Spielverlauf, nicht nur im Hash, `training_ground_combat` ist neu. Die letzten Befehle von `defence` und `rejected` wandern ans neue Spielende, damit `gameOver` abgedeckt bleibt.
- **Formatversion:** Bleibt 1, wie in E-25 und E-27.

## E-29 Inhalte: Stufen, Verkauf, Flächenschaden

- **Stufen:** `TowerType::levels` hält alles, was sich beim Verbessern ändern kann, die erste Stufe ist der gebaute Turm. In der Datei stehen die Werte der ersten Stufe weiter direkt am Turm, `upgrades` nennt je Stufe die Kosten und nur das, was sich ändert. Effekte ersetzen die der Stufe davor. Form und Schadensart bleiben fest, sonst könnte ein Upgrade an einem belegten Feld scheitern.
- **Verkauf:** Der Turm merkt sich in `spent`, was er gekostet hat. Heute ist das die Summe seiner Stufen, das prüfen die Invarianten. Das Feld bleibt, damit der Erlös nicht von den Inhalten abhängt, falls es einmal Rabatte gibt. `sellPercent` steht je Karte in der Datei, Standard 70, abgerundet. Der Verkauf gibt die Felder sofort frei und ist jederzeit möglich, auch während einer Welle.
- **Flächenschaden:** `splashRadius` am Turm, um das Ziel gemessen, ohne Abfall zum Rand. Ein Schuss würfelt einmal, jeder Gegner im Radius nimmt den Wurf mit seiner Resistenz und bekommt auch die Effekte. Tote zahlen nur einmal. Die erste Quelle ist `catapult`, Zauber nutzen später dieselbe Regel.
- **Regeneration:** Je Tick bis zum Startwert, nicht unter Gift. Damit hat Gift gegen Heiler eine zweite Wirkung.
- **Bosse:** `boss` ändert nur das Aussehen, die Regeln kennen keine Bosse. Was einen Boss ausmacht, steht in seinen Werten. Eigene Fähigkeiten kommen erst, wenn ein Inhalt sie braucht.
- **Zwei Karten:** `training_ground` behält die kurze Welle, weil Tests und Replays von ihr leben und der naive Bot sie gewinnt. `mire_siege` hat alle Gegner und Bosse. Jede Datei nennt ihre Türme selbst. Ein gemeinsamer Katalog lohnt sich mit den neun Karten der Weltkarte (E-32), ändert aber Dateiformat, Zustands-Hash und Replays. Er kommt deshalb mit dem Balancing in Etappe 12, das die Werte ohnehin anfasst. Bis dahin verlangt ein Test, dass Türme, Gegner und Zauber in allen Karten außer `training_ground` gleich sind. Umgesetzt in E-37, ohne Folgen für Hash und Replays.
- **HUD:** Verkaufen steht links, Upgrade rechts daneben. Fällt Upgrade auf der höchsten Stufe weg, geht ein Tipp zu viel an die Karte und lässt den Turm höchstens los, statt ihn zu verkaufen.
- **Balancing:** Die Zahlen sind Startwerte. Der naive Bot gewinnt `mire_siege` mit allen 15 Leben, mit jedem geprüften Seed und ohne Upgrades, die Karte ist also zu leicht. Das klärt Etappe 12.
- **Formatversion:** Bleibt 1, alle neuen Schlüssel sind optional.

## E-30 Zauber

- **Zauber statt Helden:** Der Auftrag nennt „Helden/Zauber". Gebaut sind Zauber, ohne Helden, die auf der Karte stehen. Ein Held wäre ein Turm mit Bewegung und bräuchte eigene Regeln, der Spieler gewinnt damit aber keine Entscheidung, die ein Zauber nicht auch bietet.
- **Regel:** Ein Zauber kostet kein Gold, nur Abklingzeit. Er wirkt sofort beim Befehl, zwischen zwei Ticks, um die Mitte eines Feldes. Ein Wurf für alle Gegner im Radius, jeder nimmt ihn mit seiner Resistenz und bekommt die Effekte, wie beim Flächenschaden (E-29). Ein Zauber ins Leere ist erlaubt und kostet die Abklingzeit.
- **Felder statt Punkte:** Das Ziel ist ein Feld, denn so bleibt die Eingabe ganzzahlig, und Tippen auf der Karte kennt ohnehin Felder. Auch Felder mit Hindernis oder Turm sind erlaubt.
- **Abklingzeit:** `spellCooldowns` steht im Zustand, je Zauber der Inhalte ein Wert, jeder Tick zieht eins ab. Zu Beginn sind alle Zauber bereit. Die Reihenfolge im Tick steht in `docs/Architektur.md`.
- **Letzter Gegner:** Stirbt er durch einen Zauber, endet die Partie erst im nächsten Tick. Die Prüfung des Spielendes gehört zum Tick, ein Befehl beendet nichts.
- **Client:** Ein Knopf wählt den Zauber, der nächste Tipp auf die Karte wirkt ihn, auch auf einen Turm. Die Knöpfe stehen rechts oben unter Pause und Tempo, unten rechts verdeckten sie Felder nahe der Kamera. Ein zweiter Druck auf den Knopf oder die Wahl eines Turms nimmt die Wahl zurück. Daneben steht die Restzeit in ganzen Sekunden. Einen sichtbaren Einschlag und die Vorschau des Radius gibt es erst mit den Effekten aus Etappe 10.
- **Inhalt:** `meteor` (Feuer, Fläche) und `deep_freeze` (Eis, 90 % langsamer) auf beiden Karten.
- **Bots:** Der naive Bot wirkt jeden bereiten Zauber auf den vordersten Gegner. Der Fuzz-Bot verkauft selten, sonst gewinnt keine Partie mehr, und selbst so nur etwa jede fünfzigste.
- **Formatversion:** Bleibt 1, `spells` ist optional.

## E-31 Spielfluss

- **Welle vorziehen:** `CallWaveCommand` setzt die Wartezeit vor der nächsten Gruppe auf null, sie kommt im nächsten Tick. Eine Welle ist hier eine Gruppe aus `wave`. Das geht nur, solange eine Gruppe wartet, nicht mitten in einer Gruppe.
- **Bonus:** Je `earlyCallTicksPerGold` gesparte Ticks ein Gold, abgerundet. Die Karten zahlen eines je 15 Ticks, also 10 Gold für eine Pause von 150 Ticks. Der Wert ist ein Startwert für Etappe 12.
- **Ein Spielstand ist ein Replay:** Ein laufendes Spiel wird als Replay gespeichert und beim Laden neu durchgerechnet (`resumeGame`). Ein eigenes Format für den Zustand hätte jedes Feld ein zweites Mal gebraucht, und der Mitschnitt ginge beim Laden verloren. Bei rund 4.400-facher Echtzeit kostet eine Stunde Spiel etwa eine Sekunde Laden.
- **Grenze:** Ändern sich Regeln oder Inhalte, lässt sich ein alter Spielstand nicht mehr laden. Das Laden prüft Inhalts-Hash, jedes Befehlsergebnis und den letzten Zustands-Hash und meldet den Fehler. Ein Stand aus einer älteren Version geht also verloren, er spielt aber nie falsch weiter. Muss das einmal anders werden, bleibt ein Zustandsformat als spätere Ergänzung möglich.
- **Ein Platz:** Der Client hat eine Datei `running_game.json` unter `QStandardPaths::AppDataLocation`. Der Spielstand nennt seine Karte, die Karten liegen nebeneinander im selben Ordner. Geladen wird pausiert.
- **Wann:** Von Hand in der Pause, von selbst, wenn die App in den Hintergrund geht. Von selbst nur, wenn es noch keinen Spielstand gibt oder die Datei dieses Spiel hält, denn das Programm startet immer ein neues Spiel und darf den gespeicherten Stand beim Weggehen nicht überschreiben. Windows sendet diese Zustände nach bisheriger Kenntnis nicht, und beim Schließen wird nicht gespeichert: Auf dem Desktop speichert nur der Spieler. Ein beendetes Spiel wird nicht gespeichert. Mit `--control` hat der Client keinen Platz, die Steuerung nennt ihre Dateien selbst und die Tests fassen nichts außerhalb des Projekts an.
- **Knöpfe:** Speichern und Laden stehen nur in der Pause da, denn am Rand verdecken sie Felder und den Anfang des Pfades. Das Hauptmenü beschreibt E-35.
- **Bots:** Der Fuzz-Bot ruft Wellen, der naive nicht. Ihn mutiger zu machen, gehört zum guten Bot aus Etappe 12.
- **Noch offen:** Highscores aus dem Auftrag kommen mit dem Endlosmodus in Etappe 11. Kampagnenfortschritt (E-32) und Einstellungen (E-35) gibt es. Das Spieltempo merkt sich der Spielstand nicht.
- **Formatversion:** Bleibt 1, der neue Schlüssel ist optional.

## E-32 Weltkarte der Kampagne

Vom Menschen am 2026-11-06 festgelegt, ergänzt den Auftrag.

- **Eine zusammenhängende Welt:** Die Kampagne zeigt eine große Weltkarte im Nebel, in der man den nächsten Abschnitt wählt. Einzelne Assets ragen aus dem Nebel. Kein Menü mit einer Kartenliste.
- **Die Welt entspricht den Levels:** Ist ein Abschnitt auf der Weltkarte ein Sumpf, ist auch das Level ein Sumpf. Beide nutzen dieselben Assets je Landschaft.
- **Freischalten:** Ein geschafftes Level öffnet seine Nachbarn und lichtet dort den Nebel. Das erlaubt Verzweigungen und ersetzt „Karten nacheinander freischalten" aus dem Auftrag. Die Sterne je Karte bleiben.
- **Größe:** Neun Abschnitte, vom Menschen so festgelegt. Das weicht vom Auftrag ab (3 bis 5 Karten). Form: ein Anfang, zwei Wege mit je drei Abschnitten, die sich vor dem Ende treffen, ein Abschnitt dort und das Ende. `training_ground` und `mire_siege` sind zwei davon, sieben Karten sind neu. Die Landschaften wählt Etappe 10 nach den verfügbaren CC0-Assets.
- **Daten und Schichten:** Die Weltkarte ist eine JSON-Datei (Abschnitte, Lage, Karte, Nachbarn). Regeln und Fortschritt liegen in C++, Nebel und Szene sind reine Darstellung. Die Simulation ändert sich nicht.
- **Nebel:** Zuerst halbtransparente Ebenen oder Sprites, ein eigener Shader nur, wenn das nicht reicht. Er muss auch auf Mobile laufen.
- **Reihenfolge:** Etappe 10 sucht die Assets je Landschaft schon mit Blick auf die Weltkarte, also Gelände und große Landmarken. In Etappe 11 wird die Weltkarte ein eigenes Feature, damit Sterne, Fortschritt und Endlosmodus nicht an ihr hängen.
- **Umsetzung, Daten:** `content/world.json` nennt neun Abschnitte mit Name, Landschaft, Lage (0 bis 1000) und optionaler Karte, dazu die Wege. `core/world.h` liest und prüft sie (eindeutige Ids, Wege nur zwischen bekannten Abschnitten, jeder Abschnitt vom Start erreichbar). Offen ist der Start, jeder gewonnene Abschnitt und jeder Nachbar eines gewonnenen. Wege gelten in beide Richtungen.
- **Sterne:** Ein gewonnenes Spiel gibt drei Sterne mit allen Leben, zwei mit mindestens der Hälfte, sonst einen (`starsOfGame`). Es zählt der beste Lauf.
- **Fortschritt:** `progress.json` im Datenordner der App, nur Sterne je Abschnitts-Id. Die Datei gehört dem Spieler: Kaputtes oder Unbekanntes wird still übergangen, nicht abgelehnt. Gezählt wird nur ein Spiel, das über die Weltkarte begann; Neustarts derselben Karte zählen mit, ebenso ein geladener Spielstand dieser Karte, solange der Abschnitt gewählt ist. Sterne von Abschnitten, die die Welt nicht kennt, bleiben in der Datei erhalten. Nach einem Sieg wählt die Karte den nächsten offenen, noch nicht gewonnenen Abschnitt.
- **Darstellung:** Eine Szene in Qt Quick 3D. Das Land ist ein gemaltes Bild auf einer Ebene (weiche Flecken je Abschnitt in der Farbe seiner Landschaft, gemeinsame Küste, gestrichelte Wege). Darauf stehen drei Landmarken je Landschaft, ausdrücklich mit Größe in `landscapes.json` (`landmarks`), denn Felsen sind als Mesh viel breiter als Bäume. Über geschlossenen Abschnitten liegen drei halbtransparente Wolkenebenen, nur höhere Modelle ragen heraus. Marker und Schild sind 2D über der Szene.
- **Die sieben neuen Karten:** Jede hat einen eigenen Pfad, Hindernisse passend zur Landschaft und Wellen mit passenden Gegnern. Türme, Gegner und Zauber sind dieselben wie im Sumpf. Gold, Leben und Wellen steigen grob mit dem Abstand vom Start, gemessen an der Summe der Lebenspunkte aller Gegner (Heide 432, Ödland 720, Hügelgräber 880, Wald 1056, Hochland 1164, Frost 1320, Aschenland 1596; der Sumpf hat 956). Der Nordweg über den Sumpf ist also der schwerere. Neben dem Sumpf haben nur Hochland, Hügelgräber, Frost und Aschenland Bosse. Es gibt nur zwei Bossmodelle, der `bog_titan` steht deshalb auch für Riesen in Hochland und Frost. Gegner, die nur zu einer Landschaft passen, kommen mit neuen Modellen. Ein Test verlangt nur, dass der naive Bot jede Karte gewinnt und ein Spieler ohne Türme sie verliert. Heute gewinnt der naive Bot überall mit allen Leben, die Karten sind also zu leicht. Das richtet Etappe 12 (Balancing).
- **Felsen je Landschaft:** Das Modell `boulder` ist eine Scholle mit Gras, auf Schnee und im Gebirge passt es nicht. Hochland, Ödland und Frost nutzen darum `rock` als Hindernis und in der Umgebung.
- **Ohne Karte:** Ein Abschnitt ohne `map` zeigt „Not charted yet" und lässt sich nicht spielen. Das bleibt für künftige Abschnitte, heute hat jeder eine Karte.
- **Start des Fensters:** Das Spiel öffnet seit E-35 im Hauptmenü, von dort geht es zur Weltkarte, ohne laufendes Spiel. Ein Screenshot über die Steuerschnittstelle geht deshalb auch ohne Spiel. Wer eine Karte startet, egal wie, sieht sie. Das Ende-Schild führt zurück zur Weltkarte. Die Namen der Abschnitte stehen in den Daten und sind noch nicht übersetzbar.

## E-33 Assets

- **Quelle:** Alles steht unter CC0, bisher von Kenney, seit E-41 auch Gegner von Quaternius und Türme von KayKit. Jede Datei steht in `docs/ASSETS.md`. Die Pakete selbst liegen nicht im Repository.
- **Ein Modell, ein Mesh:** `scripts/import_assets.py` verbindet die Teile eines Modells und übersetzt es mit `balsam`, denn nur ein einzelnes Mesh lässt sich instanziert zeichnen. 200 Gegner bleiben so wenige Draw Calls. Ins Repository kommen nur Meshes, Texturen und Sounds.
- **Keine Skelett-Animation:** Instanzen teilen sich eine Pose. Gegner schauen in ihre Richtung, das berechnet `MapInstances` aus dem Weg. Ihre Schritte sind seit E-41 gebackene Posen, Modelle ohne Posen hüpfen.
- **Name:** Ein Modell heißt wie sein Typ im Inhalt (`ballista`), Core kennt keine Modelle. Fehlt ein Modell, zeichnet der Client eine einfache Form, sie bleibt ungefärbt. Felsen und Bäume hängen nicht am Inhalt, ihre Modelle fehlen nie, das prüft ein Test.
- **Taps:** Die Blöcke von Hindernissen und Türmen bleiben unsichtbar stehen und nehmen weiter die Taps. Türme und Bäume ragen höher als ihr Block, deshalb nehmen ihre Modelle selbst Taps, das Feld ergibt sich aus der Stelle des Treffers. Der Block eines Baums bleibt flach, sonst verdeckt er das freie Feld dahinter. Der Boden unter Hindernissen hängt am Ende der Tabelle, damit die Nummer einer Instanz weiter ihr Feld nennt.
- **Gegner:** Seit E-41 Monster aus Ultimate Monsters (Quaternius). Bosse sind nur größer.
- **Schüsse:** Sie sind nur ein Bild. Die Simulation trifft sofort und nennt ihr Ziel nicht, ein Schuss fliegt zum nächsten Gegner, nicht immer zum echten Ziel.
- **Sound:** `GameSession::cue` meldet, was zu hören ist, `GameSounds.qml` spielt OGG mit `MediaPlayer`. `SoundEffect` kann nur WAV. Über die Steuerschnittstelle gibt es keine Töne. Der Ton einer Welle bestätigt nur das Rufen, eine Welle, die von selbst kommt, bleibt vorerst still. Die Player entstehen erst mit dem ersten Ton, denn schon das Anlegen lädt FFmpeg, und das schreibt ins Log.
- **Noch offen:** Hintergrundmusik, denn Kenney hat nur Jingles, sie braucht eine zweite Quelle. Das Gelände kam mit E-34, die übrigen Landschaften kommen mit den Karten der Weltkarte (E-32). Das Castle Kit ist geladen und geprüft, aber noch nicht eingebaut. Die Lautstärke steht in den Einstellungen (E-35).

## E-34 UI-Thema

Vom Menschen am 2026-11-06 aus vier Demos gewählt, ergänzt E-32 und den Auftrag.

- **Stil:** Holz und Eisen aus Demo A (`feature/ui-demo-1`) für Menü, HUD, Karten und Held. Die Demos selbst kommen nicht nach `dev`, ihr Code ist nur Vorlage.
- **Weltkarte:** Zusammenhängendes Land mit weichen Formen wie in Demo D (`feature/ui-demo-2`), in gedeckten Farben und mit den Rahmen von A. Landschaften gehen ineinander über, der Nebel verdeckt den Rest ganz, nur Umrisse ragen heraus (E-32).
- **Held:** Animiertes Portrait, schaut aufs Brett, kommentiert frech per Sprechblase und wirkt die Zauber. Sprachausgabe bleibt ein Experiment, `QtTextToSpeech` ist noch nicht installiert.
- **Karten:** Türme sind Spielkarten mit Icon und Werten, aufgefächert unten, höchstens fünf. Beim Platzieren gehen sie animiert ins 3D-Modell über. Nur Querformat.
- **Würfeln:** Ein Wurf gibt eine zufällige Karte und ist billiger als der Direktkauf. Bonusmünzen besiegter Gegner bringen beim Anklicken etwa 20 bis 30 % extra. Beides ändert die Regeln und wird deshalb Befehl in Core, sonst stimmen Replays nicht mehr.
- **Regeln der Hand:** Wie in Demo A: Karten sind bezahlte Türme, höchstens fünf (`kHandLimit`), Platzieren kostet nichts mehr. Der Wurf gibt eine zufällige Karte für `rollPercent` des billigsten Turms, der Laden die gewünschte Karte zum vollen Preis, weggeworfene Karten sind samt Gold weg. `BuildTowerCommand` nimmt die erste Karte seines Typs, ohne Karte zahlt er den vollen Preis. So bleiben Bots und Golden Replays gültig, die Karten nicht kennen, und ein Kauf mit sofortigem Bau kostet dasselbe wie der Weg über die Hand.
- **Verkauf gewürfelter Türme:** `spent` zählt, was die Karte gekostet hat, nicht den Listenpreis. Sonst brächte Würfeln und Verkaufen Gold aus dem Nichts.
- **Bonusmünzen:** Jeder besiegte Gegner lässt eine Münze über `coinPercent` des Kopfgelds liegen, sie verschwindet nach `coinTicks`. Münzen haben eigene Ids (`nextCoinId`), sonst verschöben sie die Ids der Türme und alte Replays liefen anders. Nur der Fuzz-Bot sammelt sie ein, das Gold der übrigen Bots und der Golden Replays bleibt also gleich.
- **Würfel und Laden im Client:** Die Hand beginnt leer. Die Reihe mit Würfel, Beutel und Papierkorb steht rechts über der Drehung, wie in Demo A unten rechts, aber ohne der Hand Platz zu nehmen. Der Würfel hält das Spiel nicht an: Die Karte liegt in Core schon in der Hand, die Hand zeigt sie erst nach der Animation. Karten gleichen Typs sind gleichwertig, gewählt ist immer die erste.
- **Spielwelt:** Die 3D-Welt hinter dem HUD muss deutlich detaillierter werden (Wunsch des Menschen). Das Schachbrett weicht Gelände je Landschaft mit Bewuchs und Umgebung über den Rand des Rasters hinaus, passend zum Abschnitt der Weltkarte (E-32, E-33). Das Raster ist beim Bauen deutlich sichtbar, sonst nur schwach. Das Budget für schwache GPUs gilt weiter.
- **Umsetzung der Spielwelt:** `content/landscapes.json` ordnet Karten Landschaften zu, in der Kartendatei geht das nicht, denn Core lehnt unbekannte Schlüssel ab und kennt keine Landschaften. Das Gelände ist ein Mesh mit einem je Karte gemalten Bild, beides aus `GroundField`. Über dem Raster bleibt es flach, Türme und Gegner brauchen also keine Höhe. Pflanzen stehen je Art in einem instanzierten Modell. Die Blöcke der Zellen sind unsichtbar und nehmen nur noch Taps. Nebel und Schatten kommen von Qt Quick 3D. Jenseits des Geländes liegt eine einfarbige Ebene oder Wasser bis in den Nebel, die Kamera darf über den Rand schauen. Nur die Enden eines Pfads am Rand führen über das Raster hinaus. Für eine neue Landschaft baut `Ground.qml` Mesh und Bild neu, denn eine Textur mit neuen Daten behält ihre alten Mipmaps.
- **Farben des Nature Kit:** Das Paket ist türkis und pfirsichfarben. `scripts/import_assets.py` ersetzt die Farben nach dem Namen des Materials durch erdige, jede Landschaft kann ihre Pflanzen tönen.
- **Raster schwach sichtbar, beim Ziehen deutlich:** Wer tippt statt zieht, braucht es weiter, darum verschwindet es nie ganz. Früher: Ein Zustand „Bauen" gibt es erst mit den Karten. Bis dahin steht `gridStrength` fest auf einem schwachen Wert, die Karten binden ihn an das Ziehen.
- **HUD und Menü:** HUD mit Icons statt Text, alles gerahmt. Das Menü behält Text.
- **Umsetzung der Karten:** Bis zum Würfeln liegen alle Turmtypen in der Hand, das Limit von fünf ist eine Regel der Hand in Core und kommt mit den Würfeln. Bis dahin passt die Hand durch engeres Auffächern, sie scrollt nicht. Größe und Aufbau folgen Demo A, nur der Preis steht auf Eisen, denn dunkle Schrift auf dem Kartenholz schafft keine 4,5:1. Ist ein gebauter Turm gewählt, sinkt die Hand für seine Knöpfe. Tippen bleibt neben dem Ziehen erhalten, wegen Touch und Bots. Der Bau geschieht beim Loslassen, der Flug der Karte ist nur Bild.
- **Portrait:** Wunsch des Menschen: Gesicht, Schild und Schild-Sprechblase wie Demo A, Pause und Tempo stehen im Bogen um das Portrait, in der Anordnung wie bei Anno 117. Das Portrait sitzt deshalb in der Ecke, Gold und Leben daneben.
- **Umsetzung des Helden:** `HeroVoice` hört nur zu, die Simulation weiß nichts vom Helden. Sätze wechseln reihum statt zufällig, ihre Dauer zählt in Spielzeit, je nach Tempo gestreckt. Plaudern pausiert 10 Sekunden, Zauber, Leben, Sieg und Niederlage kommen immer durch. Die Sprachausgabe bleibt offen, `QtTextToSpeech` ist nicht installiert.
- **Akzent:** Neon `#2CDE85` wie im Auftrag, vom Menschen am 2026-11-06 bestätigt. Demo A nutzte Orange. Alle Farben stehen in `Theme.qml`, ein Wechsel ist eine Zeile.
- **Icons:** Sie stammen aus Demo A, eigene SVG-Pfade in `IconPaths.js`. Jeder Turm und Zauber hat ein eigenes Icon, ein Test prüft das. Der Text eines Knopfs bleibt für Screenreader. Fehlt für neuen Inhalt ein Icon, zeigt der Knopf einen Blitz.

## E-35 Menü und Einstellungen

- **Hauptmenü wie Demo A:** Burgmauer, Tor mit dem Helden zwischen zwei Fackeln, rechts Schilder an Ketten: Kampagne, Einstellungen, Beenden. Beenden fehlt auf Android und iOS, dort beendet man Apps nicht selbst. Ein Endlosmodus kommt als eigenes Schild mit seinem Feature. „Neue Kampagne" und eine Kartensammlung wie in der Demo gibt es nicht: Fortschritt löschen braucht eine Rückfrage, und die Karten zeigt das Spiel selbst.
- **Seiten:** `Main.qml` kennt drei Seiten als Enum: Menü, Weltkarte, Spiel. Esc und die Zurück-Taste von Android gehen eine Stufe zurück: Einstellungen zu, Weltkarte zum Menü. Im Menü selbst gehört die Taste der Plattform, auf Android verlässt sie die App. Im Spiel tun sie nichts, dort führt das HUD zur Weltkarte; ein Tastendruck soll kein laufendes Spiel verlassen.
- **Einstellungen:** Lautstärke, Sprüche des Helden an oder aus und Vollbild (nur wo es Fenster gibt, `AppInfo.windowed`). Der Regler folgt dem Ohr: die Hälfte ist ein Zehntel der Leistung. Pfeiltasten verschieben ihn in Zehnerschritten. Eine Lautstärke außerhalb von 0 bis 100 aus der Datei wird begrenzt. Ist neben dem Tor zu wenig Platz, liegen die Einstellungen über ihm. Sie liegen über `Settings` aus `QtCore` in `settings.ini` im Datenordner der App, getrennt vom Fortschritt. Sprache und Grafikqualität kommen mit Übersetzung und Qualitätsstufen in Etappe 13.
- **Hochformat:** Das Spiel ist nur für Querformat gedacht (E-34). Die Sperre der Ausrichtung auf Mobile kommt mit Etappe 13.
- **Größe:** Das Menü ist ein Textmenü und nimmt so viel Platz, wie das Fenster hergibt (14 Einheiten breit, 10 hoch). Schilder größer als ein Finger schaden nicht.
- **Der Held auf der Weltkarte:** Sein Portrait steht links im Schild des gewählten Abschnitts. Bei jeder Wahl sagt er etwas zu Landschaft oder Lage, das Schild hängt über der Karte und verschwindet nach ein paar Sekunden. Oben links führt ein Knopf zurück ins Menü.
- **Hängeschilder:** `ChainSign` ist das Brett an zwei Ketten, das schwingt. `WordSign` zeigt darauf die Worte des Helden in HUD, Menü und Weltkarte, die Schilder des Menüs sind ebenfalls `ChainSign`.
- **Ohne Column:** Schilder und Zeilen der Einstellungen stehen an festen Stellen, denn ein `Column` ordnet erst mit dem ersten Bild an. Im versteckten Testfenster kommt kein Bild, und alle Schilder lägen übereinander.

## E-36 Endlosmodus

- **Regel:** Im Endlosmodus kommt die Welle der Karte immer wieder. Eine neue Runde beginnt, sobald die letzte Gruppe draußen und das Feld leer ist, mit der Wartezeit der ersten Gruppe. Der Neubeginn braucht einen eigenen Tick, ab der zweiten Runde kommt die erste Gruppe deshalb einen Tick später. Die Welle vorzuziehen geht wie sonst. Jede Runde gibt den Gegnern 25 % der Gesundheit ihres Typs dazu, höchstens bis zum Hundertfachen, gerechnet auf dem Rohwert, damit auch die größte erlaubte Gesundheit nicht überläuft. Regeneration heilt bis zur Gesundheit der Runde, in der ein Gegner kam. Es gibt keinen Sieg, das Spiel endet mit dem letzten Leben. Der Wert ist die Zahl der überstandenen Runden. Kopfgeld, Gold und Leben ändern sich nicht. Ob das trägt, entscheidet das Balancing in Etappe 12.
- **Daten:** Wachstum und Obergrenze sind Konstanten in Core (`kEndlessGrowthPercent`, `kEndlessMostPercent`). Gehören sie zur Karte, wandern sie mit dem Balancing in die Inhalte.
- **Zustand und Hash:** `SimulationState` kennt `mode` und `round`, jeder Gegner seine Runde. Der Hash nimmt diese Felder nur bei einem Endlosspiel auf. So bleiben die Hashes jeder Kampagne und alle Golden Replays unverändert, obwohl der Zustand neue Felder hat.
- **Dateien:** Ein Replay nennt `"mode": "endless"`, eine Kampagne lässt den Schlüssel weg. Alte Dateien bleiben gültig, `formatVersion` bleibt 1. Ein gespeichertes Endlosspiel geht als Endlosspiel weiter.
- **Werkzeuge:** `qtd_headless --mode endless` und `startMap` mit `"mode": "endless"`. Bericht und Beobachtung nennen dann `mode` und `rounds`, bei einer Kampagne bleiben sie wie bisher. Die Invariante zur Gesundheit rechnet mit derselben Funktion `fullHealth` wie die Simulation.
- **Im Spiel:** Das Menü hat ein eigenes Schild „Endless". Es führt auf dieselbe Weltkarte, dort lässt sich nur ein gewonnener Abschnitt endlos spielen, er zeigt seinen Bestwert statt der Sterne. Das HUD zählt die laufende Runde, die Endtafel nennt die überstandenen Runden und den bisherigen Bestwert. „Play again" bleibt im Modus.
- **Bestwert:** `progress.json` hält neben `stars` ein Objekt `best` mit den meisten Runden je Abschnitt, höchstens `kMostRounds`. Ohne Endlosspiel fehlt der Schlüssel, alte Dateien bleiben gültig, unbekannte Abschnitte bleiben erhalten wie bei den Sternen. Gezählt wird, wenn das Spiel verloren oder zur Weltkarte verlassen wird, einmal je Spiel.
- **Grenze:** Ein Endlosspiel endet nur mit dem letzten Leben. Bots laufen deshalb bis `--max-ticks`, wenn sie nicht verlieren.

## E-37 Ein Katalog für die Kampagne

- **Datei:** Türme, Gegner und Zauber der acht Kampagnenkarten stehen in `content/catalog.json`. Eine Karte nennt ihn mit `"catalog": "catalog.json"`, relativ zu ihrem Ordner, und hat dann keine eigenen `towers`, `enemies` und `spells`. Das Balancing ändert einen Wert so an einer Stelle. `training_ground` und `benchmark_arena` bleiben eigenständig, Tests und Messungen leben von ihren Werten.
- **Laden:** `loadContentFile` prüft erst den Katalog für sich und setzt ihn dann in die Karte ein. Fehler nennen die Datei, in der sie stehen: ein unbekannter Schlüssel des Katalogs den Katalog, ein unbekannter Gegner der Welle die Karte. `parseContent` bleibt bei einem einzelnen Text, Tests mit eingebetteten Inhalten ändern sich nicht.
- **Hash und Replays:** Der Inhalts-Hash rechnet über die geladenen Inhalte, nicht über die Datei. Gleiche Werte geben denselben Hash, die Golden Replays bleiben gültig. Ein Replay nennt weiter nur die Karte.
- **Grenze:** Wer eine Karte an einen anderen Ort kopiert, muss den Katalog mitnehmen. Ein Katalog kann keinen weiteren nennen.

## E-38 Der gute Bot und was das Balancing misst

- **Aufgabe:** Der Auftrag verlangt Balancing-Läufe mit einem naiven und einem guten Bot. Der gute Bot ist die Messlatte für „mit Verstand gespielt", nicht der beste denkbare Spieler.
- **Bauen:** Der Bot schätzt für jeden Turmtyp auf jedem freien Feld den Wert je Gold: Punkte des Pfads in Reichweite (alle halbe Felder) mal Schaden je Tick. Der Schaden ist der mittlere Wurf durch die Abklingzeit, mal eins plus Flächenradius, mal eins plus Verlangsamung, plus Gift über seine Dauer, mal dem Anteil, den die Welle der Karte von dieser Schadensart nimmt (nach Gesundheit gewichtet). Upgrades zählen den Zuwachs je Gold. Er spart auf den besten Plan und denkt ihn jede Sekunde neu. Drehen probiert er nur, damit ein Turm passt. Ein Turm, der nur verlangsamt und keinen Schaden macht, ist ihm nichts wert. Seine Schätzungen rechnet er in `double`: Er gehört nicht zu den Regeln, braucht kein Festkomma und bleibt auf derselben Maschine wiederholbar.
- **Zauber:** Abkühlende Zauber übergeht er. Bereit gewirkt in die dichteste Gruppe ab drei Gegnern im Radius, oder auf einen Gegner im letzten Viertel des Pfads.
- **Nicht:** Er verkauft nicht, nutzt keine Karten, Würfel und Münzen und zieht keine Wellen vor. Das bleibt Spielern überlassen und kann später dazukommen.
- **Messen:** `qtd_headless --statistics` ergänzt je Spiel das Gold je Turmtyp der stehenden Türme und eine Goldkurve. Die Werte entstehen aus dem Replay nach dem Spiel, der Bot wird beim Spielen nicht beobachtet. Die Zielkorridore und die Prüfstufe folgen mit dem Abstimmen der Werte.
- **Befund vor dem Abstimmen:** Naiver und guter Bot gewinnen alle neun Karten mit jedem geprüften Seed, oft mit gleich vielen Leben. Die Karten sind zu leicht (vgl. E-29).


## E-39 Zielkorridore und abgestimmte Werte

- **Korridore:** `docs/Balancing.md` hält die Zielkorridore als Tabelle, die `scripts/balancing.py` liest: Der gute Bot gewinnt jede Karte in mindestens 90 % der Spiele, der naive Bot die Kampagne in höchstens 80 % und das Übungsgelände immer. Jeder Turm des Katalogs bekommt 3 % bis 40 % des Golds, das der gute Bot über die Kampagne in stehende Türme steckt, und steht auf mindestens zwei Karten. Der Mittelwert der Goldkurve des guten Bots liegt je Karte zwischen 10 und 100. Die Vollprüfung spielt dafür mit dem Release-Build 50 Seeds je Bot und Karte, das dauert wenige Sekunden.
- **Warum diese Kennzahlen:** Die Gewinnraten sagen, ob Verstand belohnt wird. Der Goldanteil sagt, ob ein Turm sich lohnt, gemessen am Spieler, der rechnet. Eine Siegquote des naiven Bots nach unten gibt es nicht: Ein Spieler, der nie aufrüstet, darf verlieren.
- **Der gute Bot rechnet Verlangsamen und Warten:** Verlangsamen zählt jetzt als längere Zeit unter den Türmen, die schon stehen: bei halber Geschwindigkeit doppelte Zeit, also der Schaden der anderen Türme dort noch einmal, wenn er stärker verlangsamt als sie. Ein Schadensturm zählt dort, wo schon verlangsamt wird, entsprechend mehr. Gift zählt wie bisher, solange es zwischen zwei Schüssen wirkt, weil der Turm meist dasselbe Ziel trifft. Wie in den Regeln (E-28) addiert sich Gift nicht: Auf einem Punkt des Pfads zählt nur das stärkste, ein weiterer Giftturm bringt nur, was er darüber hinaus vergiftet. Beim Vergleich für ein Upgrade zählt der Turm selbst mit seiner alten Stufe zu den Türmen, die schon stehen. Ein Frostturm verstärkt dann scheinbar ein wenig seinen eigenen Schaden. Das ist eine kleine Unschärfe der Schätzung und bleibt so. Auf einen teureren Plan spart der Bot nur, wenn er je Gold mindestens die Hälfte mehr bringt als das Beste, was er jetzt bezahlen kann. Das ersetzt in E-38 die Faktoren für Verlangsamung und das Sparen auf jeden besseren Plan.
- **Werte:** Gesundheit aller Gegner etwa mal 2,6 (gerundet), Kopfgeld unverändert. Balliste Reichweite 4 (Stufe 3: 4,5), Zwillingsballiste 85 Gold und Reichweite 4,5, Bastion 110 Gold und Schaden 7 bis 10, Hufeisen 140 Gold und Schaden 7 bis 11, Katapult 100 Gold, Giftturm 55 Gold, Reichweite 3,75 (Stufe 3: 4,25) und Gift 0,14, 0,19 und 0,25 je Tick. `ashen_keep` beginnt mit 320 Gold und 12 Leben. Vorher wurden Frostturm, Giftturm und Hufeisen vom guten Bot nie gebaut, Bastion und Katapult nahmen drei Viertel des Golds.
- **Golden Replays:** Die drei Replays von `mire_siege` nutzen den Katalog. Ihre Hashes sind in einem eigenen Commit neu geschrieben, die Regeln sind unverändert, nur die Werte.
- **Endlosmodus (E-36) geprüft:** Wachstum 25 % je Runde und festes Kopfgeld bleiben. Mit den neuen Werten übersteht der naive Bot 0 bis 8 Runden (`mire_siege`, `grey_peaks`, `ashen_keep`, je ein Seed, 30.000 Ticks). Der gute Bot steht nach 30.000 Ticks in Runde 7 bis 9 und auf `mire_siege` nach 60.000 Ticks in Runde 18, ohne Leben zu verlieren. Der Modus trennt die Spieler also. Sehr lange Endlosspiele machen den guten Bot langsam, weil er bei jedem Plan alle Felder bewertet: 200.000 Ticks liefen nicht in elf Minuten durch. Für Menschen spielt das keine Rolle, für die Korridore ist der Endlosmodus deshalb nicht dabei.
- **Nicht:** Die Korridore prüfen nur die beiden Bots. Wie schwer sich eine Karte für Menschen anfühlt, misst das nicht. Karten, Würfel und Münzen (E-34) nutzt keiner der Bots, sie machen das Spiel für Menschen leichter.

## E-40 Übersetzung

- **Aufgabe:** Alle sichtbaren Texte sind übersetzbar, das Spiel bringt Deutsch mit (Auftrag, Sprache). Die Texte im Quelltext sind Englisch.
- **Dateien:** `qt_add_translations` am Ziel `qtd_client` mit `src/client/i18n/qtowerdefense_de.ts` und einer englischen Datei nur für die Pluralformen (`PLURALS_TS_FILE`), damit „1 round“ und „2 rounds“ stimmen. Die `.qm` liegen im Programm unter `:/i18n/`. Neue Texte holt `cmake --build --preset debug --target update_translations` in die Dateien, ohne Zeilennummern, damit Änderungen am Code die Dateien nicht anfassen.
- **Sprachwahl:** `Translation` (C++, in QML) mit `System`, `English`, `German`, in `settings.ini` gespeichert und in den Einstellungen umschaltbar. Ein Knopf schaltet weiter, jede Sprache heißt in ihren eigenen Worten („Deutsch“). `System` heißt Deutsch, wenn `QLocale()` Deutsch spricht, sonst Englisch. Tests setzen `QLocale::setDefault` auf Englisch und sind so auf jedem Rechner gleich. Beim Wechsel ruft `Translation` `QQmlEngine::retranslate()`, alle Texte wechseln sofort. `Translation` installiert die Sprache des Systems schon beim Entstehen, bevor QML Texte liest, und steht als erstes Objekt im Fenster.
- **Anzeigenamen:** Türme, Zauber, Abschnitte und Landschaften zeigt der Client mit übersetzten Namen, nachgeschlagen über ihre ID. Unbekannte IDs aus fremden Inhalten behalten ihren Namen aus den Daten.
- **Fehlertexte:** Lade- und Speicherfehler kommen aus Core und bleiben Englisch, sie richten sich an Autoren der Daten (E-16). Der Client setzt sie in einen übersetzten Satz („Diese Karte lässt sich nicht spielen: …“).
- **Prüfung:** `scripts/check_translations.py` liest die Quellen mit lupdate und der Projektdatei des Debug-Builds und scheitert bei Texten ohne Übersetzung, bei Übersetzungen ohne Text, bei fehlenden Pluralformen und bei Übersetzungen mit anderen Platzhaltern (`%1`, `%n`). Dazu sucht es in QML sichtbare Eigenschaften (`text`, `title`, `caption`, `Accessible.name` …), die auf einen nackten Text gesetzt sind. Was ein Ausdruck baut, sieht es nicht. Es läuft in den Quellregeln der Schnellprüfung.
- **Fenstertitel:** `AppInfo.windowTitle` kommt aus C++ und ist konstant, er wechselt beim Umschalten nicht. Er lautet in beiden Sprachen gleich. Neue Texte aus C++ brauchen ein Signal zum Wechsel.
- **Nicht:** Weitere Sprachen. Eine neue `.ts` in `TS_FILES` und ein Eintrag in `Translation` reichen dafür. Die Fehlertexte in Core werden nicht übersetzt.

## E-41 Neue Modelle, laufende Gegner

Vom Menschen am 2026-11-08 gewünscht: bessere Modelle für Gegner und Türme, Gegner sollen laufen und nicht gleiten.

- **Quellen:** Gegner aus „Ultimate Monsters“ von Quaternius (quaternius.com, Download über den dort verlinkten Google-Drive-Ordner), Türme aus dem „KayKit Medieval Hexagon Pack 1.0“ von Kay Lousberg (GitHub-Repository des Erstellers). Beide CC0. Die Lizenzdatei von Quaternius trägt im Kopf versehentlich den Namen eines anderen Pakets, die Lizenz selbst ist CC0 wie auf der Seite. Felsen, Bäume, Umgebung und Töne bleiben von Kenney.
- **Laufen ohne Skelett:** `scripts/import_assets.py` backt die Laufanimation jedes Gegners (`Walk`, bei Fliegern `Fast_Flying`) in acht feste Meshes (`<name>_0` … `<name>_7`), mit Linear Blend Skinning in `scripts/gltf_mesh.py`. Alle Posen haben den Maßstab der ersten, keine reicht unter den Boden. Der Client zeichnet je Gegnertyp und Pose ein instanziertes Modell (`MapInstances.walkFrame`), jeder Gegner steht in genau einer Pose. Echte Skelett-Animation hätte je Gegner ein eigenes Modell gebraucht und das Instancing aus E-33 aufgegeben.
- **Pose aus dem Weg:** Die Pose folgt `Enemy::travelled`, ein Zyklus sind 0,9 Felder (`kStrideTenths`), versetzt nach Id, damit Nachbarn nicht im Gleichschritt gehen. Verlangsamte Gegner gehen dadurch langsamer, die Füße rutschen kaum, und das Bild hängt nur am Zustand. Bosse sind größer und machen entsprechend längere Schritte. Gegner mit Posen hüpfen nicht mehr. Posen zählen nur vollständig: Fehlt eine, zeichnet der Client wie bisher ein Modell ohne Posen oder eine Form.
- **Kosten:** Zehn Gegner mit je acht Posen sind 80 Meshes, rund 15 MB im Programm, und 80 Modelle statt 10 in der Szene. Leere Posen zeichnen nichts. Alle Monster teilen eine Textur (`monsters.png`).
- **Pakete:** Die Pakete liegen nicht im Repository. `import_assets.py` liest Zips: das GitHub-Zip von KayKit als `kaykit-medieval-hexagon.zip`, die glTF-Ordner von Quaternius als `ultimate-monsters.zip`.
- **Nicht:** Animationen für Treffer, Tod oder Angriff, Schweben der Flieger. Die Zuordnung der Monster zu den Gegnern ist ein erster Wurf und darf sich ändern.

## E-42 Icon, Intro und Ladevorhang

Wunsch des Menschen (2026-11-06, beauftragt 2026-11-08): ein Icon und ein Lade- oder Splash-Screen mit kleinem Intro.

- **Icon:** Ein Steinturm mit Fahne im Eisen-Achteck, in den Farben von `Theme.qml`. Es ist eigener Inhalt: `scripts/make_icon.py` zeichnet es aus Vielecken mit weichen Kanten, jede Größe für sich, ohne Bibliothek. Daraus entstehen `icon.png` (256 Pixel, Fenster und Intro) und `src/app/qtowerdefense.ico` (16 bis 256 Pixel, PNG im ICO, für die Programmdatei über `qtowerdefense.rc`). Das Fenster bekommt es mit `QGuiApplication::setWindowIcon`. Android und iOS brauchen später eigene Icons in ihren Paketen.
- **Intro:** `SplashScreen.qml` liegt über allen Seiten. Beim Start wächst das Emblem herein, darunter der Titel, nach gut zwei Sekunden blendet es aus. Ein Tap beendet es früher. Es deckt den ersten Aufbau des Fensters ab.
- **Ladevorhang:** Beim Start einer Karte, auch bei Neustart und geladenem Spielstand, fällt derselbe Vorhang mit „Die Karte wird geladen …“. Er fällt sofort und blendet nur beim Heben aus. Gezählt wird mit `FrameAnimation`, das im Thread der Oberfläche vor jedem Bild tickt und dabei Bilder anfordert. `frameSwapped` käme aus dem Render-Thread, ein QML-Handler darauf liefe dort. Zwei Bilder lang bleibt die Szene unsichtbar: Das erste mit dem Vorhang allein ist gezeichnet, bevor das nächste mit der Szene Meshes, Texturen und Shader lädt, im threaded wie im basic Render-Loop. Danach zeichnet die Szene drei Bilder hinter dem Vorhang, dann hebt er sich. Solange er hängt, nimmt er alle Taps, Finger und das Mausrad.
- **Steuerschnittstelle:** Mit `--control` gibt es weder Intro noch Vorhang. Bots, Rauchtest und Screenshots wollen das Spiel sehen, und ein Vorhang machte aufeinanderfolgende Bilder gleich. `main.cpp` setzt dazu `AppInfo::setControlled` vor dem Laden des Fensters, QML liest `AppInfo.controlled`. Die Kommandozeile liest nur der Parser.
- **Nicht:** Ein Fortschrittsbalken. Qt Quick 3D lädt im Render-Thread in einem Stück, einen echten Fortschritt gibt es nicht. Ein nativer Splash des Betriebssystems vor dem Start von Qt, auf Android ein Thema der Plattform.

## E-43 Kein Screenshot vor dem ersten Bild

Befund (2026-11-09): Ein Screenshot über die Steuerschnittstelle vor dem ersten Bild des Fensters, danach `startMap`, stürzte ab (`0xC0000005` in `Qt6Gui.dll` bei `QRhi::newTexture`). Im Debug-Build geschah das in jedem Lauf, im Release-Build in etwa der Hälfte der Läufe, im threaded wie im basic Render-Loop. Das Fenster war in diesem Moment nicht sichtbar (`isExposed()` falsch, null Bilder). Für ein solches Fenster zeichnet `grabWindow` das Bild mit einem eigenen Renderer. Danach lädt die erste Karte ihre Texturen offenbar über Zustand, der nicht mehr gilt. Die genaue Ursache in Qt ist nicht geklärt, die Regel umgeht sie.

- **Regel:** `screenshot` antwortet mit einem Fehler („the window is not on the screen yet“), solange das Fenster nicht sichtbar ist. Wer ein Bild will, wiederholt die Anfrage. Der Rauchtest tut das bis zu zehn Sekunden lang.
- **Nicht:** Innerhalb der Anfrage auf das Fenster warten. Das bräuchte eine verschachtelte Ereignisschleife, und darin liefe die nächste Anfrage schon vor der Antwort auf diese.

## E-44 Größe der Bedienung

Aus Etappe 13 (UI-Skalierung, 2026-11-09). Die Einheit von HUD und Weltkarte war schon vorher eine Fingerbreite von 9 mm über die Pixeldichte, mindestens 40 Pixel und höchstens ein Vierzehntel der Fensterbreite. Auf dem Desktop (rund 3,8 Pixel je mm) greift fast immer die Untergrenze, dort ließ sich nichts größer stellen.

- **Einstellung:** `GameSettings.controlSize` mit den Stufen Klein, Normal und Groß (`enum ControlSize`), im Einstellungsbrett als Knopf, der die Stufen durchläuft. Die Faktoren 0,8, 1 und 1,25 stehen in `Theme.controlScales`. Werte außerhalb der Stufen aus der INI-Datei werden zurückgeholt.
- **Rechnung:** `Theme.hudUnit` multipliziert Fingerbreite oder Untergrenze mit dem Faktor, die Fensterbreite begrenzt danach weiter. So wirkt die Stufe auch auf dem Desktop, und ein schmales Fenster bleibt bedienbar. Mit Klein fällt die Einheit auf dem Desktop unter die 40 Pixel (32), das ist gewollt: Die Untergrenze gilt für Normal. HUD und Weltkarte rechnen beide damit, die Formel stand vorher doppelt.
- **Nicht:** Das Hauptmenü und das Einstellungsbrett skalieren nicht mit. Sie füllen ihr Fenster ohnehin nach dessen Höhe (`menuUnitsHigh`), größer ginge nur auf Kosten des Platzes.

## E-45 Qualitätsstufen

Aus Etappe 13 (2026-11-09), nach dem Auftrag: Rendering in einem Budget, das schwächere GPUs erlaubt. Die Spielszene hatte bisher Schatten in hoher Qualität und keine Kantenglättung, die Weltkarte 4-faches MSAA. Es gibt nur ein Licht, die Sonne.

- **Stufen:** `GameSettings.quality` mit Niedrig, Mittel und Hoch (`enum Quality`). Niedrig: keine Schatten, keine Kantenglättung. Mittel: Schatten in mittlerer Qualität mit harten Kanten, 2-faches MSAA. Hoch: Schatten in hoher Qualität mit weichen Kanten (PCF4) wie bisher, 4-faches MSAA. Die Weltkarte hat keine Schatten, bei ihr wirkt nur die Kantenglättung.
- **Vorgabe:** Hoch auf dem Desktop, Mittel auf Android und iOS (`AppInfo.windowed`). Werte außerhalb der Stufen aus der INI-Datei werden zurückgeholt.
- **Eine Stelle:** `RenderQuality.qml` sagt, was eine Stufe einschaltet. `MainScene` und `WorldScene` lesen beide daraus.
- **Einstellungsbrett:** Mit sechs Zeilen passt es in niedrigen Fenstern nicht mehr ganz. Die Zeilen liegen in einer `Flickable`, die nur scrollt, wenn der Platz fehlt. Ein Finger schiebt sie senkrecht, auch über dem Lautstärkeregler: Der hält erst fest, wenn der Finger sich seitwärts bewegt, ein Tippen setzt den Wert beim Loslassen. Ein langes Halten ohne Bewegung setzt nichts, wie bei den Knöpfen. Die Tastatur scrollt die Zeilen nicht zum Fokus mit, das reicht für Touch und Maus.
- **Nicht:** Eine automatische Wahl nach gemessener Bildrate. Dafür fehlt noch die Messung auf echten schwachen Geräten (Mobile-Prüfung).

## E-46 macOS, iOS und Android vorbereitet

Auftrag des Menschen (2026-11-09): die drei Plattformen vorbereiten, ohne etwas zu installieren und ohne zu testen. Nichts davon ist je gebaut worden, alles hier ist ungeprüft.

- **Was gebaut wird:** Auf Android und iOS nur das Spiel. Tests und `qtd_headless` brauchen einen Desktop, dort fragt `find_package` auch Test und QuickTest nicht ab (`QTD_DESKTOP`). Auf macOS alles wie auf Windows.
- **Warnungen:** Außer bei MSVC laufen `-Wall -Wextra -Wpedantic` ohne `-Werror`, bis der erste Build sauber ist. Dann `QTD_WARNINGS_AS_ERRORS` als Vorgabe einschalten.
- **App-Kennung:** `io.github.qtowerdefense` für Bundle und Paket, in `src/app/CMakeLists.txt`. Ein Platzhalter: Vor dem ersten Hochladen in einen Store festlegen, danach lässt sie sich nicht mehr ändern.
- **Icons:** `scripts/make_icon.py` zeichnet auch `qtowerdefense.icns` für macOS, die Launcher-Icons von Android in fünf Dichten und ein Bild von 1024 Pixeln für iOS. Das iOS-Bild liegt auf `IRON_DARK` und ist ein PNG ohne Alphakanal (RGB), denn der App Store lehnt schon den Kanal ab, auch wenn jedes Pixel deckt. Xcode macht die übrigen Größen daraus. Der Katalog liegt als Resource im Bundle.
- **Android:** Kein eigenes Manifest, Qt nimmt seine Vorlage. Name, Paket, Version und Icon kommen aus den Target-Eigenschaften `QT_ANDROID_*`, der Ordner `src/app/android` liefert nur die Icons. Mindest- und Ziel-SDK bleiben bei Qts Vorgabe. Ob `QT_ANDROID_APP_NAME` und `QT_ANDROID_APP_ICON` in Qt 6.12 so heißen, ist nicht nachgeschlagen; beim ersten Build prüfen.
- **Apple:** Die `Info.plist` erzeugen CMake und Qt aus den Eigenschaften `MACOSX_BUNDLE_*`.
- **Presets:** `macos`, `ios` und `android`, die Pfade kommen aus Umgebungsvariablen (`docs/Building.md`).
- **Offen für die Mobile-Prüfung:** Ausrichtung (heute dreht sich das Spiel mit dem Gerät; ob Hochformat taugt, ist ungeprüft), sichere Bereiche um Notch und Kamera, Signieren, Shader-Kosten auf echten Geräten, AAB für den Play Store.

## E-47 Das Spiel im Browser (WebAssembly)

Wunsch des Menschen (2026-11-09): ein Build für den Browser auf GitHub Pages. Am selben Tag hier gebaut (Emscripten in `C:\dev\emsdk`, Qt für WebAssembly über `aqt`) und im eingebauten Browser gespielt: Menü, Weltkarte, Karte mit 3D-Landschaft und Gegnern laufen, die Einstellungen kommen nach dem Neuladen aus IndexedDB zurück. Der Workflow für Pages selbst lief noch nie.

- **Qt-Build:** Qt 6.12 für WebAssembly in der einsträngigen Variante (`wasm_singlethread`) mit Emscripten 5.0.5, der Version, mit der Qt 6.12 gebaut ist (Qt-Doku „Qt for WebAssembly“). Einsträngig, weil Threads `SharedArrayBuffer` und damit die Header COOP und COEP brauchen, die GitHub Pages nicht setzt. Die Simulation braucht keine Threads; Qt Quick zeichnet dann im basic Render-Loop, mit dem Vorhang und Intro schon laufen (E-42).
- **Speichern:** Im Browser liegt das Dateisystem im Speicher und ist nach dem Neuladen leer. `src/app/web_storage.cpp` hängt deshalb den Ordner der App-Daten (Einstellungen, Welt, laufendes Spiel) als IDBFS ein. Vor dem Laden des Fensters kommen die Dateien des letzten Besuchs aus IndexedDB zurück; das Programm wartet darauf mit einem Timer, denn ohne Asyncify kann es nicht blockieren. Geschrieben wird zuerst in den Speicher, alle fünf Sekunden und beim Verlassen der Seite nach IndexedDB. Wer die Seite in den Sekunden nach einem Speichern hart schließt, verliert also höchstens diese. Scheitert das Laden, schreibt der Besuch nichts zurück: Der Abgleich machte IndexedDB sonst so leer wie den Speicher.
- **Wie ein Telefon:** `AppInfo.windowed` ist im Browser falsch. Es gibt keinen Knopf zum Beenden und keinen Vollbild-Schalter, die Qualität steht vorgegeben auf Mittel (E-45). Ohne eigene Fenster geht das Fenster auf Vollbild: im Browser füllt es die Seite statt als 1280×720 mit Titelleiste darin zu stehen, auf Telefonen verdeckt es die Statusleiste.
- **Seite:** Qt schreibt neben das Programm eine HTML-Seite mit Loader. Der Workflow `.github/workflows/pages.yml` kopiert sie als `index.html` mit `.js`, `.wasm` und `qtloader.js` nach Pages. Er baut auf GitHub, nicht hier: Qt für Linux und für WebAssembly über `aqt`, Emscripten über `emsdk`. Er läuft nach einem Push auf `main` oder von Hand, nie gemacht. In den Einstellungen des Repositorys muss Pages auf „GitHub Actions“ stehen.
- **Ton über Web Audio:** Qt 6.12 für WebAssembly enthält kein Qt Multimedia, `QTD_SOUND` ist dort aus. `src/client/web/GameSounds.qml` steht unter demselben Namen für den Typ mit Qt Multimedia und spielt über den Singleton `WebAudio`: Der gibt die OGG-Datei aus den Ressourcen beim ersten Mal an `decodeAudioData` und spielt sie danach mit einem `GainNode` in der Lautstärke ab. Welcher Klang zu welchem Cue gehört und die Kurve der Lautstärke stehen in `Theme` (`cueSounds`, `loudness()`), beide Varianten nutzen sie. Der `AudioContext` entsteht mit dem ersten Klang, und jede Eingabe (Loslassen, Tippen, Taste) weckt ihn wieder: Browser spielen erst nach einer Eingabe. Solange er schläft, spielt nur ein Klang, den eine Eingabe auslöst; die anderen erklängen sonst alle zugleich beim Aufwachen. Anders als auf dem Desktop gilt die Lautstärke für einen ganzen Klang, ein Schieben am Regler ändert erst den nächsten; die Klänge sind kurz. `WebAudio` gibt es in jedem Build, damit `qmllint` das QML des Browsers auf dem Desktop prüfen kann; außerhalb des Browsers spielt es nichts. Wunsch des Menschen am selben Tag. Im eingebauten Browser geprüft, dass ein Klang dekodiert wird und der Kontext läuft; gehört hat ihn noch niemand. Safari spielt OGG/Vorbis erst in neueren Versionen.
- **Was der Bau gezeigt hat:**
  - `QQuick3D::idealSurfaceFormat()` probiert einen OpenGL-Kontext und stürzt im Browser ab, weil es vor der Anwendung läuft. Dort setzt `configureGraphics()` nur Tiefe 24 und Stencil 8, WebGL 2 braucht keine Version.
  - Statisches Qt bringt nur die QML-Module mit, die `qmlimportscanner` findet. Das Programm hat kein eigenes QML, deshalb zeigt `QT_QML_IMPORT_SCANNER_EXTRA_ROOT_PATHS` auf das QML des Clients; ohne fehlte `QtCore` (`Settings`). `QT_QML_ROOT_PATH` daneben ist dasselbe für die Android-Pakete.
  - Der Name des Programms heißt im Browser `this.program`. `main.cpp` setzt ihn deshalb ausdrücklich, sonst hieße der Datenordner so.
  - `/wd4702` für den Code von qmlcachegen gilt nur noch unter MSVC.
- **Offen, mit Risiko:**
  - Größe: Das `.wasm` hat optimiert 53 MB, davon 17 MB Assets. Der erste Start lädt lange und kompiliert die Shader im Browser.
  - `wasm-opt` aus Emscripten stürzte hier zweimal ab und lief beim dritten Mal durch. Ob das am Rechner liegt (siehe die Abstürze von MSVC) oder an Binaryen, ist offen; auf GitHub zeigt es sich beim ersten Lauf.
  - Bildrate im Browser und Verhalten auf Telefonen im Browser sind ungemessen.

## E-48 Lizenz: GPL-3.0-or-later

Entscheidung des Menschen (2026-11-09): Der Code steht unter der GNU General Public License, Version 3 oder nach Wahl jeder späteren Version (SPDX `GPL-3.0-or-later`). Das ersetzt den Satz „Projekt vorerst intern, keine Lizenzdatei" aus Abschnitt 7 des Auftrags (Annahmen).

- **Datei:** `LICENSE` ist der unveränderte Text von gnu.org (`gpl-3.0.txt`). Die README nennt die Lizenz.
- **Nötig, nicht nur verträglich:** Qt Quick 3D, Qt Shader Tools und Qt Quick Timeline gibt es als Open Source nur unter GPL-3.0, nicht unter LGPL. Ein Spiel mit Qt Quick 3D muss deshalb unter GPL-3.0 stehen, solange keine kommerzielle Qt-Lizenz dahintersteht. Die Assets unter CC0 passen dazu.
- **Quelltext zum Browser-Build:** Der Build für GitHub Pages bindet Qt statisch ein. Wer das Programm weitergibt, muss den ganzen zugehörigen Quelltext anbieten (GPL-3.0 §6): unseren im selben Repository, den von Qt als Verweis auf die Quellen genau dieser Version (Qt 6.12.0, `download.qt.io/archive/qt/6.12/6.12.0/single/`). Beides nennt die README. Die Seite selbst verweist noch nicht darauf; das kommt, bevor der Workflow zum ersten Mal veröffentlicht.
- **„Oder später":** Wie es die FSF empfiehlt. Qt steht nur unter Version 3 (`GPL-3.0-only`); das fertige Programm mit Qt wird deshalb unter Version 3 weitergegeben, unser Code allein darf später auch unter einer neueren Version stehen.
- **Rechteinhaber:** „Copyright (C) 2026 The QtowerDefense authors", Wunsch des Menschen. Die Zeile steht in der README, über dem Hinweis auf die Lizenz.
- **Offen:** ob Quelldateien Köpfe mit SPDX-Kennung bekommen. Bis dahin gilt, was die README sagt.

## E-49 Push zum Mac

Entscheidung des Menschen (2026-11-11): Der Stand geht über den Mac nach GitHub, wie bei QtCraft. Das ersetzt den Satz „rein lokal: keine Remotes, kein Push" aus dem Abschnitt Git des Auftrags.

- **Weg:** Remote `mac` ist `qtcraft-mac:qtowerdefense-sync.git`, ein leeres Repository auf dem Mac. Der SSH-Zugang dorthin erlaubt nur Git, und das Repository nimmt nur Branches unter `desktop/` an. `dev` geht deshalb als `desktop/dev` hinüber: `git push mac dev:refs/heads/desktop/dev`.
- **GitHub nur vom Mac:** Von hier geht nichts direkt nach GitHub. Der Agent auf dem Mac holt den Stand ab, legt das Repository auf GitHub an und bereitet das Release vor.
- **Unverändert:** Gepusht wird nur `dev` nach einem Merge, keine Feature-Branches. Nach `main` merged nur der Mensch.
- **Erster Push:** `dcf9d9f` mit Stage 13, ohne grüne volle Prüfung: Die Platte war voll, der Mensch wollte den Stand trotzdem auf dem Mac.

## E-50 Release 1.0.0 auf GitHub

Entscheidung des Menschen (2026-11-11): Das Spiel erscheint als Version 1.0.0 öffentlich unter `DeiVadder/QtowerDefense` auf GitHub. GitHub Actions baut Windows, macOS, Linux und den Browser; GitHub Pages zeigt die Browser-Version. Die Dateien für Agenten (`CLAUDE.md`, Auftrag, `.claude/`, `tools/claude-plugins/`) werden mitveröffentlicht. Das ergänzt E-49: Der Mac pusht nach GitHub, der Windows-Rechner weiter nur zum Mac. Die Regel „nur MSVC" gilt weiter für diesen Rechner; auf GitHub bauen auch GCC (Linux) und Clang (macOS, Browser).

- **Workflow:** `.github/workflows/build.yml` ersetzt `pages.yml`. Jeder Push auf `main` oder `dev`, jeder Pull Request und jeder Tag baut alle vier Plattformen, führt die Tests mit den Labels `core`, `automation` und `golden` aus (die Golden Replays prüfen so dieselben Hashes aus GCC und Clang) und packt je Plattform ein Paket. Die Client-Tests laufen dort nicht, die Runner haben keine GPU. Ein Push auf `main` veröffentlicht die Browser-Version auf Pages (die Umgebung `github-pages` erlaubt nur den Standard-Branch). Ein Tag `v*` legt ein GitHub-Release mit den vier Paketen an; der Tag muss zur Version in `CMakeLists.txt` passen, die Notizen kommen aus dem Abschnitt der Version in `CHANGELOG.md`.
- **Pakete:** `cmake --install` mit `qt_generate_deploy_qml_app_script()` legt Qt, Plugins und QML-Module neben das Spiel (Windows, Linux) oder in das Bundle (macOS). Windows und macOS als `.zip`, Linux als `.tar.gz`, der Browser als `.zip` der Seite. Nur das Spiel, ohne `qtd_headless` und Tests. Unter Windows legt `windeployqt` die Laufzeit von MSVC als `vc_redist.x64.exe` bei, die README sagt, wann man ihn braucht. Die Desktop-Pakete enthalten die FFmpeg-Bibliotheken von Qt Multimedia (LGPL); der Verweis auf die Quellen von Qt deckt sie ab.
- **macOS:** Universal (`arm64;x86_64`), Mindestversion 14.4 wie Qt 6.12 selbst. Ohne Konto bei Apple ist das Bundle nur ad hoc signiert und nicht notarisiert; die README erklärt das Öffnen. Am 2026-11-11 hier erstmals gebaut: ohne Warnungen, das gepackte Bundle startet und spielt (Steuerschnittstelle mit Screenshot). Von den Tests scheitert nur `tst_qml` an vier Prüfungen, die Pixel der 3D-Szene lesen (siehe `FORTSCHRITT.md`).
- **Linux:** Neues Preset `linux`, gebaut auf Ubuntu 24.04 mit GCC. Warnungen brechen dort den Build nicht ab (`QTD_WARNINGS_AS_ERRORS` nur bei MSVC). Hier nie gebaut, der erste Lauf auf GitHub zeigt es.
- **Quelltext zur Browser-Seite (E-48):** Der Workflow setzt auf die Seite einen Verweis auf dieses Repository und auf die Quellen von Qt 6.12.0. Die Desktop-Pakete enthalten `LICENSE` und `README.md` mit denselben Verweisen.
- **Autor:** Am 2026-11-11 mit der neutralen Identität `QtowerDefense Dev <dev@qtowerdefense.invalid>` veröffentlicht, die History ist unverändert. Der Mensch hat am selben Tag entschieden, sie zu behalten; das erledigt den Satz zur Identität im Auftrag (Abschnitt Git).
- **Einmal von Hand in GitHub:** `main` aus `dev` anlegen und als Erstes pushen, damit `main` Standard-Branch wird (sonst lehnt die Umgebung `github-pages` das Veröffentlichen ab). Pages auf „GitHub Actions“ stellen.
- **Windows-Tests auf GitHub:** Der Workflow legt `bin` von Qt in den `PATH`, wie `scripts/env.cmd` hier.
- **Erster Lauf (2026-11-11):** macOS, Linux, Browser und Pages grün, die Golden Replays geben auch mit GCC dieselben Hashes. Windows scheiterte an aqt, siehe unten.
- **Offen, mit Risiko:** Die README beschreibt die Pakete schon so, als gäbe es sie; vor dem Tag den ersten grünen Lauf abwarten. Die Actions stehen auf den Hauptversionen von 2025 und laufen teils auf Node 20, das GitHub 2026 ausmustert; zeigt der Log Warnungen, die Versionen anheben. `aqt` 3.3.0 kennt das Windows-Repository von Qt 6.12 nicht (seit 6.12 hat dort jeder Compiler einen eigenen Ordner, `qt6_6120/qt6_6120_msvc2022_64`; aqt sucht `qt6_6120/qt6_6120` und scheitert an der Prüfsumme von `Updates.xml`). Für Windows lädt deshalb `.github/scripts/install_qt_windows.py` Qt direkt von download.qt.io, mit SHA-256 für `Updates.xml` und jedes Archiv, die Prüfsummen nur direkt von download.qt.io; jedes Archiv landet, wo die Operation „Extract“ seines Pakets es hinlegt; kann eine neuere aqt-Version das Layout, kann das Skript weg. macOS, Linux und der Browser bleiben bei aqt. Schattiert unter Windows das `link` von Git Bash den Linker von MSVC, `CMAKE_LINKER` setzen. Die Namen von `aqt` (`win64_msvc2022_64`, `clang_64`, `linux_gcc_64`) und die Pakete für Linux können abweichen; der Log listet die angebotenen Namen.
