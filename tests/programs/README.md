# tests/programs

BASIC-Testprogramme (`name.lst`) mit erwarteter Ausgabe (`name.out`). Jedes Programm läuft auf dem
IR-Interpreter und allen Zielen, deren Befehlsgruppen es abdeckt; alle Ausgaben müssen gleich sein.

Kopfzeilen im Programm (Kommentare):

```
' test: groups core float print
' test: seed 42
' source: GFA-BASIC 3.5      (nur wenn die .out-Datei am Original erzeugt wurde)
```

Siehe [docs/testing.md](../../docs/testing.md#differenztests).

Die Programme schreiben mit `PRINT` auf den Bildschirm. `tools/gfa/gfa_run.py` lenkt diese Ausgabe
im Original automatisch in eine Datei um (Programme ohne `datei$=`-Zeile), sodass dasselbe Programm
auch am echten GFA-BASIC laufen kann:

```sh
python3 tools/gfa/gfa_run.py tests/programs/arith.lst --out tests/programs/arith.out
```

**Stand:** Alle `.out`-Dateien außer `strheap.out`, `errnest.out`, `input.out`, `random.out`, `intfast.out`, `binary.out` und `screen.out` sind am Original erzeugt
(Kopfzeile `' source: GFA-BASIC 3.5`). `strheap` belastet den String-Heap (Speicherbereinigung, auch
mit gesicherten `LOCAL`-Strings), `arrays` Arrays aller Typen, `SWAP`, `ERASE`, `OPTION BASE` und
Grenzfälle von `VAR` (am Original gleich), `input` die Tastatureingabe (Eingabe aus `input.in`, nach
der Befehlsreferenz, D20), `poly1` Polygone (probe25 T8), `dirs` Verzeichnisse und `FILES`/`DIR`, `reserve` `RESERVE`/`FRE`/`CLEAR`/`SOUND`,
`gui1` Requester und Menüs als Textdialoge (D26, Eingabe `gui1.in`), `gfx1` Grafik Stufe 1 (D25, Werte aus probe22–24, Zusammenfassung in `gfx1.gfx`), `umlaut` Namen mit Umlauten und eine zugeklappte Prozedur, `errret` Fehlerroutine ohne `RESUME` (endet mit Fehler 5, `errret.err`), `console` Textausgabe, Cursor, Komma-Zonen, `RINSTR` (aus probe20/21), `screen` dieselben Cursorfälle ohne Dateien (Ausgabe vom Interpreter, Werte wie bei `console`), `memory.z80.out` und `ptr.z80.out` die Ausgaben auf dem Z80 (Little-Endian, 16-Bit-Deskriptoren; von Hand geprüft), `files.cpc.out` die auf dem CPC (AMSDOS: nur eine Ausgabedatei, drei zusätzliche Fehlerzeilen in T8), `ptr` `ARRPTR`/`*x`, Deskriptoren, `SWAP *p,a()`, `TYPE` (aus probe19, Ausgabe vom Original), `matrix` die Matrixbefehle (wie probe17, D23), `matfile` `MAT PRINT #` in eine Datei, `matinput` `MAT INPUT #` und CR bei `INPUT #` (aus probe18, Ausgabe vom Original), `regs` die Registervergabe des 68000 (D22), `memory` `PEEK`/`POKE`/`MALLOC`/`BMOVE` (D21), `binary` `MKx$`/`CVx` (MKD$-Bytes vom Original), `intfast` die Ganzzahl-Abkürzungen der Assembler-Ziele an den Überlaufgrenzen (vom Interpreter),
`files` sequentielle Dateien (aus probe14), `random` wahlfreie Dateien (aus probe16, zwei Zeilen
weichen bewusst ab), `math` die transzendenten Funktionen (aus probe13, auf 8
Stellen gerundet), `resume` die Fortsetzung nach Fehlern in Kopfzeilen
(probe11, am Original gleich), `errnest` einen Fehler in der Fehlerroutine (das Original stürzt
dabei ab, D19), `errors2` Grenzfälle der Fehlerbehandlung (`LOCAL`/`VAR` beim Rücksprung,
Fehler in Prozeduren, Schleifenbedingungen, `DEFFN` und in der Fehlerroutine, `RESUME Marke` aus
einer tieferen Prozedur; am Original gleich).

Differenztests: `test_programs` (Interpreter), `pc_<name>` (C++-Ziel), `m68k_programs`
(MC68000 im Emulator; Programme, die das Backend noch nicht kann, werden übersprungen und gezählt).
