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

Sprünge werden ohne `.S` geschrieben, siehe [docs/toolchain.md](../../docs/toolchain.md).
