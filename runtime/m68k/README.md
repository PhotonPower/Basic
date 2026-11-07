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
| `ix_bn` | Big-Integer fester Länge (Langwörter, höchstwertiges zuerst): Multiplikation mit 16-Bit-Faktoren, Schieben, Vergleich, Subtraktion |
| `rt_flt_dec`, `rt_fmt_flt` | Dezimalziffern des exakten Binärwerts; `PRINT`/`STR$`-Form mit `DEFNUM` |
| `rt_str` | String-Heap nach D18: Deskriptoren, Blöcke mit Rückverweis, Zusammenschieben, Zwischenergebnisse, Zuweisung, Rückgabe von String-Funktionen |
| `rt_strfn` | `MID$=`, `LSET`, `RSET`, Verketten, Vergleich, `LEFT$`/`RIGHT$`/`MID$`, `INSTR`, `CHR$`, `ASC`, `LEN`, `SPACE$`, `STRING$`, `UPPER$`, `TRIM$`, `STR$`, `HEX$`/`BIN$`/`OCT$` |
| `rt_arr` | Arrays: `DIM` (bis 3 Dimensionen), Elementadresse mit Prüfung (Fehler 14–18), `DIM?`, `ERASE`, `OPTION BASE`, `SWAP` ganzer Arrays |
| `rt_val`, `rt_strval` | `VAL`/`VAL?` korrekt gerundet (bis 800 Ziffern, Bignum-Länge nach Bedarf), Präfixe `&H $ &O &X %` |
| `rt_data` | `DATA`/`READ`/`RESTORE` über die Tabelle `prg_data` des Programms (Fehler 34, 35) |
| `rt_err`, `rt_onerr`, `rt_resume`, `rt_errtext` | Laufzeitfehler, `ON ERROR GOSUB`, `RESUME` (auch `NEXT`, Marke), `ERR`, `ERR$` |
| `rt_using`, `rt_strfmt` | `PRINT USING` (Nachbau von `rt_using.c`, Zustand auf einem kleinen Stapel), `MODE`, `STR$(x,n)`, `STR$(x,y,z)` |
| (Plattform) | `plat_putc`, `plat_exit`, `plat_error`, `plat_alloc` (Speicher für String-Heap und Array-Bereich beim ersten Gebrauch) aus `platform/m68k-test` bzw. `platform/amiga` |
| `rt_math` | `EXP`, `LOG`, `LOG10`, `SIN`, `COS`, `TAN`, `ATN`, `ASIN`, `ACOS`, `DEG`, `RAD`, `x^y` mit gebrochenem Exponenten; **erzeugt** von `tools/gen/gen_rt_math.py` aus `runtime/ref/rt_math.c` |
| `rt_file` | Dateien: Kanäle 0–99 mit Puffern, `OPEN` (`I O A U R`)/`CLOSE`, `PRINT #`/`WRITE #` (über die Ausgabeadresse `rt_print_out`), `INPUT #`, `LINE INPUT #`, `INPUT$`, `EOF`, `LOF`, `EXIST`, `KILL`, `NAME`; wahlfrei `FIELD`, `GET`, `PUT`, `RECORD`, `SEEK`, `RELSEEK`, `LOC`, `INP`, `OUT`; nutzt `plat_f*` der Plattform |
| `rt_kbd` | Tastatur: `INPUT`, `LINE INPUT`, `INKEY$`, `INPUT$(n)` über `plat_getc` (D20) |
| `rt_bits` | `BTST`/`BCLR`/`BSET`/`BCHG`, `SHL`/`SHR`/`ROL`/`ROR` (8/16/32 Bit), `SWAP`, `WORD`, `CARD`, `BYTE` |
| `rt_print`, `rt_int`, `rt_for`, `rt_power`, `rt_end` | Ausgabe, Ganzzahlhilfen, `FOR`, Potenz, Programmende |

Offen: `rt_fmt_flt` braucht im Mittel rund 35 000 Befehle je Zahl (Big-Integer-Division über 51
Quotientenbits). Ein schneller Weg für gewöhnliche Werte kommt später; die Konformanztests sichern
ihn ab.

Sprünge werden ohne `.S` geschrieben, siehe [docs/toolchain.md](../../docs/toolchain.md).
