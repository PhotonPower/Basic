# Laufzeit und Codegröße (Meilenstein 0.6)

Optimierungen müssen dieselben Ergebnisse liefern wie vorher (alle Tests, besonders die
Differenztests gegen das Original und `intfast` gegen den Interpreter) und ihre Wirkung belegen.

## Messen

Die Messprogramme `tests/programs/bench_*.lst` bilden typische Rechenkerne ab und sind zugleich
Testprogramme (erwartete Ausgabe vom Interpreter):

| Programm | Inhalt |
|---|---|
| `bench_sieve` | Sieb des Eratosthenes, `FOR` mit variabler Schrittweite, Byte-Feld |
| `bench_sort` | Bubblesort von 150 Zahlen, Ganzzahlarithmetik, `SWAP` von Elementen |
| `bench_float` | Mandelbrot 32 × 12 mit Fließkomma, `SQR`, `SIN` |
| `bench_string` | Verketten, `MID$`, `INSTR`, `STR$`, Vergleiche |
| `bench_calls` | Rekursion (`fib`, `ack`), Prozeduren mit `LOCAL` und `VAR` |

```sh
python3 tools/bench.py            # Tabelle, Änderung gegenüber der Ausgangsmessung
python3 tools/bench.py --save     # die Zahlen als neue Ausgangsmessung (tests/bench/baseline.json)
```

Gemessen wird auf z80-test (Befehle, T-States, Größe) und m68k-test (Befehle, Größe; der
68000-Emulator zählt keine Takte). Die Ausgangsmessung ist der Stand vor 0.6 (Commit `99e5724`).

## Ausgangsmessung

| Programm | Z80 T-States | Z80 Bytes | 68000 Befehle | 68000 Bytes |
|---|---:|---:|---:|---:|
| `bench_calls` | 64 560 840 | 7 084 | 826 673 | 4 352 |
| `bench_float` | 2 449 042 316 | 12 684 | 10 774 234 | 12 224 |
| `bench_sieve` | 551 613 641 | 9 019 | 5 929 742 | 6 832 |
| `bench_sort` | 751 106 597 | 9 575 | 8 691 209 | 7 528 |
| `bench_string` | 133 308 544 | 8 534 | 896 332 | 7 098 |

Auf einem CPC (rund 4 MHz) wären das für `bench_float` etwa 10 Minuten, für `bench_sieve` gut zwei.

## Schritte

