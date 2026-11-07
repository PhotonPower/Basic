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
  | 1 | overflow | Fließkomma-Überlauf, z. B. `EXP(30000)` (probe3 T16, T18) |
  | 2 | no integer\|-2147483648 .. 2147483647 | Überlauf bei Zuweisung an `%` (probe1 T1) |
  | 3 | no byte\|0 .. 255 | Überlauf bei Zuweisung an `\|` (probe2 T6) |
  | 4 | no word\|-32768 .. 32767 | Überlauf bei Zuweisung an `&` (probe1 T4) |
  | 5 | no negative squareroot | `SQR(-1)` (probe1 T51) |
  | 6 | logarithm only for\|numbers greater zero | `LOG(0)` (probe1 T52) |
  | 10 | string too long\|max. 32767 chars | `LEFT$("abc",-1)` (probe4 T45) |
  | 14 | redimensioned array | zweites `DIM` desselben Arrays (probe9 T3) |
  | 15 | array undimensioned | Zugriff ohne `DIM` (probe9 T4) |
  | 16 | array index out of range | `f%(10)` bei `DIM f%(5)` (probe1 T53) |
  | 17 | dim out of range | (Text aus `ERR$`, probe9 T8) |
  | 18 | wrong # of indices | (Text aus `ERR$`, probe9 T8) |
  | 34 | out of data | (Text aus `ERR$`, probe9 T9) |
  | 35 | data not numeric | `READ x` bei `DATA abc`; der Datenzeiger bleibt stehen (probe9 T1, T2) |
  | 67 | asin/acos error | `ASIN(2)`, `ACOS(-1.5)` (probe13 T5, T6) |
  | 24 | file not open | `PRINT #1` nach einem abgefangenen Fehler, siehe unten |

  `|` trennt im Original die Zeilen der Fehlermeldung. Alle Texte 0–35 stehen in
  `src/ir/interp.cpp` (`errorText`). `ERR` ist ein vorzeichenbehaftetes Byte: Nach `ERROR 200` ist
  `ERR` = −56, `ERR$(-56)` = „unknown error 200“ (probe9 T6).
- **`VAR`-Parameter** (probe10): `VAR` gilt für **alle folgenden** Parameter (`p(VAR a,b)`: beide;
  `p(a,VAR b)`: nur `b`). Ein `VAR`-Parameter ist eine echte Referenz: Eine Zuweisung wirkt sofort
  auf die Variable des Aufrufers. Als Argument ist nur eine ganze Variable gleichen Typs erlaubt;
  Konstante, Feldelement oder anderer Typ ergeben im Original Fehler 68 „var-type mismatch“, ein
  Ausdruck Fehler 45 „too many parameters“. Der Compiler weist das schon beim Übersetzen zurück.
