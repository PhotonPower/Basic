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

**Stand:** Alle `.out`-Dateien außer `strheap.out` sind am Original erzeugt (Kopfzeile
`' source: GFA-BASIC 3.5`). `numbers` deckt den Umfang des MC68000-Backends ab; `strheap` belastet den
String-Heap (Speicherbereinigung, auch mit gesicherten `LOCAL`-Strings) und stammt vom Interpreter.

Differenztests: `test_programs` (Interpreter), `pc_<name>` (C++-Ziel), `m68k_programs`
(MC68000 im Emulator; Programme, die das Backend noch nicht kann, werden übersprungen und gezählt).
