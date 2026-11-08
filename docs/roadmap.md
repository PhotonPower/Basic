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
✔ `VAL`, `DATA`/`READ`; `dataread` läuft im Emulator.
✔ `ON ERROR`, `RESUME`, `ERR`, `ERR$`; `errors` läuft im Emulator.
✔ `PRINT USING`, `STR$(x,y,z)`, `MID$=`, `LSET`/`RSET`; alle Testprogramme laufen im Emulator.
✔ Amiga-Ziel: `basicc --target amiga` schreibt ein AmigaDOS-Programm (Hunk, `platform/amiga`:
`dos.library`, gepufferte Ausgabe, eigener Stapel, Speicher über `AllocMem`). Alle Testprogramme
laufen in der OS-Emulation (`test_m68k_programs`) und unter Kickstart 1.2 im FS-UAE
(`tools/gfa/amiga_run.py`) mit der Ausgabe des Originals.
✔ Transzendente Funktionen (`runtime/ref/rt_math.c`, 68000 bitgleich, probe13).
✔ Zahlenausgabe 2,6-mal schneller (Länge der großen Zahlen nach Bedarf).
✔ Sequentielle Dateien (probe14) in allen Ausführungen; auf dem Amiga im FS-UAE geprüft.
✔ Tastatureingabe (D20), am Original bestätigt (probe15).
✔ Wahlfreie Dateien (probe16) in allen Ausführungen; auf dem Amiga im FS-UAE geprüft.
✔ Ganzzahl-Abkürzungen im Lowering (D16, Nachtrag).
✔ `MKx$`/`CVx` (ohne Speicherzugriff).
✔ Gruppe `memory` (D21): `VARPTR`/`V:`, `PEEK`/`POKE` (8/16/32 Bit), `MALLOC`/`MFREE`, `BMOVE`,
`BYTE{}` … `CHAR{}`.
✔ Ganzzahl-Ketten in 32 Bit (D16, Nachtrag), Zwischenwerte in D4–D7 (D22).
✔ Gruppe `matrix` in allen Ausführungen (probe17, D23), 68000 im Konformanztest bitgleich.
✔ `MAT INPUT #` (probe18).
✔ `ARRPTR`/`*x`, Deskriptoren wie im Original (auch die 68000-Speicherform der Felder), `SWAP *p,a()`,
`TYPE` (probe19).
✔ `POS`, Cursor, `PRINT AT`, `LOCATE`, `HTAB`/`VTAB`, `CLS`, `TIMER`, `RINSTR`; Komma-Zonen ab dem
Anfang der Anweisung (probe20, probe21, D24).
✔ Grafik Stufe 1 (D25): Bildschirmmodell und Rasterregeln vom Original im Interpreter und C++-Ziel.
✔ Grafik auf m68k-test (Ports, der Testtreiber zeichnet mit `brt::Gfx`).
✔ Grafik Stufe 2: Amiga über `intuition.library`/`graphics.library`; `gfx1` unter Kickstart 1.2 mit
den Werten des Originals.
✔ Requester und Menüs Stufe 1 (D26): Textdialoge im Interpreter und C++-Ziel.
✔ probe25: `RESERVE`/`FRE` (D27), `MKDIR`/`RMDIR`/`CHDIR`/`FILES`/`DIR`, `SOUND`, `POLYLINE`/`POLYFILL`
am Original; Polygone in allen Ausführungen, `RESERVE`/`FRE`/`CLEAR`/`SOUND` auch auf dem 68000.
✔ Requester und Menüs auf dem 68000 (D26 Stufe 2a/2b): m68k-test über Ports, Amiga über Intuition
(Menüauswahl mit der Maus noch ungeprüft).
✔ Verzeichnisbefehle auf dem 68000 (Amiga: dos.library; m68k-test: RAM-Disk mit Verzeichnissen).
✔ `PRINT`/`LOCATE` im Grafikfenster auf dem Amiga, `OPENS`/`OPENW` mit Modus und Flags (probe26,
probe27).
✔ Bibliotheksfunktionen beim Namen, `WINDOW()`, `SCREEN()`, `DIR$()` (D28); die drei
Anwenderprogramme starten auf dem Amiga.
✔ Maus und Joystick (D29).
Weiter: Menüauswahl mit der Maus prüfen (von Hand); Felder mit mehr als 3 Dimensionen auf dem 68000; `ABSOLUTE` mit dem vollen Speicherabbild nach D6; weitere Optimierungen (Variablen in Registern innerhalb von Schleifen ohne Aufrufe, Fließkomma inline).