- **Dateikanäle nach einem Fehler:** Nach einem mit `ON ERROR GOSUB` abgefangenen Fehler ist ein
  offener Kanal unbrauchbar: `PRINT #1` meldet Fehler 24 „file not open“, `OPEN "A",#1` dagegen
  „file already open“. Erst `CLOSE #1` und erneutes `OPEN "A"` machen ihn wieder nutzbar; was vorher
  geschrieben wurde, bleibt erhalten (Versuche beim Lauf von probe1, siehe
  [gfa-original.md](gfa-original.md#regeln-für-prüfprogramme)). Ob das auch ohne `ON ERROR` oder nur
  für bestimmte Fehler gilt, ist offen.
- ~~**`RESUME` nach Fehlern in Kopfzeilen**~~ (probe11, 2026-11-07): `RESUME NEXT` geht zur
  **nächsten Programmzeile**, auch in einen Rumpf hinein; `RESUME` wiederholt nur die fehlerhafte
  Zeile. Im Einzelnen:

  | Fehler in | `RESUME NEXT` geht weiter mit |
  |---|---|
  | `WHILE`-Bedingung (T1) | erster Zeile des Rumpfs; `WEND` prüft wieder |
  | `IF`-Bedingung (T2) | dem `THEN`-Zweig, danach hinter `ENDIF` |
  | `ELSE IF`-Bedingung (T3) | diesem Zweig |
  | `FOR`-Zeile (T4, T12) | dem Rumpf (einmal); `NEXT` erhöht und verlässt die Schleife |
  | `DO WHILE`/`DO UNTIL` (T6) | dem Rumpf; `LOOP` prüft wieder |
  | `UNTIL`, `LOOP UNTIL` (T5, T7) | der Zeile hinter der Schleife |
  | `SELECT`-Ausdruck (T8) | der Zeile hinter `ENDSELECT` (kein Zweig) |

  Bei einem Fehler in der Schrittweite rechnet `NEXT` im Original mit einem zufälligen Wert (T12:
  `i%` = 12782135, beim nächsten Lauf 12782147); wir nehmen 1 ([D19](decisions.md#d19)).
  Bestätigt durch `tests/programs/resume.lst`.
- **Fehler in `DEFFN` und in der Fehlerroutine** (errors2, 2026-11-07): Ein Fehler in einer
  `DEFFN`-Formel gehört zur aufrufenden Anweisung; den Parameter setzt das Original danach **nicht**
  zurück (`q` behält den Wert aus dem Aufruf). Ein zweiter Fehler in der Fehlerroutine nach einem
  neuen `ON ERROR GOSUB` lässt das Original abstürzen („Software error – task held“). Beides bilden
  wir nicht nach ([D19](decisions.md#d19)).
- ~~**`ERR` nach `RESUME`**~~ (probe12 T1): `RESUME` setzt `ERR` auf 0 (in der Fehlerroutine 7,
  danach 0, ohne Dateibefehle in der Routine). Ob auch das Ende der Fehlerroutine ohne `RESUME`
  `ERR` löscht, ist offen (probe12 T2 lief nicht durch).
- ~~**`SELECT` mit Mehrzeichen-Strings**~~ (Anhang C, Punkt 8), am Original (probe4 T1–T9):
  - Die Zeichen werden **Big-Endian** gepackt: `CASE "ab"` = 97·256 + 98 = 24930, `CASE "abcd"` =
    1633837924. Die Formel im Handbuch (ASCII1 + 255 · ASCII2) ist falsch, auch die
    Little-Endian-Deutung in der Referenz.
  - `CONT` läuft nur in den **nächsten** Block weiter (`CASE 2 … CONT` führt Block 2 und 3 aus, nicht
    4).
  - `CASE TO 3`, `CASE 5 TO 8,20`, `CASE 9 TO`, `CASE "a" TO "z"` funktionieren wie beschrieben.
  - `SELECT 2.7` trifft `CASE 2`: Der Wert wird zur Ganzzahl abgeschnitten.
- ~~**Operatoren**~~ (probe4 T35–T40, probe5 T1–T14). Rangfolge am Original, von stark nach schwach;
  innerhalb einer Stufe wird von links nach rechts ausgewertet:

  | Stufe | Operatoren | Beleg |
  |---|---|---|
  | 1 | `^` (linksassoziativ) | `2^3^2` = 64 |
  | 2 | Vorzeichen `-` | `-2^2` = −4 |
  | 3 | `*` `/` | `7 MOD 3*2` = 1, `8/2 MOD 3` = 1 |
  | 4 | `MOD` `DIV` `\` | `7 MOD 3+1` = 2, `9 DIV 4 MOD 2` = 0 |
  | 5 | `+` `-` | |
  | 6 | Vergleiche `=` `<>` `<` `>` `<=` `>=` `==` | `1+1=2` = −1, `7 MOD 3=1` = −1 |
  | 7 | `AND` `OR` `XOR` `IMP` `EQV` (**gleichrangig**) | `1 OR 2 AND 0` = 0, `0 AND 0 XOR 1` = 1 |
  | 8 | `NOT` (gilt für den ganzen Rest des Ausdrucks) | `NOT 0+1` = −2, `NOT 1 AND 1` = −2 |

  Dass `AND` nicht stärker bindet als `OR` und `NOT` am schwächsten, weicht von den meisten BASICs
  ab. Die Stufe von `IMP`/`EQV` ist aus Analogie angenommen, nicht geprüft.
  - Wahr ist −1. Logische Operatoren arbeiten bitweise auf 32 Bit, Floats werden vorher Richtung 0
    abgeschnitten (`NOT 1` = −2, `3.7 AND 6` = 2, `-1.5 OR 0` = −1). `IMP` und `EQV` bitweise
    (`6 IMP 3` = −5, `6 EQV 3` = −6).
- **Strings** (probe4 T41–T50):
  - Vergleiche nach ASCII (`"a"<"B"` falsch), ein Präfix ist kleiner (`"ab"<"abc"`).
  - `MID$("abc",5)` = `""`, `MID$("abc",0,2)` = `"ab"` (0 zählt wie 1), `LEFT$` mit zu großer Länge
    liefert den ganzen String. `LEFT$("abc",-1)` meldet Fehler 10.
  - Der Befehl `MID$` arbeitet wie im Handbuch (`Testwert`), `MID$(b$,2)="XYZ"` mit `b$="abc"` ergibt
    `aXY`.
  - `VAL?`: 4 für `"3.00 DM"`, 0 für `"x"`, 3 für `" 12"` (führende Leerzeichen zählen mit), 2 für
    `"1E"`; `VAL("1E")` = 1.
  - `STRING$(3,"ab")` = `ababab`, `ASC("")` = 0, `CHR$(0)` ist ein normales Zeichen.
  - `INSTR`: `INSTR("","")` = 1, `INSTR("abc","")` = 0, `INSTR("abcabc","c",4)` =
    `INSTR(4,"abcabc","c")` = 6. Startposition 0 zählt wie 1, eine negative meldet Fehler 10.
  - Eigenheit: Mit leerem Suchstring liefert `INSTR("abc","",n)` im Original für n = 1…5 die Werte 0,
    1, 2, 4, 0 (probe5 T15). Wir liefern n für 1 ≤ n ≤ `LEN(a$)`+1, sonst 0
    ([D12](decisions.md#d12)).
- **Ausgabe auf dem Amiga:** Ein GFA-Programm gibt in ein Fenster aus (öffnet es bei Bedarf). Für
  den ersten Stand schreibt das kompilierte Programm auf `Output()` der Shell; Fensterausgabe folgt mit
  der Gruppe `gui`.
