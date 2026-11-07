# Sprache und Befehlsgruppen

Der Compiler übersetzt **GFA-BASIC 3.0/3.5 (Amiga)** in der ASCII-Form (`.LST`). Grundlage ist die
Befehlsreferenz `GFA-BASIC_3_5_Amiga_Befehlsreferenz.md`, die aus dem Handbuch (GFA Systemtechnik,
1988, mit den 3.5-Kapiteln 13 und 14) extrahiert wurde. Sie liegt derzeit nicht in diesem Repository;
ihr Anhang C sammelt OCR-Unsicherheiten und Widersprüche, die vor der Implementierung eines Befehls
zu prüfen sind.

Das tokenisierte `.GFA`-Format wird nicht gelesen (es ist im Handbuch nicht beschrieben); Programme
werden im Original mit `SAVE,A` als `.LST` gespeichert.

## Befehlsgruppen

Jeder Befehl und jede Funktion gehört zu genau einer Gruppe. Eine Plattform unterstützt eine Menge
von Gruppen; benutzt ein Programm eine Gruppe, die das Zielsystem nicht hat, bricht der Compiler mit
einer Meldung ab (statt Stubs zu erzeugen, die zur Laufzeit stillschweigend nichts tun).

| Gruppe | Inhalt (Kapitel der Referenz) |
|---|---|
| `core` | Variablen und Typen, Zuweisung, Operatoren, Ganzzahl- und Bitfunktionen, Arrays, `DIM`/`ERASE`/`INSERT`/`DELETE`/`SWAP`/`QSORT`, Kontrollfluss, Prozeduren/Funktionen, `DATA`/`READ`, Fehlerbehandlung (Kap. 2, 3, 4 teilweise, 7) |
| `float` | Fließkomma-Arithmetik und -Funktionen, `RND`, `FACT`/`VARIAT`/`COMBIN` (Kap. 4, 14) |
| `string` | String-Variablen und -Funktionen, `MKx$`/`CVx` (Kap. 2, 4) |
| `print` | `PRINT`, `PRINT USING`, `WRITE`, `TAB`, `POS`, `DEFNUM`, `MODE`, `LOCATE`, `PRINT AT` (Kap. 5) |
| `input` | `INPUT`, `LINE INPUT`, `FORM INPUT`, `INKEY$` (Kap. 5) |
| `file-seq` | `OPEN` (sequentiell), `PRINT #`, `INPUT #`, `LINE INPUT #`, `WRITE #`, `CLOSE`, `EOF`, Dateiverwaltung (Kap. 6) |
| `file-random` | wahlfreier Zugriff: `FIELD`, `GET`, `PUT`, `SEEK`, `LOF`, mehrere Kanäle gleichzeitig (Kap. 6) |
| `memory` | `PEEK`/`POKE`, `VARPTR`/`V:`, `*x`, `BYTE{}` …, `ABSOLUTE`, `MALLOC`/`MFREE`, `BMOVE` (Kap. 2) |
| `graphics` | Grafikbefehle aus Kap. 8 |
| `gui` | Screens, Fenster, Menüs, Requester, Ereignisse (Kap. 9) |
| `sprites` | Sprites, BOBs, Joystick, Maus (Kap. 5, 10) |
| `sound` | `SOUND`, `WAVE`, Sprachausgabe (Kap. 5, 10) |
| `matrix` | Vektoren und Matrizen (Kap. 13, nur 3.5) |
| `amiga-os` | direkte Aufrufe der AmigaOS-Bibliotheken (Kap. 11), `GURU` |

Die Gruppen sind in `include/basic/target.hpp` definiert. Die genaue Zuordnung jedes einzelnen
Befehls entsteht mit dem Frontend als Tabelle in `src/frontend`.

## Unterstützung je Zielsystem

Geplanter Umfang (✓ geplant, – nicht vorgesehen, (✓) mit Einschränkung):

| Gruppe | pc | amiga | cpc | z80-test | m68k-test |
|---|---|---|---|---|---|
| core | ✓ | ✓ | ✓ | ✓ | ✓ |
| float | ✓ | ✓ | ✓ | ✓ | ✓ |
| string | ✓ | ✓ | ✓ | ✓ | ✓ |
| print | ✓ | ✓ | ✓ | ✓ | ✓ |
| input | ✓ | ✓ | ✓ | – | – |
| file-seq | ✓ | ✓ | (✓) eine Ein- und eine Ausgabedatei | – | – |
| file-random | ✓ | ✓ | – | – | – |
| memory | (✓) emulierter Speicher | ✓ | (✓) Little-Endian, 16-Bit-Adressen | ✓ | ✓ |
| graphics | später | ✓ | (✓) Firmware-Grafik | – | – |
| gui | – | ✓ | – | – | – |
| sprites | – | ✓ | – | – | – |
| sound | – | ✓ | – | – | – |
| matrix | ✓ | ✓ | ✓ | ✓ | ✓ |
| amiga-os | – | ✓ | – | – | – |

## Reihenfolge der Umsetzung

1. Sprachkern: Integer, Float, Strings, `PRINT`, Schleifen, `IF`/`SELECT`, Prozeduren mit `LOCAL` und
   `VAR`, `DATA`/`READ` – komplett durch alle Stufen und auf allen Zielen.
