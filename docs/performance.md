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
