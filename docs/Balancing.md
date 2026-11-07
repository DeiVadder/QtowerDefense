# Balancing

Die Werte von Türmen, Gegnern und Karten werden nicht nach Gefühl gesetzt, sondern gegen
Zielkorridore geprüft (E-39). `scripts/balancing.py` lässt den naiven und den guten Bot
(E-38) jede Karte der Weltkarte mit vielen Seeds spielen, liest die Korridore aus der Tabelle
unten und scheitert, wenn eine Kennzahl herausfällt. Die Vollprüfung ruft das Skript mit dem
Release-Build auf.

```
python scripts/balancing.py --build build/release --runs 50
```

## Was die Korridore sagen

- **Der gute Bot gewinnt.** Auf jeder Karte mindestens 90 % der Spiele. Er sieht nur, was ein
  Mensch sieht. Gewinnt er nicht, ist die Karte für Menschen zu schwer.
- **Der naive Bot gewinnt nicht immer.** Er baut nur Balliste neben Balliste und rüstet nie
  auf. Gewinnt er die Kampagne fast immer, fordert sie nicht. Das Übungsgelände muss er aber
  gewinnen, es soll jeder schaffen.
- **Kein Turm ist dominant oder nutzlos.** Gemessen am Gold, das der gute Bot über die Kampagne in
  stehende Türme steckt: Jeder Turm bekommt mindestens 3 % und höchstens 40 % davon, und jeder
  steht auf mindestens zwei Karten. Ein Turm, den der gute Bot nie baut, lohnt sich nicht.
- **Das Gold fließt.** Der Mittelwert der Goldkurve des guten Bots (E-38) liegt je Karte
  zwischen 10 und 100. Darunter fehlt Gold für jede Entscheidung, darüber gibt es nichts mehr zu
  entscheiden.

Die Kampagne sind die Karten der Weltkarte, die den gemeinsamen Katalog nutzen (E-37). Das
Übungsgelände hat eigene Türme und zählt bei der Turmnutzung nicht mit.

## Korridore

`metric` ist `win_rate`, `spend_share`, `maps_used` oder `mean_gold`. `scope` ist `each_map`
(jede Karte der Weltkarte für sich), `campaign` (alle Kampagnenkarten zusammen), `each_tower`
(jeder Turm des Katalogs über die Kampagne) oder die Kennung einer Karte. Die Grenzen gelten
einschließlich.

<!-- corridors begin -->
| metric      | bot   | scope           | min  | max |
|-------------|-------|-----------------|------|-----|
| win_rate    | good  | each_map        | 0.9  | 1   |
| win_rate    | naive | campaign        | 0    | 0.8 |
| win_rate    | naive | training_ground | 1    | 1   |
| spend_share | good  | each_tower      | 0.03 | 0.4 |
| maps_used   | good  | each_tower      | 2    | 8   |
| mean_gold   | good  | each_map        | 10   | 100 |
<!-- corridors end -->

## Stand

Messung mit 10 Seeds je Karte nach dem Abstimmen (Etappe 12):

| Karte            | naiv gewonnen | gut gewonnen | Leben gut (von) |
|------------------|---------------|--------------|-----------------|
| training_ground  | 10/10         | 10/10        | 5 (5)           |
| mire_siege       | 10/10         | 10/10        | 12 (15)         |
| whispering_pines | 10/10         | 10/10        | 15 (15)         |
| grey_peaks       | 0/10          | 10/10        | 12 (12)         |
| heather_downs    | 10/10         | 10/10        | 15 (15)         |
| red_gulch        | 6/10          | 10/10        | 10–15 (15)      |
| barrow_fields    | 10/10         | 10/10        | 12 (12)         |
| frost_gate       | 0/10          | 10/10        | 12 (12)         |
| ashen_keep       | 10/10         | 10/10        | 1–11 (12)       |

Der naive Bot gewinnt 56 von 80 Kampagnenspielen (70 %). Goldanteile des guten Bots über die
Kampagne: Balliste 28 %, Zwillingsballiste 26 %, Frostturm 18 %, Bastion 14 %, Giftturm 5 %,
Katapult 4 %, Hufeisen 4 %.