- Quelltext-Linker (`src/link`) mit Map-Datei und Label-Prüfung.
- LIR → 68000-Assembler, `runtime/m68k` (Integer, Strings, Ausgabe, Fließkomma-Kern), `platform/amiga`
  und `platform/m68k-test`.
- Konformanztests der Fließkomma-Module, Differenztests auf m68k-test und amiga.

## 0.5 – Z80 / Amstrad CPC

Stand: ✔ Konvention (D30), ✔ Aufrufhilfe `Z80Routines` und Konformanztest, ✔ Fließkomma-Kern
`fp_add`/`fp_sub`/`fp_neg`/`fp_mul`/`fp_div`/`fp_cmp`/`fp_from_i32`/`fp_to_i32`/`i16`/`u8`
bitgenau zur Referenz, ✔ `fp_sqrt` (Stelle für Stelle), `fp_trunc`/`int`/`frac`/`cint`, `fp_canon`,
`fp_abs`, `==` (`fp_approx_eq`), ✔ Backend `src/backend/z80` (Stapelplätze über IX, Laufzeit mit Zeigern),
✔ Plattform z80-test, ✔ 32-Bit-Integer (`zi`, `rt_int`), ✔ `PRINT` von Strings und Integern,
✔ `basicc --target z80-test`, Testtreiber `z80_programs` (überspringt, was noch fehlt), ✔ Fließkomma-Ausgabe (`ix_bn`, `rt_flt_dec`,
`rt_fmt_flt`, `rt_print_flt`, `DEFNUM`) bitgenau zur Referenz, ✔ Strings nach D18 (`rt_str`: Halde der
Plattform, Bereinigung, Zwischenergebnisse; `rt_strfn`: Verkettung, Vergleich, `LEN`, `ASC`, `CHR$`,
`LEFT$`/`RIGHT$`/`MID$`, `SPACE$`, `STRING$`, `INSTR`, `UPPER$`, `TRIM$`, `STR$`), ✔ Felder (`rt_arr`:
`DIM`, Elemente mit Prüfung, `DIM?`, `ERASE`, `OPTION BASE`, `SWAP`; Feldbereich der Plattform),
✔ Prozeduren und Funktionen mit Parametern, `LOCAL`, `VAR`-Parameter, String-Funktionen, ✔ `ON ERROR`,
`RESUME`, `ERR`, `ERR$`, `ERROR`, ✔ `FOR` mit Integer-Variable, `ROUND`, `VAL`/`VAL?`, `DATA`/`READ`/
`RESTORE`, ✔ transzendente Funktionen und `^` (bitgleich), ✔ Tastatur (`INPUT`, `LINE INPUT`, `INKEY$`,
`INPUT$`), `WRITE`, ✔ Bitfunktionen, `SELECT` mit Strings, `MKx$`/`CVx`, `HEX$`/`BIN$`/`OCT$`, ✔ `PRINT USING`/`MODE`/`STR$(x,y,z)`, `LSET`/`RSET`/`MID$()=`,
`RESERVE`/`FRE`, `TIMER`, ✔ Matrizen. Weiter: Codegröße (Plätze nach Lebensdauer teilen), Gruppe `memory` (Little-Endian, eigene erwartete Ausgaben),
Plattform `cpc` (Firmware-Ausgabe und -Tastatur, Cursor, AMSDOS-Dateien).

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
7. **Am Original prüfen:** ~~`arrays.lst`~~ ist gleich (auch die `VAR`-Grenzfälle), ~~`resume.lst`~~
   ebenso. ~~`errors2.lst`~~ ebenso, seit `RESUME` `ERR` löscht (probe12 T1). ~~probe12 ab T2~~ geklärt:
   Eine Fehlerroutine, die ohne `RESUME` endet, lässt den Fehler unbehandelt (Meldung, Programmende). Fehler in `DEFFN` und verschachtelte Fehler
   siehe [D19](decisions.md#d19). `strheap.lst` ist unkritisch.
