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
