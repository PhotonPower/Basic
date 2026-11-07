# Roadmap

> Pläne, keine fertigen Funktionen. Reihenfolge und Umfang sind Vorschläge.

## 0.0 – Gerüst (erledigt)

Verzeichnisstruktur, CMake mit den Schwesterprojekten (fester Commit), CI, Zielregister
(`basicc --targets`), Emulator-Harness für z80-test, cpc, m68k-test und amiga mit Smoke-Tests,
Dokumentation von Architektur, Zwischencode, Runtime, Numerik, Zielsystemen und Tests.

## 0.1 – Numerische Referenz

- ✔ `runtime/ref`: `fp_add/sub/mul/div/sqrt/cmp`, `==`, Konvertierungen Integer ↔ Float,
  `INT`/`TRUNC`/`FRAC`/`CINT`/`ROUND`.
- ✔ Zahl → Text (13 Stellen, `DEFNUM`) und Text → Zahl (`VAL`, `VAL?`), korrekt gerundet.
- ✔ Zufallsgenerator.
- ✔ Unit-Tests der Referenz gegen die IEEE-Arithmetik des Rechners und die Ergebnisse am Original.
- ✔ `PRINT USING`, `STR$(x,n)` und `STR$(x,y,z)` (Regeln aus probe4/probe5, D12, D13).
- Generator für Konformanzfälle (`tools/`).

## 0.2 – Frontend und IR

- ✔ Lexer, Parser (`basicc --emit-ast`).
- ✔ Semantik für den Sprachkern ([language.md](language.md#reihenfolge-der-umsetzung)), HIR
  (`basicc --emit-hir`).
- ✔ Interpreter auf der HIR (`basicc --run`), erste Golden-Tests unter `tests/programs/`
  ([D17](decisions.md#d17)); alle Golden-Tests stimmen Byte für Byte mit dem Original überein.
- LIR und Lowering folgen mit 0.4.

## 0.3 – C++-Ziel

- ✔ C++-Backend aus der HIR (`basicc --target pc`), `platform/pc` (gemeinsame Laufzeitschicht mit dem
  Interpreter).
- ✔ Differenztest: alle Golden-Tests übersetzt, gebaut und mit den Ausgaben des Originals verglichen.
- Emulierter Speicher (`PEEK`/`POKE`, `VARPTR`, D6) folgt mit der Gruppe `memory`.

## 0.4 – MC68000 / Amiga

Stand: ✔ Quelltext-Linker (`src/link`), ✔ Plattform `m68k-test`, ✔ Konformanz-Harness
(`tests/test_conformance_m68k.cpp`), ✔ der ganze Fließkomma-Kern bitgenau (Grundrechenarten,
Wurzel, Vergleiche, Konvertierungen, Rundungsfunktionen, Zufall), ✔ Zahlenausgabe `rt_fmt_flt`,
✔ LIR und Lowering, ✔ 68000-Backend für Zahlen, Kontrollfluss, Prozeduren und `PRINT`
(`basicc --target m68k-test`); das Golden-Programm `numbers` läuft im Emulator.
✔ Strings (Heap mit Speicherbereinigung, D18); `control` läuft im Emulator.
✔ Bit- und Ganzzahlfunktionen; `arith` läuft im Emulator.
✔ `VAR`, Arrays, `SWAP`, `CLR`, `ERASE`; `procs` läuft im Emulator.
Weiter: `DATA` und `VAL`, `ON ERROR`, `PRINT USING`,
dann das Amiga-Ziel.

- Quelltext-Linker (`src/link`) mit Map-Datei und Label-Prüfung.
- LIR → 68000-Assembler, `runtime/m68k` (Integer, Strings, Ausgabe, Fließkomma-Kern), `platform/amiga`
  und `platform/m68k-test`.
- Konformanztests der Fließkomma-Module, Differenztests auf m68k-test und amiga.

## 0.5 – Z80 / Amstrad CPC

- LIR → Z80-Assembler, `runtime/z80`, `platform/cpc` (AMSDOS-Header, `.sna`) und `platform/z80-test`.
- Konformanz- und Differenztests, Größenbudgets.

## Danach

- Transzendente Funktionen, `PRINT USING`, Arrays, `ON ERROR`/`RESUME`, `INPUT`, Dateien, `memory`, `matrix`.
- Grafik (Amiga, CPC), dann GUI, Sprites, Sound, Kapitel 11 (Amiga).
- Optimierungen (Wertebereiche, Peephole).
- FPGA-Plattformen auf Basis von `z80-test`/`m68k-test`; Feature-Flags.
- CPC 6128: Bankumschaltung für Strings/Arrays.

## Offene Fragen

1. **CPC-Modelle:** nur 6128 oder auch 464/664? (Betrifft Speichergrenzen und ob Banking geplant
   werden kann; die Firmware-Sprungleiste für Text/Grafik/Dateien ist gleich.)
2. **Befehlsreferenz im Repository?** Die Referenz ist aus dem Handbuch von 1988 extrahiert. Soll sie
   unter `docs/` liegen, oder bleibt sie außerhalb (Rechtefrage des Handbuchtextes)?
3. ~~**Zugang zum Original**~~ – vorhanden: GFA-BASIC 3.5 im FS-UAE auf dem Mac; automatisiert über
   `tools/gfa/gfa_run.py` ([gfa-original.md](gfa-original.md)). probe1 ist ausgewertet
   (`tests/probes/results/probe1.txt`), ebenso probe2 und probe3.
4. ~~**GFA-Float-Format** (`FLOAT{}`, `MKD$`/`CVD`)~~ – geklärt (probe1 T42–T45): 48 Bit Mantisse mit
   führender 1, 16 Bit Exponent mit Bias `$3FE`, Vorzeichen durch Negieren des Exponentworts
   ([numerics.md](numerics.md#fließkommaformat)). `MKS$` ist im Original offenbar fehlerhaft (probe2).
5. **Amiga-Ausgabe:** Shell-Ausgabe (`Output()`) als Standard oder von Anfang an ein eigenes Fenster
   wie im Original?
6. ~~**Festlegung D4 gegen das Original**~~ – entschieden: binary64 bleibt, Eigenheiten werden
   nicht nachgebildet ([D11](decisions.md#d11)). Ausgangslage war: GFA-BASIC 3.5 rechnet mit 48 Bit Mantisse und 16 Bit
   Exponent (Bereich bis etwa 10^±9500); das ändert sichtbare Ergebnisse (`FOR x=0 TO 1 STEP 0.1`: 10
   statt 11 Durchläufe, probe1 T21; `1E300*1E300` = `1.0E+600` statt Überlauf, probe2 T15). `==`
   folgt einer anderen Regel als in [numerics.md](numerics.md#ungefähre-gleichheit-) angenommen
   (probe1 T41, probe2 T34–T39). Dazu kommen Eigenheiten, bei denen fraglich ist, ob wir sie
   nachbilden wollen: die ungenaue Konstante `PI` und das fehlerhafte `MKS$`.
