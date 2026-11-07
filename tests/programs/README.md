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

**Stand:** Die `.out`-Dateien aller Programme sind am Original erzeugt (Kopfzeile
`' source: GFA-BASIC 3.5`); der Referenz-Interpreter liefert Byte für Byte dasselbe.