1. `FOR` mit `%`-Variable und ganzzahliger, nicht konstanter Schrittweite in 32 Bit statt in
   Fließkomma ([D16](decisions.md#d16), Nachtrag): `bench_sieve` Z80 −70 % T-States, 68000 −68 %
   Befehle.
2. `FOR` mit einer Kette aus `+ - *` als Ende (`TO i%-1`) in 32 Bit; läuft die Kette über, prüft
   die Schleife in Fließkomma weiter (der Rumpf bleibt einmal im Code): `bench_sort` Z80 −65 %,
   68000 −61 %.
3. Z80, Feldzugriff (`rt_arr_elem`): keine Multiplikation mit dem Multiplikator 1 und nach der
   letzten Dimension, die Elementgröße (1, 2, 4, 8) als Verschiebung: `bench_sort` −42 %.
4. Z80, Vergleich und Sprung: Liest nur der folgende Sprung das Ergebnis eines Vergleichs, setzt
   `zi_cmpf` die Flags (Z gleich, CF kleiner) und ein bedingter Sprung folgt; `gt`/`le` tauschen
   die Operanden. Kein −1/0 im Speicher, keine Fallunterscheidung über `zi_cmp_code`.

5. Z80, `fp_mul`: byteweises Schulverfahren (Zeile a[i] · b mit Übertrag in einem Byte,
   ausgeschriebene 8×8-Multiplikation, Null-Bytes übersprungen) statt 53 Schritten über das ganze
   Produkt; nach dem Produkt nur die 9 gebrauchten Bytes verschieben. `fp_w_shl1`/`fp_w_shr1` und
   das Entpacken des Exponenten ohne Schleife. Bitgleich (Konformanztest): `bench_float` −59 %.
6. Z80, `fp_mul` mit einer Tabelle der Viertelquadrate (a·b = T[a+b] − T[|a−b|], T[n] = ⌊n²/4⌋,
   1 KB an einer Seitengrenze im BSS, beim ersten Aufruf gefüllt): `fp_mul` −24 %, `bench_float`
   −13 %; die Programme brauchen gut 1,2 KB mehr Speicher (nicht in der Datei).

7. 68000: `fp_round_pack` normalisiert um 16 und 8 Bit auf einmal, bitweise nur noch höchstens
   7 Schritte (kleine ganze Zahlen als Fließkomma brauchten bis zu 63): `bench_calls` −23 %
   Befehle. `rt_arr_elem` ohne Multiplikation mit 1 und nach der letzten Dimension, die
   Elementgröße als Verschiebung: `bench_sort` −34 % Befehle. `fp_mul` rechnet schon mit `MULU`
   (16 Teilprodukte 16 × 16 Bit); ohne Taktzählung im Emulator lässt sich dort nichts belegen.

8. `STR$` einer Ganzzahl und `PRINT` einer Ganzzahl (auch auf dem 68000) schreiben bei
   `DEFNUM` 13 die Ziffern direkt (`rt_fmt_int`, `rt_str_int`) statt über die Fließkomma-Formatierung
   mit Großzahlen; bei kleinerem `DEFNUM` bleibt der Weg über Fließkomma (dort wird gerundet,
   probe2 T33). Grenzfälle in `intfast`: `bench_string` Z80 −71 % T-States, 68000 −41 % Befehle.

9. Z80, Laden von Variablen: Liest nur dieselbe Anweisung den geladenen Wert, und ändert bis dahin
   nichts die Variable (kein Speichern in sie, kein Schreiben über eine Adresse, kein Aufruf, keine
   Laufzeitroutine mit einer Adresse, kein Sprung), wird er nicht in einen Stapelplatz kopiert: Der
   Operand ist die Variable selbst. Fehler kommen nicht dazwischen, weil die Fehlerroutine an einer
   Anweisungsgrenze fortsetzt.
10. Z80, `zi_add`/`zi_sub` ohne Schleife, `zi_mulv` nur über die signifikanten Bytes des kleineren
    Faktors (für `*2` 8 statt 32 Schritte); im Konformanztest gegen C.
11. Z80, Codegröße für den CPC (ein Sternkartenprogramm mit gut 500 Zeilen brauchte 65 KB):
    - `LOCAL` und Parameter: `CALL rt_lsave / DW v` und `CALL rt_lrest / DW v` statt vier
      `LD HL,(v+n)` / `PUSH` bzw. `POP` / `LD (v+n),HL` (5 statt 16–17 Byte), Nullsetzen mit
      `CALL rt_zero8` (6 statt 26 Byte).
    - Laden und Speichern einer Fließkomma- oder Ganzzahlvariablen in einen nahen Stapelplatz:
      `CALL rt_ld8 / DB d / DW v` (bzw. `rt_ld4`, `rt_st8`, `rt_st4`), 6 statt 12 Byte.
    - Aufruf einer Laufzeitroutine mit Zahlenoperanden in nahen Stapelplätzen über ein Sprungbrett:
      `CALL zc3 / DB a,b,d / DW routine` statt dreimal `rt_ix*` und `CALL` (8 statt 15 Byte; `zc2`,
      `zc2d` für zwei Operanden ohne bzw. einen mit Ergebnis). Die Routine bekommt dieselben Register
      wie beim direkten Aufruf und kehrt direkt zurück. Ausgenommen sind die Ganzzahlroutinen `zi_*`:
      Sie stehen in den engen Schleifen, und über das Sprungbrett lief `bench_sieve` 12 % länger.

    Das Sternkartenprogramm: Programmcode 30 935 → 26 095 Byte (−16 %), es passt damit unter 64 KB.
    Die 40 Testprogramme, die auf `cpc` laufen: zusammen 457 524 → 454 835 Byte (−0,6 %; sie benutzen
    kaum `LOCAL`). Preis: Die
    Benchmarks brauchen auf dem Z80 bis zu 6 % mehr T-States (`bench_calls` +6,3 %, `bench_sieve`
    +6,1 %, `bench_sort` +2,7 %, `bench_float` und `bench_string` unter 1 %); ihre Größe ändert sich
    kaum, weil die neuen Module die Ersparnis bei so kleinen Programmen aufwiegen.
12. Z80, Laufzeit feiner geschnitten: Ein Programm, das Zahlen nie als Text liest oder schreibt (nur
    Strings und Ganzzahlen ausgibt, Strings liest), bekam trotzdem die Umwandlung Text ↔ Fließkomma
    (`rt_val`, `rt_flt_dec`, `rt_fmt_flt`, `ix_bn`, zusammen 3,7 KB), weil `rt_print`, `rt_data`,
    `rt_kbd` und `rt_strfn` sie pauschal verlangten. Jetzt in eigenen Modulen: `rt_print_flt`
    (`PRINT` einer Fließkommazahl, `DEFNUM`), `rt_data_num` (`READ` in eine Zahl), `rt_kbd_num`
    (`INPUT` in eine Zahl), `rt_str_str` (`STR$` einer Fließkommazahl). Zwei Fälle brauchten mehr:
    - `PRINT` einer Ganzzahl geht bei `DEFNUM` unter 13 über die Fließkommaform (probe2 T33);
      `rt_print_int` springt dafür über den Zeiger `rt_print_low`, den erst `rt_set_defnum` (in
      `rt_print_flt`) umstellt. `rt_print_int` bleibt so im Grundmodul (`plat_error` braucht es).
    - `STR$` einer Ganzzahl: Hat das Programm kein `DEFNUM`, ist der Wert immer 13, und das Lowering
      ruft `rt_str_dig` (nur Ziffern) statt `rt_str_int`. Ebenso werden `PRINT 7` und `STR$(7)` mit
      einer ganzzahligen Konstanten im 32-Bit-Bereich über den Ganzzahlweg ausgegeben (dieselben
      Ziffern; −0 ausgenommen).

    Ein kleines Grafikprogramm mit `LINE INPUT`, `INKEY$`, `READ` in Strings und `STR$` von
    Ganzzahlen: Laufzeit 18 461 → 14 698 Byte (−20 %). Die 40 Testprogramme auf `cpc`: 454 835 →
    437 778 Byte (−3,7 %).

Werkzeug dafür: `BASIC_Z80_PROFILE=1 BASIC_Z80_PROGRAM=<datei> build/tests/test_z80_programs` zeigt
die T-States je Routine (Marke im Quelltext; `modul__x` zählt zum Modul, Programmcode als
`program`); `BASIC_Z80_PROFILE=2` zeigt jede Marke einzeln. Für den 68000 zeigt
`BASIC_M68K_PROFILE=1` (bzw. `=2`) mit `build/tests/test_m68k_programs` die Befehle und Datenzugriffe je
Routine auf m68k-test (der Emulator zählt keine Takte).
