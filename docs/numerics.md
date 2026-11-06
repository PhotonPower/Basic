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
| ±Unendlich, NaN | gibt es nicht: Überlauf löst einen GFA-Laufzeitfehler aus |
| Division durch 0 | GFA-Laufzeitfehler |
| −0 | gibt es nicht: Ergebnisse 0 haben immer positives Vorzeichen |
| `SQR`, `LOG` außerhalb des Definitionsbereichs | GFA-Laufzeitfehler (z. B. #5 bei `SQR` negativ) |

Wird eine Zahl aus dem Speicher gelesen (`DOUBLE{}`, `CVD`, Datei), die denormalisiert, unendlich oder
NaN ist, wird sie beim Laden normalisiert (Denormale → 0) bzw. löst einen Fehler aus.

`SINGLE{}` und `MKS$`/`CVS` (4-Byte-Format) sowie das GFA-eigene 8-Byte-Format von `FLOAT{}` und
`MKD$`/`CVD` werden nur an diesen Schnittstellen konvertiert. Die genaue Bitbelegung des GFA-Formats
ist noch zu klären ([roadmap.md](roadmap.md#offene-fragen)).

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

| Thema | Festlegung (Vorschlag, gegen GFA zu prüfen) |
|---|---|
| Darstellung | Zweierkomplement; `%` 32 Bit, `&` 16 Bit, `\|` 8 Bit ohne Vorzeichen, `!` 0/−1 |
| `INC`, `DEC`, `ADD x,y`, `SUB`, `MUL` (Befehle), `ADD()` … (Funktionen) | ohne Überlaufprüfung, Umbruch modulo 2ⁿ der Zielbreite (Referenz: `INC a\|` von 255 → 0) |
| `+ - *` in Ausdrücken | offen: GFA rechnet gemischte Ausdrücke vermutlich in Float; zu klären |
| `DIV`, `MOD`, `\` | Division mit Abschneiden Richtung 0, Rest mit dem Vorzeichen des Dividenden (wie `DIVS`) |
| Division durch 0 | GFA-Laufzeitfehler |
| Schiebebefehle | wie in der Referenz (`SHL&`, `SHR\|` mit Breite, Bitnummer `AND 31`) |
| Float → Ganzzahl außerhalb des Bereichs | GFA-Laufzeitfehler |

`INT` rundet ab (floor), `TRUNC`/`FIX` schneidet Richtung 0 ab, `FRAC(x) = x − TRUNC(x)`.
`CINT` rundet; die Rundungsart bei genau ,5 ist noch zu klären.

## Ungefähre Gleichheit `==`

Die Referenz sagt: `==` vergleicht etwa 28 Bit der Mantisse. Festlegung: Beide Operanden werden mit
„round to nearest even“ auf 28 Bit signifikante Mantisse gerundet und dann exakt verglichen. Das ist
präzise definiert und auf allen Zielen gleich; ob es in Grenzfällen genau dem Original entspricht,
ist zu prüfen.

## Zahl ↔ Text

`PRINT`, `STR$`, `PRINT USING`, `VAL`, `INPUT`, `READ` sind Teil der Numerik, weil die
Differenztests Ausgaben vergleichen.

- Ausgabe mit höchstens **13 signifikanten Stellen** wie GFA-BASIC, deterministisch aus dem
  Bitmuster berechnet (exakte Ganzzahlarithmetik auf der Mantisse, keine Fließkomma-Zwischenschritte).
- `DEFNUM n` (Anzahl signifikanter Stellen) und `MODE` (Dezimal- und Tausendertrennzeichen) gehören
  zur Formatierung und werden in `rt_fmt_flt` bzw. `PRINT USING` berücksichtigt.
- Formatregeln (wann Exponentdarstellung, Leerzeichen vor positiven Zahlen, `.5` statt `0.5`) werden
  aus dem Handbuch bzw. am Original bestimmt und als Golden-Tests festgehalten.
- `VAL` liest Dezimal, `&H`/`$`, `&X`/`%`, `&O` und ist korrekt gerundet (Text → nächstgelegene
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
