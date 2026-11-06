# examples

Beispielprogramme im ASCII-Format (`.lst`, wie mit `SAVE,A` gespeichert).

| Datei | Inhalt |
|---|---|
| `hallo.lst` | Textausgabe, `FOR`-Schleife, Prozedur mit `LOCAL`, Funktion, `SQR` – das erste Ziel für alle Backends |

Übersetzen (sobald der Compiler so weit ist, siehe [docs/roadmap.md](../docs/roadmap.md)):

```sh
basicc --target cpc   examples/hallo.lst -o HALLO.BIN
basicc --target amiga examples/hallo.lst -o hallo
basicc --target pc    examples/hallo.lst -o hallo.cpp
```
