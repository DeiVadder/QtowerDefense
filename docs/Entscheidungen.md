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
- **Empfehlung:** In Etappe 1 mit dem Festkomma-Code erneut bewerten. Aussichtsreich ist ein LLVM-Release und `-fsanitize=undefined -fsanitize-trap=undefined` nur für Core-Tests; der Trap-Modus braucht keine Laufzeit. Bis dahin gilt der Ersatz aus Auftrag Abschnitt 4 (Warnstufe, clang-tidy, Überlaufprüfungen in Debug).
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
- **Was:** `bugprone-narrowing-conversions` ist seit dem Review aktiv, das Gerüst ist damit sauber (eine Probe mit `long long` nach `int` meldet den Fehler). Die Zahl in Klammern nennt die Treffer im Gerüst, gemessen am 2026-11-04 mit clang-tidy 19.1.5. Ein Selbsttest verlangt für jeden Ausschluss einen Eintrag hier.
- **Warum, je Ausschluss:**
  - `bugprone-easily-swappable-parameters` (0): Trifft jede Funktion mit zwei gleichartigen Parametern, in der Simulation also Koordinaten und Festkomma-Paare. Die Schwelle ließe sich nur raten; Verwechslungen fangen eigene Typen und Tests besser.
  - `performance-enum-size` (0): Verlangt kleine Basistypen für jedes `enum`. Der Speichergewinn ist hier ohne Bedeutung, und in Replays und JSON zählt ein einheitlicher Basistyp mehr.
  - `modernize-use-trailing-return-type` (18): Reiner Stil, der Qt-Code und das Projekt schreiben Rückgabetypen vorne.
  - `modernize-use-nodiscard` (0): Würde fast jede const-Methode markieren. `[[nodiscard]]` steht dort von Hand, wo Ignorieren wirklich ein Fehler ist.
  - `modernize-avoid-c-arrays` (1): Der Treffer ist das absichtliche Feld in `tests/asan/asan_selfcheck.cpp`. Der Test braucht genau diesen Überlauf eines rohen Felds.
  - `modernize-use-designated-initializers` (2): Bei kleinen Wertetypen wie `Version{1, 2, 3}` bringt es nur Länge, die Reihenfolge ist durch Tests gesichert.
  - `modernize-use-auto` (0): `auto` bleibt eine Entscheidung im Einzelfall, in der Simulation soll der Typ sichtbar sein.
  - `modernize-return-braced-init-list` (0): Reiner Stil, nennt den Typ im `return` nicht mehr.
  - `misc-include-cleaner` (42): Verlangt für jeden Typ einen eigenen Include, auch für `QChar` oder `QStringView`, die `<QString>` ohnehin mitbringt, und für alles aus einem eigenen Header. Das bläht die Include-Listen auf, ohne dass ein Fehler verhindert wird.
  - `misc-non-private-member-variables-in-classes` (3): Trifft reine Wertetypen wie `Version`, deren öffentliche Felder gewollt sind.
  - `misc-no-recursion` (0): Rekursion ist erlaubt, etwa beim Durchlaufen von Daten. Tiefe und Abbruch prüfen Tests.
  - `misc-use-anonymous-namespace` (0): `static` und anonymer Namensraum sind beide zulässig, dazu prüft `misc-use-internal-linkage` weiter die Bindung.
- **Verworfen:** Ausschlüsse ohne Begründung lassen. Die Checks mit 0 Treffern jetzt schon zuschalten: Das wäre eine Stilentscheidung ohne Code, an dem sie sich messen ließe. Mit Etappe 1 neu ansehen.

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
