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
- **Zwei Karten:** `training_ground` behält die kurze Welle, weil Tests und Replays von ihr leben und der naive Bot sie gewinnt. `mire_siege` hat alle Gegner und Bosse, das Spiel startet dort, bis die Menüs aus Etappe 11 eine Wahl bieten. Beide Dateien nennen ihre Türme selbst. Ein gemeinsamer Katalog lohnt sich erst mit weiteren Karten.
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
- **Knöpfe:** Speichern und Laden stehen nur in der Pause da, denn am Rand verdecken sie Felder und den Anfang des Pfades. Ein Menü kommt mit Etappe 11.
- **Bots:** Der Fuzz-Bot ruft Wellen, der naive nicht. Ihn mutiger zu machen, gehört zum guten Bot aus Etappe 12.
- **Noch offen:** Kampagnenfortschritt, Highscores und Einstellungen aus dem Auftrag kommen mit Etappe 11, denn es gibt sie noch nicht. Das Spieltempo merkt sich der Spielstand nicht.
- **Formatversion:** Bleibt 1, der neue Schlüssel ist optional.

## E-32 Weltkarte der Kampagne

Vom Menschen am 2026-11-06 festgelegt, ergänzt den Auftrag.

- **Eine zusammenhängende Welt:** Die Kampagne zeigt eine große Weltkarte im Nebel, in der man den nächsten Abschnitt wählt. Einzelne Assets ragen aus dem Nebel. Kein Menü mit einer Kartenliste.
- **Die Welt entspricht den Levels:** Ist ein Abschnitt auf der Weltkarte ein Sumpf, ist auch das Level ein Sumpf. Beide nutzen dieselben Assets je Landschaft.
- **Freischalten:** Ein geschafftes Level öffnet seine Nachbarn und lichtet dort den Nebel. Das erlaubt Verzweigungen und ersetzt „Karten nacheinander freischalten" aus dem Auftrag. Die Sterne je Karte bleiben.
- **Größe:** Sechs Abschnitte, auch das weicht vom Auftrag ab (3 bis 5 Karten). Weniger wirken in einer großen Welt leer, und jede Karte kostet Balancing in Etappe 12. Form: ein Anfang, zwei Wege mit je zwei Abschnitten, ein gemeinsames Ende. `training_ground` und `mire_siege` sind zwei davon, vier Karten sind neu. Die Landschaften wählt Etappe 10 nach den verfügbaren CC0-Assets.
- **Daten und Schichten:** Die Weltkarte ist eine JSON-Datei (Abschnitte, Lage, Karte, Nachbarn). Regeln und Fortschritt liegen in C++, Nebel und Szene sind reine Darstellung. Die Simulation ändert sich nicht.
- **Nebel:** Zuerst halbtransparente Ebenen oder Sprites, ein eigener Shader nur, wenn das nicht reicht. Er muss auch auf Mobile laufen.
- **Reihenfolge:** Etappe 10 sucht die Assets je Landschaft schon mit Blick auf die Weltkarte, also Gelände und große Landmarken. In Etappe 11 wird die Weltkarte ein eigenes Feature, damit Sterne, Fortschritt und Endlosmodus nicht an ihr hängen.
