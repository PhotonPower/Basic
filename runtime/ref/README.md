# runtime/ref

C-Referenzimplementierung der Runtime in portablem C99, nur Ganzzahlarithmetik (kein `float`/`double`,
kein `libm`). Sie **definiert die Semantik**: Das PC-Ziel benutzt sie direkt, der Compiler faltet
Konstanten damit, und die Assembler-Ports werden bitgenau gegen sie getestet.
Siehe [docs/numerics.md](../../docs/numerics.md) und [docs/runtime.md](../../docs/runtime.md).
Modulkopf in C: `//! module …`, `//! provides …` usw.

Schnittstelle: [`rt_ref.h`](rt_ref.h). Fehler meldet die Referenz über `rt_error(n)`, das die
Umgebung bereitstellt (erzeugtes Programm, Konstantenfaltung, Tests).

| Modul | Inhalt |
|---|---|
| `fp_round_pack` | Runden und Zusammensetzen (round to nearest even), Überlauf → Fehler 1, Unterlauf → 0 |
| `fp_add`, `fp_mul`, `fp_div`, `fp_sqrt` | Grundoperationen, korrekt gerundet |
| `fp_cmp`, `fp_approx_eq` | exakter Vergleich, `==` (28 Bit, Hälfte von 0 weg) |
| `fp_canon` | Wert aus dem Speicher normalisieren |
| `fp_from_i32`, `fp_to_int` | Ganzzahl → Float; Zuweisung an `%`/`&`/`\|` (abschneiden, Fehler 2/4/3) |
| `fp_round_int`, `fp_round` | `TRUNC`, `INT`, `FRAC`, `CINT`, `ROUND` |
| `ix_bn` | kleine Big-Integer für die exakten Umwandlungen |
| `rt_fmt_flt`, `rt_val` | Zahl → Text (`PRINT`, `STR$`, `DEFNUM`), Text → Zahl (`VAL`, `VAL?`) |
| `rt_rnd` | Zufallsgenerator xorshift32 |

Tests: `tests/test_runtime_ref.cpp` vergleicht mit der IEEE-Arithmetik des Rechners (Zufallsoperanden
mit festem Startwert), mit `strtod`/`printf` und mit den Ergebnissen der Prüfprogramme
(`tests/probes/results`).

Noch offen für 0.1: `PRINT USING`/`STR$(x,y,z)` und der Generator für die Konformanzfälle der
Assembler-Ports.
