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
