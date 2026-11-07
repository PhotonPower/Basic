# runtime/m68k

Die Runtime als MC68000-Module (Motorola-Syntax, Assembler des Schwesterprojekts). Jede Routine
liefert **bitgenau** dasselbe wie `runtime/ref`; das prüft `tests/test_conformance_m68k.cpp` im
Emulator mit Grenzfällen und Zufallsoperanden (`BASIC_CONFORMANCE_CASES` setzt die Anzahl).

Aufrufkonvention ([docs/runtime.md](../../docs/runtime.md#aufrufkonventionen)): `f64` in D0:D1
(höherwertiger Teil in D0), zweiter Operand in D2:D3, Ergebnis in D0:D1. D0–D3, A0, A1 dürfen
verändert werden, alle anderen Register bleiben erhalten. Fehler meldet eine Routine über
`rt_error` (Nummer in D0); im Programm kehrt `rt_error` nicht zurück.

Für die Grundoperationen genügt es, das **exakte** Ergebnis und ein korrektes Sticky-Bit an
`fp_round_pack` zu übergeben; das gerundete Ergebnis ist dann zwangsläufig gleich dem der Referenz.

| Modul | Inhalt |
|---|---|
| `fp_round_pack` | Normalisieren, Runden (ties to even), Zusammensetzen; `fp_finish` meldet Überlauf |
| `fp_add` | `fp_add`, `fp_sub`, `fp_neg` |
| `fp_cmp` | exakter Vergleich, Ergebnis −1/0/1 in D0 |
| `fp_mul` | exaktes 106-Bit-Produkt aus `MULU`-Teilprodukten |
| `fp_div` | Divisionsschleife über 64 Quotientenbits |
| `fp_sqrt` | Wurzel durch Schieben und Subtrahieren (56 Bit + Sticky) |
| `fp_from_i32`, `fp_to_int` | Ganzzahl (D0) → Float; Zuweisung an `%`/`&`/`\|` mit Fehler 2/4/3 |
| `fp_round_int`, `fp_round` | `TRUNC`, `INT`, `FRAC`, `CINT`, `ROUND` |
| `fp_approx_eq`, `fp_canon` | `==` (28 Bit, Hälfte von 0 weg); Wert aus dem Speicher normalisieren |
| `rt_rnd` | xorshift32: `rt_randomize`, `rt_rnd_next`, `rt_rnd`, `rt_rand` |

Sprünge werden ohne `.S` geschrieben, siehe [docs/toolchain.md](../../docs/toolchain.md).
