# Numerik

Alle Zielsysteme rechnen **bitgleich**. Das gilt für Ganzzahlen, Fließkomma, mathematische
Funktionen, die Umwandlung zwischen Zahl und Text und den Zufallsgenerator. Deshalb schreiben wir
die gesamte Numerik selbst: Weder die CPC-Firmware (5-Byte-Format, rund 9 Stellen) noch die
Amiga-Mathe-Bibliotheken noch die `libm` des PC-Compilers garantieren gleiche Ergebnisse.

Die Semantik ist durch die C-Referenz in `runtime/ref` definiert ([runtime.md](runtime.md)). Dieses
Dokument beschreibt die Festlegungen, die sie umsetzt.

## Fließkommaformat

Speicherformat: **IEEE 754 binary64** (1 Bit Vorzeichen, 11 Bit Exponent mit Bias 1023, 52 Bit
Mantisse plus verstecktes Bit), Big-Endian im BASIC-Speicher (wie auf dem Amiga), knapp 16
Dezimalstellen Genauigkeit.

Bewusst vereinfachte Semantik, damit die Z80-Implementierung klein bleibt:

| Fall | Festlegung |
|---|---|
| Rundung | nur „round to nearest, ties to even“ |
| denormalisierte Zahlen | gibt es nicht: Ergebnisse unterhalb von 2⁻¹⁰²² werden zu 0 |
| ±Unendlich, NaN | gibt es nicht: Überlauf löst einen GFA-Laufzeitfehler aus (Original: **kein** Fehler bei `1E300*1E300` und `EXP(1000)`, probe1 T48, T50; Ergebnis noch unbekannt) |
| Unterlauf | Ergebnis 0 ohne Fehler (wie Original, probe1 T49) |
| Division durch 0 | GFA-Laufzeitfehler (Original: Fehler 0 „divide by zero“, probe1 T15) |
| −0 | gibt es nicht: Ergebnisse 0 haben immer positives Vorzeichen |
| `SQR`, `LOG` außerhalb des Definitionsbereichs | GFA-Laufzeitfehler: 5 bei `SQR` negativ, 6 bei `LOG(0)` (probe1 T51, T52) |

Wird eine Zahl aus dem Speicher gelesen (`DOUBLE{}`, `CVD`, Datei), die denormalisiert, unendlich oder
NaN ist, wird sie beim Laden normalisiert (Denormale → 0) bzw. löst einen Fehler aus.

`SINGLE{}` und `MKS$`/`CVS` (4-Byte-Format) sowie das GFA-eigene 8-Byte-Format von `FLOAT{}` und
`MKD$`/`CVD` werden nur an diesen Schnittstellen konvertiert.

**GFA-eigenes 8-Byte-Format** (am Original ermittelt, probe1 T42–T45): 6 Byte Mantisse, dann 2 Byte
Exponent, beide Big-Endian.

- Mantisse: 48 Bit **mit** führender 1 (kein verstecktes Bit), normalisiert auf 0,5 ≤ m < 1.
- Exponent: Bias `$3FE`, Wert = m · 2^(e − `$3FE`).
- Vorzeichen: Bei negativen Zahlen ist das Exponentwort negiert (Zweierkomplement), die Mantisse
  bleibt positiv.
- 0 ist `00 00 00 00 00 00 00 00`.

| Wert | Bytes |
|---|---|
| 1 | `80 00 00 00 00 00 03 FF` |
| −2,5 | `A0 00 00 00 00 00 FC 00` (`$FC00` = −`$0400`) |
| 0,1 | `CC CC CC CC CC CD 03 FB` |

`MKS$` lieferte für 1 und 0,1 gleichermaßen `FF FF FF 7F` (probe1 T46, T47); das 4-Byte-Format ist
damit noch nicht verstanden (probe2).

