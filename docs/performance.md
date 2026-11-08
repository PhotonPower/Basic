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

Werkzeug dafür: `BASIC_Z80_PROFILE=1 BASIC_Z80_PROGRAM=<datei> build/tests/test_z80_programs` zeigt
die T-States je Routine (Marke im Quelltext; `modul__x` zählt zum Modul, Programmcode als
`program`).
