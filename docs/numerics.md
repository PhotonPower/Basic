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
| ±Unendlich, NaN | gibt es nicht: Überlauf löst Fehler 1 „overflow“ aus, wie im Original (probe3 T16, T18, T19). Die Grenze liegt bei binary64 anders: Im Original ist `1E300*1E300` noch kein Überlauf (probe2 T15) |
| `VAL` außerhalb des Bereichs | ergibt 0 ohne Fehler, wie im Original (`VAL("1E10000")`, probe3 T20) |
| Unterlauf | Ergebnis 0 ohne Fehler (wie Original, probe1 T49): Ist das mit unbegrenztem Exponenten auf 53 Bit gerundete Ergebnis kleiner als 2⁻¹⁰²², wird es 0 ([D13](decisions.md#d13)) |
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
- 0 ist `00 00 00 00 00 00 00 00`; −0 gibt es nicht (`-1*0`, probe2 T18).
- Der Exponent hat 16 Bit, der Wertebereich reicht also bis etwa 10^±9500: `1E300*1E300` ergibt
  ohne Fehler `1.0E+600` (Bytes `8E B3 97 14 29 7A 0B C8`), `EXP(1000)` ergibt `1.970071114025E+434`
  (probe2 T15–T17). Der nutzbare Bereich ist aber kleiner als der Exponent erlaubt: `1E600*1E600`
  meldet Fehler 1 „overflow“, und `1E-300*1E-300` ergibt bereits 0 (probe3 T16, T17).

| Wert | Bytes |
|---|---|
| 1 | `80 00 00 00 00 00 03 FF` |
| −2,5 | `A0 00 00 00 00 00 FC 00` (`$FC00` = −`$0400`) |
| 0,1 | `CC CC CC CC CC CD 03 FB` |

`MKS$` scheint im Original fehlerhaft zu sein: 1, 2 und 0,1 ergeben `FF FF FF 7F`, −1 ergibt
`FF FF FF FF`, 0 ergibt `00 00 00 00`, und `CVS(MKS$(1.5))` ist 9.223371487099E+18 (etwa 2⁶³)
(probe1 T46, T47, probe2 T40–T44). Das sieht nach einer sättigenden Umwandlung aus, nicht nach einem
Fließkommaformat. Wir bilden das nicht nach, `MKS$`/`CVS` liefern IEEE single ([D11](decisions.md#d11)).
`CVD(MKD$(0.1))` ergibt wieder 0,1 (probe2 T45).

**`PI`** ist im Original ungenau: Bytes `C9 0F DA A2 21 DB 04 00`, also 3,14159265359141670…
statt 3,14159265358979… (Abweichung 1,6·10⁻¹², 114 Einheiten der letzten Stelle; probe2 T19, T21).
Daher die Ausgabe `3.141592653591` (probe1 T34). Das Dezimalliteral `3.14159265359` wird dagegen
korrekt gerundet eingelesen (`C9 0F DA A2 21 77`, probe2 T20). Transzendente Funktionen sind im
Original nicht genau: `EXP(1000)` hat einen relativen Fehler von etwa 4·10⁻¹² (probe2 T16). Beides
bilden wir nicht nach: `PI` ist der nächstgelegene binary64-Wert ([D11](decisions.md#d11)).

> **Bewusste Abweichung vom Original** ([D11](decisions.md#d11)): GFA-BASIC 3.5 rechnet mit 48 Bit
> Mantisse, wir mit den 53 Bit von binary64. Das ist im Ergebnis sichtbar: `FOR x=0 TO 1 STEP 0.1`
> läuft im Original 10-mal (probe1 T21), mit binary64 11-mal (0,1 zehnmal addiert ergibt
> 1,000000000000007 bzw. 0,9999999999999999).

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
  Das Argument muss im Ganzzahlbereich liegen, sonst Fehler 2 wie im Original (probe13 T21).
- `DEG`/`RAD` sind `x*180/PI` bzw. `x*PI/180` in genau dieser Reihenfolge.

Umsetzung (`runtime/ref/rt_math.c`, 68000-Port `runtime/m68k/rt_math.s`, erzeugt von
`tools/gen/gen_rt_math.py`, im Konformanztest bitgleich):

| Funktion | Verfahren | Abstand zur libm (gemessen) |
|---|---|---|
| `EXP` | x = k·ln 2 + r (ln 2 in zwei Teilen), Taylor bis r¹³/13!, Skalieren mit 2ᵏ | ≤ 1 ulp |
| `LOG`, `LOG10` | x = 2ᵉ·m, √½ < m ≤ √2; log m = f − s(f − R) mit s = f/(2+f), Reihe in s² bis 2/23 | ≤ 1 bzw. 2 ulp |
| `SIN`, `COS`, `TAN` | Reduktion x = n·π/2 + r mit π/2 in vier Teilen (drei zu 26 Bit), Taylor bis r¹⁷/17! bzw. r¹⁶/16! | ≤ 3 ulp |
| `ATN` | 1/x über 1, π/6-Verschiebung über tan(π/12), Reihe bis z¹⁴/29 | ≤ 3 ulp |
| `ASIN`, `ACOS` | atan(x/√((1−x)(1+x))); `ACOS` über 0,5 als Halbwinkel 2·asin(√((1−x)/2)) | ≤ 3 ulp |
| `^` (Exponent keine Ganzzahl) | EXP(y·LOG(x)) | |

Die Reduktion nimmt je Durchgang die oberen 26 Bit von n, so dass alle Produkte n·Teil exakt sind.
Bis |x| ≈ 10⁸ genügt ein Durchgang; darüber folgen weitere, das Ergebnis bleibt trotzdem sehr genau
(`SIN(1E10)` auf etwa 14 Stellen, `SIN(1E20)` = −0,6452512 statt −0,6452513). Das Original ist bei
großen Argumenten ungenau (`SIN(1E10)` = −0,491963 statt −0,487506, `SIN(1E20)` = 1; probe13 T12,
T13) und hat ein ungenaues `PI` (`TAN(PI/2)` = 70368744177660); beides bilden wir nicht nach (D11).

Ränder wie im Original (probe13): `EXP` unter etwa −745 ist 0 ohne Fehler, ein Überlauf Fehler 1;
`LOG`/`LOG10` von x ≤ 0 und `(-8)^(1/3)` Fehler 6; `ASIN`/`ACOS` außerhalb von [−1, 1] Fehler 67
„asin/acos error“; `0^-1` Fehler 0, `0^0` = 1, `0^0.5` = 0. Das Testprogramm `tests/programs/math.lst`
gibt Werte auf 8 Stellen gerundet aus und stimmt so mit dem Original überein.

## Ganzzahlen

| Thema | Festlegung (✔ = am Original bestätigt) |
|---|---|
| Darstellung | Zweierkomplement; `%` 32 Bit, `&` 16 Bit, `\|` 8 Bit ohne Vorzeichen, `!` 0/−1 |
| `INC`, `DEC`, `ADD x,y`, `SUB`, `MUL` (Befehle), `ADD()` … (Funktionen) | ohne Überlaufprüfung, Umbruch modulo 2ⁿ der Zielbreite (Referenz: `INC a\|` von 255 → 0). ✔ für `INC a%` (2147483647 → −2147483648), `INC w&` (32767 → −32768), `INC c\|` (255 → 0), `DEC c\|` (0 → 255), `DEC a%` (−2147483648 → 2147483647), probe1 T3, T5, probe2 T5, T7, T8 |
| `+ - *` in Ausdrücken | ✔ ohne Überlauf, wie in Float: `x=a%+1` ergibt 2147483648, `x=a%*2` 4294967294, `b%=a%+1-1` und `b%=(a%*2) DIV 2` ergeben 2147483647 ohne Fehler (probe2 T1–T4) |
| Zuweisung an Ganzzahlvariable | ✔ mit Bereichsprüfung: `b%=a%+1`, `b%=3E9` → Fehler 2 „no integer\|-2147483648 .. 2147483647“; `w&=w&+1` → Fehler 4 „no word\|-32768 .. 32767“; `c\|=c\|+1` bei 255 → Fehler 3 „no byte\|0 .. 255“. Die Zielvariable bleibt unverändert (probe1 T1, T2, T4, T6, probe2 T6) |
| Float → Ganzzahl (Zuweisung) | ✔ Abschneiden Richtung 0: 2,5 → 2, 3,5 → 3, −2,5 → −2, 2,7 → 2, −2,7 → −2, `7/2` → 3 (probe1 T7–T12) |
| `DIV`, `MOD`, `\` | ✔ Abschneiden Richtung 0, Rest mit dem Vorzeichen des Dividenden: `-7 DIV 2` = −3, `-7 MOD 2` = −1, `7 MOD -2` = 1, `-7\2` = −3, `7\-2` = −3 (probe1 T13, probe2 T9) |
| Division durch 0 | ✔ Fehler 0 „divide by zero“, auch bei `DIV` (probe1 T14) |
| Schiebebefehle | wie in der Referenz (`SHL&`, `SHR\|` mit Breite, Bitnummer `AND 31`) |
| Float → Ganzzahl außerhalb des Bereichs | ✔ Fehler 2 bzw. 4 (probe1 T6) |

`INT` rundet ab (floor, ✔ `INT(-0.5)` = −1, probe1 T17), `TRUNC`/`FIX` schneidet Richtung 0 ab,
`FRAC(x) = x − TRUNC(x)`. `CINT` rundet bei genau ,5 **von 0 weg** (✔ 0,5 → 1, 1,5 → 2, 2,5 → 3,
−0,5 → −1, −1,5 → −2, probe1 T16). `ROUND` dagegen rundet **aufwärts**: ✔ `ROUND(x,n)` =
`INT(x·10ⁿ+0,5)/10ⁿ`, also `ROUND(2.5)` = 3, `ROUND(-2.5)` = −2, `ROUND(0.125,2)` = 0,13,
`ROUND(-0.125,2)` = −0,12, `ROUND(1250,-2)` = 1300 (probe1 T17, probe6 T11). Weil auf dem Binärwert
gerechnet wird, ergibt `ROUND(0.35,1)` im Original 0,3 (0,35·10 < 3,5 bei 48 Bit), bei uns 0,4
(0,35·10 = 3,5 in binary64, [D11](decisions.md#d11)).

## Ungefähre Gleichheit `==`

Die Referenz sagt: `==` vergleicht etwa 28 Bit der Mantisse. Festlegung, am Original bestätigt:
Beide Operanden werden auf **28 signifikante Bit** gerundet, bei genau der Hälfte **vom Betrag her
aufwärts** (von 0 weg), und dann exakt verglichen. Ein Übertrag beim Runden erhöht den Exponenten
(`1-2^-29` wird zu 1). 0 ist nur gleich 0.

Diese Regel erklärt alle 17 geprüften Fälle aus probe2 T34–T39 und probe3 T5–T15. „Round to nearest
even“, die frühere Festlegung, widerspricht dagegen 6 davon, z. B. `1==1+2^-28` (Original: falsch).
Die Bytes in probe3 T1–T4 zeigen, dass `2^(-28)` exakt ist.

| Vergleich | Original | Begründung (28 Bit, Hälfte aufwärts) |
|---|---|---|
| `1==1+2^-29` | wahr | 2^-29 liegt unter der halben Stelle |
| `1==1+2^-28` | falsch | genau halbe Stelle → aufgerundet |
| `1+2^-28==1+2*2^-28` | wahr | beide runden auf 1+2^-27 |
| `1+2^-29==1+2^-28` | falsch | ab- bzw. aufgerundet |
| `1==1-2^-29` | wahr | rundet mit Übertrag auf 1 |
| `1==1-2^-28` | falsch | liegt eine volle Stelle darunter |
| `0==2^-116` | falsch | 0 ist nur gleich 0 |

(probe1 T41, probe2 T34–T39, probe3 T5–T15)

## Zahl ↔ Text

`PRINT`, `STR$`, `PRINT USING`, `VAL`, `INPUT`, `READ` sind Teil der Numerik, weil die
Differenztests Ausgaben vergleichen.

- Ausgabe mit höchstens **13 signifikanten Stellen** wie GFA-BASIC, deterministisch aus dem
  Bitmuster berechnet (exakte Ganzzahlarithmetik auf der Mantisse, keine Fließkomma-Zwischenschritte).
  Gerundet wird der exakte Binärwert, bei einem exakten Gleichstand von 0 weg (✔ `12345678901235` →
  `12345678901240`, `1234567890123.5` → `1234567890124`, probe6 T6, T7).
- `DEFNUM n` (Anzahl signifikanter Stellen) und `MODE` (Dezimal- und Tausendertrennzeichen) gehören
  zur Formatierung und werden in `rt_fmt_flt` bzw. `PRINT USING` berücksichtigt. ✔ `DEFNUM 1` gibt
  2 Stellen aus (`2.5`, `0.25`), die Schwellen der Exponentdarstellung hängen nicht von `DEFNUM` ab
  (`DEFNUM 3`: `12300`, `10000000000000`, `1.0E+14`, `0.0000123`, `1.23E-06`; probe6 T8–T10).
- Formatregeln am Original (probe1 T22–T40), `PRINT` und `STR$` geben gleich aus:
  - **kein** Leerzeichen vor positiven Zahlen (`[1][-1][-12]`), führende Null (`0.5`, `-0.5`);
  - 13 signifikante Stellen, nachfolgende Nullen entfallen (`0.3333333333333`, `0.6666666666667`,
    `33.33333333333`, `0.3` für `0.1+0.2`);
  - ohne Exponent, solange der auf 13 Stellen gerundete Betrag unter 10¹⁴ und mindestens 10⁻⁵ ist:
    `10000000000000` (10¹³), `15000000000000`, `0.00001`, `0.0001`, `-0.000123`; mit Exponent
    `1.0E+14`, `1.5E+14`, `1.0E-06`, `1.25E-08`, und `99999999999999` wird zu `1.0E+14` (probe1
    T28–T33, probe2 T25–T32). Im Exponentformat steht mindestens eine Nachkommastelle, der Exponent
    hat Vorzeichen und mindestens zwei Ziffern (`1.0E-06`, `1.0E+600`);
  - `PI` gibt `3.141592653591` aus. Das liegt an der ungenauen Konstante (siehe oben), nicht an der
    Ausgabe (probe2 T19–T22);
  - `DEFNUM 4` rundet auf 4 Stellen, ohne die Exponentschwelle zu ändern: `3.142`, `123500`,
    `0.0001235`; `STR$` ebenso (probe2 T33);
  - Komma in `PRINT` springt auf die nächste Spalte, die ein Vielfaches von 16 ist (probe1 T39).
- Werden die Formatregeln bestätigt, kommen sie als Golden-Tests nach `tests/programs`.

### `PRINT USING`, `STR$(x,y[,z])` und `MODE` (am Original, probe4)

- **Rundung über die Dezimaldarstellung:** Gerundet wird die auf 13 Stellen gerundete Dezimalzahl,
  bei genau ,5 von 0 weg: `USING "#"` ergibt für 0,5/1,5/2,5 `1`/`2`/`3`, `"#.#"` für 0,25 `0.3` und
  für 0,35 `0.4`, obwohl 0,35 binär knapp darunter liegt (T14–T18). `STR$(x,y,z)` rundet ebenso
  (`STR$(-1.5,4,0)` = `"  -2"`, T34). Das setzen wir auf binary64 genauso um.
- Beispiele aus dem Handbuch bestätigt: `"#.####"`,PI → `3.1416`; `"###.##^^^^"` → ` 16.00E+00`,
  `  4.10E+03`; `"PI_._._. #.###"` → `PI... 3.142`; `&`, `!`, `\  \` (T10–T12, T30).
- Vorzeichen: `"-##.#"`, −1,25 → `- 1.3` (Minus auf fester Stelle); positiv → Leerzeichen;
  `"+##"`, 5 → `+ 5`; `"##.#-"`, −3 → ` 3.0-`; ohne eigene Stelle steht das Minus direkt vor der
  Zahl, wenn Platz ist (`"###.##"`, −0,5 → ` -0.50`). `*` füllt (`**12`), `$` steht direkt vor der
  ersten Ziffer (` $3.50`), `,` trennt Tausender (`1,234.50`) (T21–T28).
- Zu wenig Stellen: Die führenden Ziffern **fallen weg** (`"##"`, 1234 → `34`), ein Minus ohne Platz
  ebenso (`"#"`, −2,5 → `3`) (T19, T20).
- **`MODE` 2 und 3** vertauschen auch die Bedeutung von `.` und `,` **im Formatstring**: Dort muss es
  `"#.###,##"` heißen; `"#,###.##"` wird zu `4,500.##`. `STR$(1234.5,8,2)` ergibt ` 1234,50`. `PRINT`
  ist von `MODE` nicht betroffen. `MODE` 0 und 1 unterscheiden sich nur im Datum (T32).
- `STR$(x,n)`: links mit Leerzeichen auf n aufgefüllt; ist `STR$(x)` länger, bleiben die **linken**
  n Zeichen (`STR$(123456,3)` = `"123"`, `STR$(-12,2)` = `"-1"`, `STR$(1.5,2)` = `"1."`). Das
  Handbuch (`RIGHT$(…)`) irrt hier (probe4 T33, probe5 T19).
- `STR$(x,y,z)` formatiert wie `PRINT USING` mit y Stellen, davon z Nachkommastellen; zu lange
  Ergebnisse verlieren also die **führenden** Ziffern und das Minus (`STR$(12345.6,4,1)` = `"45.6"`,
  `STR$(12345.6,3,0)` = `"346"`, `STR$(-1.25,3,1)` = `"1.3"`, `STR$(1,0)` = `""`). Ebenso
  `USING "##"`: −5 → `-5`, −15 → `15`; `"#.#"`, 99,96 → `0.0` (probe5 T20, T25–T27).
- Weitere Regeln am Original (probe7, probe8), Einzelheiten in [D14](decisions.md#d14):
  - Nach dem letzten Wert folgt der Rest des Formats als Text, auch Felder.
  - Ein Wert ohne passendes Feld erscheint als `?` und `PRINT`-Form (`Summe:?5`).
  - Negative Werte behalten das Minus auch bei 0 (`-0.00`); im Exponentformat nur mit eigener
    Vorzeichenstelle.
  - Der Exponent hat genau (Zahl der `^`) − 2 Ziffern und ist ein Vielfaches der Stellen vor dem
    Punkt (`###.##^^^^`: 12345 → ` 12.35E+03`; `##.##^^^^`: 0,5 → `50.00E-02`).
- Umsetzung: `rt_using_*` und `rt_str_fmt`/`rt_str_n` in `runtime/ref`.
- **Eigenheiten des Originals, nicht nachgebildet** ([D12](decisions.md#d12)):
  - Formate ohne Stelle vor dem Punkt sind im Original fehlerhaft (`".##"`, 0,25 → `. 0`, 0,5 →
    `. 1`; probe4 T29, probe5 T21–T24). Wir formatieren korrekt (`.25`, `.50`).
  - Mehrere Werte mit einem Format: Im Original wird nur der erste formatiert, der zweite erscheint
    als `?2.75` (probe4 T31, probe5 T30, T31). Wir wenden das Format auf jeden Wert an.
- `VAL` liest Dezimal, `&H`/`$`, `&X`/`%`, `&O` (✔ `VAL("1.5E3")` = 1500, `VAL("&HFF")` = 255,
  `VAL("%101")` = 5, `VAL(".5x")` = 0,5 – Lesen endet am ersten ungültigen Zeichen, probe1 T54) und ist korrekt gerundet.
  Am Original (probe4 T47, probe6 T1–T5): Mit Präfix wird eine 32-Bit-Zahl gelesen, die überläuft
  und als vorzeichenbehaftet gilt (`&HFFFFFFFF` = −1, `&H100000000` = 0); vor dem Präfix ist kein
  Vorzeichen erlaubt (`VAL("-&H10")` = 0); ein Präfix ohne Ziffern ergibt 0 mit `VAL?` = 0. Ein `E`
  wird auch ohne Ziffern verbraucht (`VAL?("1E")` = 2, `VAL?("1E+x")` = 2). Außerhalb des Bereichs
  ergibt `VAL` 0 ohne Fehler. Insgesamt (Text → nächstgelegene
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