> **Abweichung vom Original:** GFA-BASIC 3.5 rechnet mit 48 Bit Mantisse, nicht mit den 53 Bit von
> binary64. Das ist im Ergebnis sichtbar: `FOR x=0 TO 1 STEP 0.1` läuft im Original 10-mal (probe1
> T21), mit binary64 11-mal (0,1 zehnmal addiert ergibt 1,000000000000007 bzw. 0,9999999999999999).
> Ob die Festlegung [D4](decisions.md#d4) bleibt, ist offen ([roadmap.md](roadmap.md#offene-fragen)).

## Grundoperationen

`fp_add`, `fp_sub`, `fp_mul`, `fp_div`, `fp_sqrt`, `fp_cmp` und die Konvertierungen sind **korrekt
gerundet** im obigen Sinne. Damit ist das Ergebnis jeder Grundoperation eindeutig, und jede
Implementierung (C, 68000, Z80, später ggf. FPGA-Hardware) lässt sich bitgenau gegen die Referenz
testen.

## Transzendente Funktionen

`SIN COS TAN ASIN ACOS ATN EXP LOG LOG10 ^` müssen nicht korrekt gerundet sein, aber
**deterministisch**: überall derselbe Algorithmus mit denselben Konstanten in derselben Reihenfolge
von Grundoperationen.

- Konstanten (Polynomkoeffizienten, π/2 in zwei Teilen für die Argumentreduktion, ln 2 …) stehen als
  exakte Hex-Bitmuster in `runtime/ref`, nie als Dezimalliteral.
- Die Funktionen sind nur aus Grundoperationen zusammengesetzt. Sind die Grundoperationen bitgenau,
  sind es die Funktionen damit automatisch, auch wenn ein Port sie in Assembler nachschreibt.
- `SINQ`/`COSQ` werden exakt als `SIN(RAD(g))` bzw. `COS(RAD(g))` gerechnet (laut Referenz erlaubt).
- `DEG`/`RAD` sind `x*180/PI` bzw. `x*PI/180` in genau dieser Reihenfolge.

## Ganzzahlen

| Thema | Festlegung (✔ = am Original bestätigt) |
|---|---|
| Darstellung | Zweierkomplement; `%` 32 Bit, `&` 16 Bit, `\|` 8 Bit ohne Vorzeichen, `!` 0/−1 |
| `INC`, `DEC`, `ADD x,y`, `SUB`, `MUL` (Befehle), `ADD()` … (Funktionen) | ohne Überlaufprüfung, Umbruch modulo 2ⁿ der Zielbreite (Referenz: `INC a\|` von 255 → 0). ✔ für `INC a%` (2147483647 → −2147483648) und `INC w&` (32767 → −32768), probe1 T3, T5 |
| `+ - *` in Ausdrücken, Zuweisung an Ganzzahlvariable | ✔ mit Überlaufprüfung: `b%=a%+1`, `b%=a%*2`, `b%=3E9` → Fehler 2 „no integer\|-2147483648 .. 2147483647“; `w&=w&+1` → Fehler 4 „no word\|-32768 .. 32767“. Die Zielvariable bleibt unverändert (probe1 T1, T2, T4, T6). Ob im Ausdruck in Integer oder Float gerechnet und erst bei der Zuweisung geprüft wird, klärt probe2 |
| Float → Ganzzahl (Zuweisung) | ✔ Abschneiden Richtung 0: 2,5 → 2, 3,5 → 3, −2,5 → −2, 2,7 → 2, −2,7 → −2, `7/2` → 3 (probe1 T7–T12) |
| `DIV`, `MOD`, `\` | ✔ Abschneiden Richtung 0, Rest mit dem Vorzeichen des Dividenden: `-7 DIV 2` = −3, `-7 MOD 2` = −1, `7 MOD -2` = 1 (probe1 T13) |
| Division durch 0 | ✔ Fehler 0 „divide by zero“, auch bei `DIV` (probe1 T14) |
| Schiebebefehle | wie in der Referenz (`SHL&`, `SHR\|` mit Breite, Bitnummer `AND 31`) |
| Float → Ganzzahl außerhalb des Bereichs | ✔ Fehler 2 bzw. 4 (probe1 T6) |

`INT` rundet ab (floor, ✔ `INT(-0.5)` = −1, probe1 T17), `TRUNC`/`FIX` schneidet Richtung 0 ab,
`FRAC(x) = x − TRUNC(x)`. `CINT` rundet bei genau ,5 **von 0 weg** (✔ 0,5 → 1, 1,5 → 2, 2,5 → 3,
−0,5 → −1, −1,5 → −2, probe1 T16). `ROUND` ebenso: `ROUND(2.5)` = 3, `ROUND(0.125,2)` = 0,13
(probe1 T17; 0,125 ist exakt darstellbar).

## Ungefähre Gleichheit `==`

Die Referenz sagt: `==` vergleicht etwa 28 Bit der Mantisse. Festlegung: Beide Operanden werden mit
„round to nearest even“ auf 28 Bit signifikante Mantisse gerundet und dann exakt verglichen. Das ist
präzise definiert und auf allen Zielen gleich; ob es in Grenzfällen genau dem Original entspricht,
ist zu prüfen.

Am Original (probe1 T41): `1==1+2^-29` ist wahr (−1), `1==1+2^-28` ist falsch (0), ebenso für
2^-27 und 2^-26. Bei 1 (Mantisse `$800000000000`) liegt 2^-29 auf Bit 18, 2^-28 auf Bit 19 der
48-Bit-Mantisse; Unterschiede in den unteren 19 Bit werden also ignoriert. Die 28-Bit-Rundung oben
ergibt für 2^-28 dagegen „gleich“ (genau halbe Stelle, gerade Rundung) und muss angepasst werden;
die genaue Regel (Abschneiden oder Differenz, absolut oder relativ) klärt probe2.

## Zahl ↔ Text

`PRINT`, `STR$`, `PRINT USING`, `VAL`, `INPUT`, `READ` sind Teil der Numerik, weil die
Differenztests Ausgaben vergleichen.

- Ausgabe mit höchstens **13 signifikanten Stellen** wie GFA-BASIC, deterministisch aus dem
  Bitmuster berechnet (exakte Ganzzahlarithmetik auf der Mantisse, keine Fließkomma-Zwischenschritte).
- `DEFNUM n` (Anzahl signifikanter Stellen) und `MODE` (Dezimal- und Tausendertrennzeichen) gehören
  zur Formatierung und werden in `rt_fmt_flt` bzw. `PRINT USING` berücksichtigt.
- Formatregeln am Original (probe1 T22–T40), `PRINT` und `STR$` geben gleich aus:
  - **kein** Leerzeichen vor positiven Zahlen (`[1][-1][-12]`), führende Null (`0.5`, `-0.5`);
  - 13 signifikante Stellen, nachfolgende Nullen entfallen (`0.3333333333333`, `0.6666666666667`,
    `33.33333333333`, `0.3` für `0.1+0.2`);
  - ganze Zahlen bis mindestens 13 Stellen ohne Exponent (`1234567890123`, `2147483648`), `1E15` als
    `1.0E+15`; Exponentdarstellung mit mindestens einer Nachkommastelle und Vorzeichen im Exponenten
    (`1.0E+100`, `-1.0E-100`); `1E-5` als `0.00001`. Die genauen Schwellen klärt probe2;
  - `PI` gibt `3.141592653591` aus. Korrekt auf 13 Stellen gerundet wäre `3.14159265359` (sowohl aus
    binary64 als auch aus dem auf 48 Bit gerundeten Wert). Die Umwandlung im Original ist also nicht
    korrekt gerundet oder die Konstante weicht ab (probe2);
  - Komma in `PRINT` springt auf die nächste Spalte, die ein Vielfaches von 16 ist (probe1 T39).
- Werden die Formatregeln bestätigt, kommen sie als Golden-Tests nach `tests/programs`.
- `VAL` liest Dezimal, `&H`/`$`, `&X`/`%`, `&O` (✔ `VAL("1.5E3")` = 1500, `VAL("&HFF")` = 255,
  `VAL("%101")` = 5, `VAL(".5x")` = 0,5 – Lesen endet am ersten ungültigen Zeichen, probe1 T54) und ist korrekt gerundet (Text → nächstgelegene
  darstellbare Zahl), damit `VAL(STR$(x))` stabil ist.

## Zufallszahlen

Eigener Generator, auf allen Zielen gleich: **xorshift32** (Marsaglia, Schiebewerte 13, 17, 5).

- `RANDOMIZE y` setzt den Zustand aus `y` (0 wird durch eine feste Konstante ersetzt, da xorshift den
  Zustand 0 nicht verlassen kann); `RANDOMIZE` ohne Argument bzw. Programmstart nehmen einen
  Plattform-Zeitwert. Für Tests kann der Startwert per Compileroption fest gesetzt werden.
- `RND` = nächster Zustand / 2³² (exakt darstellbar, 0 ≤ r < 1).
- `RANDOM(x)` = `INT(RND * x)`, `RAND(y)` = (Zustand >> 16) × (y AND &HFFFF) >> 16.

Die Folgen entsprechen nicht denen des Originals; Programme, die eine bestimmte Folge des Originals
erwarten, gibt es vermutlich kaum.

## Konstantenfaltung

Der Compiler faltet konstante Ausdrücke und wandelt Float-Literale mit `runtime/ref` um. Ein
Programm mit `x=1.5` braucht so zur Laufzeit keine Text-zu-Zahl-Umwandlung, und gefaltete Werte sind
bitgleich zu denen, die das Zielsystem zur Laufzeit berechnen würde.

## Leistung

Auf dem Z80 bei 4 MHz ist binary64 in Software langsam (grob geschätzt einige hundert Mikrosekunden
bis Millisekunden je Grundoperation). Der Z80-Emulator zählt T-States; die Harness gibt sie aus, sodass
Optimierungen der Fließkommaroutinen messbar sind ([testing.md](testing.md)). Ein späteres schnelleres
Format für den CPC wäre eine eigene Compileroption mit eigener Referenz, nicht eine Abweichung von
dieser.