2. Arrays, String-Funktionen, `PRINT USING`, Fehlerbehandlung mit `ON ERROR`/`RESUME`.
3. `INPUT`, Dateien.
4. `memory`, `matrix`.
5. Plattformspezifisch: Grafik (Amiga, CPC), dann GUI, Sprites, Sound und Kapitel 11 (Amiga).

## Offene Sprachfragen

Diese Punkte sind aus dem Handbuch nicht sicher zu entnehmen und werden am Original geklärt
(`tests/probes/*.lst`, Ablauf in [gfa-original.md](gfa-original.md), Ergebnisse unter
`tests/probes/results`). Bestätigtes wird als Testprogramm unter `tests/programs` festgehalten.

- ~~**Arithmetik in Ausdrücken:**~~ Ausdrücke rechnen ohne 32-Bit-Überlauf (wie in Float); erst die
  Zuweisung an `%`/`&`/`|` prüft den Bereich (Fehler 2/4/3), die Zielvariable bleibt dann unverändert.
  `INC`/`DEC` brechen ohne Fehler um (probe1 T1–T6, probe2 T1–T8, Details in
  [numerics.md](numerics.md#ganzzahlen)).
- ~~**`FOR … NEXT`**~~ (probe1 T18–T21, probe2 T10–T14), entgegen dem Handbuch (Anhang C, Punkt 15):
  - `FOR i%=5 TO 1` läuft **nicht** (0 Durchläufe); die Laufvariable hat danach den Startwert 5. Die
    Bedingung wird also vor dem ersten Durchlauf geprüft.
  - Nach `FOR i%=1 TO 3` ist `i%` = 4 (Endwert + Schrittweite).
  - Der Endwert wird **einmal** beim Eintritt ausgewertet: Eine Änderung von `e%` in der Schleife
    ändert die Zahl der Durchläufe nicht.
  - `FOR x=0 TO 1 STEP 0.1` läuft 10-mal (Folge der 48-Bit-Mantisse, siehe
    [numerics.md](numerics.md#fließkommaformat)).
  - `FOR i%=5 TO 1 STEP -1`: 5 Durchläufe, danach `i%` = 0. `FOR i%=1 TO 10 STEP 3`: 4 Durchläufe,
    danach `i%` = 13. `FOR x=1 TO 0 STEP -0.25`: 5 Durchläufe, danach `x` = −0,25.
  - Auch die Schrittweite wird **einmal** beim Eintritt ausgewertet.
  - Am Bereichsende bricht die Laufvariable ohne Fehler um: `FOR i%=2147483646 TO 2147483647` läuft
    2-mal, danach ist `i%` = −2147483648 (die Schleife endet trotzdem).
- **Zahlenausgabe:** geklärt bis auf `MODE` (kein Leerzeichen vor positiven Zahlen, `0.5`, 13
  Stellen, Exponent ab 10¹⁴ und unter 10⁻⁵, `DEFNUM`, Kommazonen zu 16 Spalten;
  [numerics.md](numerics.md#zahl--text)).
- ~~**`CINT`:**~~ rundet bei genau ,5 von 0 weg (probe1 T16).
- **Fehlernummern** (Anhang C, Punkt 17), am Original ermittelt, Text wie von `ERR$` geliefert:

  | Nr. | `ERR$` | Auslöser |
  |---|---|---|
  | 0 | divide by zero | `1/0`, `1 DIV 0` (probe1 T14, T15) |
  | 2 | no integer\|-2147483648 .. 2147483647 | Überlauf bei Zuweisung an `%` (probe1 T1) |
  | 3 | no byte\|0 .. 255 | Überlauf bei Zuweisung an `\|` (probe2 T6) |
  | 4 | no word\|-32768 .. 32767 | Überlauf bei Zuweisung an `&` (probe1 T4) |
  | 5 | no negative squareroot | `SQR(-1)` (probe1 T51) |
  | 6 | logarithm only for\|numbers greater zero | `LOG(0)` (probe1 T52) |
  | 16 | array index out of range | `f%(10)` bei `DIM f%(5)` (probe1 T53) |
  | 24 | file not open | `PRINT #1` nach einem abgefangenen Fehler, siehe unten |

  `|` trennt im Original die Zeilen der Fehlermeldung.
- **Dateikanäle nach einem Fehler:** Nach einem mit `ON ERROR GOSUB` abgefangenen Fehler ist ein
  offener Kanal unbrauchbar: `PRINT #1` meldet Fehler 24 „file not open“, `OPEN "A",#1` dagegen
  „file already open“. Erst `CLOSE #1` und erneutes `OPEN "A"` machen ihn wieder nutzbar; was vorher
  geschrieben wurde, bleibt erhalten (Versuche beim Lauf von probe1, siehe
  [gfa-original.md](gfa-original.md#regeln-für-prüfprogramme)). Ob das auch ohne `ON ERROR` oder nur
  für bestimmte Fehler gilt, ist offen.
- **`SELECT` mit Mehrzeichen-Strings:** Packung der Zeichen (Anhang C, Punkt 8).
- **Ausgabe auf dem Amiga:** Ein GFA-Programm gibt in ein Fenster aus (öffnet es bei Bedarf). Für
  den ersten Stand schreibt das kompilierte Programm auf `Output()` der Shell; Fensterausgabe folgt mit
  der Gruppe `gui`.
