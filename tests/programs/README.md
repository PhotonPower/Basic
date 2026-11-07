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

**Stand:** Alle `.out`-Dateien außer `strheap.out`, `errnest.out`, `input.out`, `random.out` und `intfast.out` sind am Original erzeugt
(Kopfzeile `' source: GFA-BASIC 3.5`). `strheap` belastet den String-Heap (Speicherbereinigung, auch
mit gesicherten `LOCAL`-Strings), `arrays` Arrays aller Typen, `SWAP`, `ERASE`, `OPTION BASE` und
Grenzfälle von `VAR` (am Original gleich), `input` die Tastatureingabe (Eingabe aus `input.in`, nach
der Befehlsreferenz, D20), `intfast` die Ganzzahl-Abkürzungen der Assembler-Ziele an den Überlaufgrenzen (vom Interpreter),
`files` sequentielle Dateien (aus probe14), `random` wahlfreie Dateien (aus probe16, zwei Zeilen
weichen bewusst ab), `math` die transzendenten Funktionen (aus probe13, auf 8
Stellen gerundet), `resume` die Fortsetzung nach Fehlern in Kopfzeilen
(probe11, am Original gleich), `errnest` einen Fehler in der Fehlerroutine (das Original stürzt
dabei ab, D19), `errors2` Grenzfälle der Fehlerbehandlung (`LOCAL`/`VAR` beim Rücksprung,
Fehler in Prozeduren, Schleifenbedingungen, `DEFFN` und in der Fehlerroutine, `RESUME Marke` aus
einer tieferen Prozedur; am Original gleich).

Differenztests: `test_programs` (Interpreter), `pc_<name>` (C++-Ziel), `m68k_programs`
(MC68000 im Emulator; Programme, die das Backend noch nicht kann, werden übersprungen und gezählt).
