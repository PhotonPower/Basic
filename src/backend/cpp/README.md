# src/backend/cpp

C++-Backend (Ziel `pc`): erzeugt aus der HIR lesbaren C++17-Quelltext, der mit `runtime/ref` und
`platform/pc` übersetzt wird (`basicc --target pc programm.lst -o programm.cpp`).

- Prozeduren und Funktionen werden C++-Funktionen; Parameter und `LOCAL` sichert ein `brt::Frame`,
  `VAR`-Parameter sind Referenzen auf die Variable des Aufrufers.
- Schleifen, `IF` und `SELECT` bleiben als Struktur erhalten; `GOTO` wird `goto`.
- Benutzt das Programm `ON ERROR`, wird jede Anweisung in `try`/`catch` gefasst, damit `RESUME`,
  `RESUME NEXT` und `RESUME marke` an der richtigen Stelle aufsetzen; sonst nicht.
- Wo die Reihenfolge der Auswertung zählt (Argumente mit Funktionsaufrufen oder `RND`), entstehen
  Zwischenvariablen, weil C++ die Reihenfolge von Funktionsargumenten offenlässt.

Differenztest: `tests/CMakeLists.txt` übersetzt jedes Programm aus `tests/programs`, baut es und
vergleicht die Ausgabe mit der `.out`-Datei vom Original (Tests `pc_<name>`).
