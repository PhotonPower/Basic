# src/frontend

Lexer, Parser, AST und Semantik (Typen, `DEFxxx`, Aliase, Labels, Prozeduren, `VAR`-Parameter,
Strukturprüfung, Befehlsgruppen, Konstantenfaltung mit `runtime/ref`).
Siehe [docs/architecture.md](../../docs/architecture.md#frontend).

| Datei | Inhalt |
|---|---|
| `lexer.*` | Zeilen und Token: Namen mit Typ-Postfix, Zahlenliterale (`&H`, `$`, `&O`, `&X`, `%`), Strings, Kommentare (`REM`, `'`, `!` am Zeilenende außer bei `DATA`), Aliase `=<` `=>` `><` |
| `ast.hpp`, `ast_dump.cpp` | Syntaxbaum: **eine Anweisung je Zeile**, Blöcke werden erst in der Semantik gebildet (wie der Strukturtest F10 des Editors); Textausgabe für `basicc --emit-ast` |
| `parser.*` | rekursiver Abstieg; Rangfolge der Operatoren wie am Original ermittelt ([language.md](../../docs/language.md)); Aliase (`ENDWHILE`, `SUB`, `ENDPROC`, `OTHERWISE` …) werden vereinheitlicht |
| `builtins.*` | eingebaute Funktionen und Konstanten (Name, Ergebnistyp, Argumente) |
| `sema.*` | Semantik: Blöcke und Strukturprüfung, Typen (Postfix, `DEFxxx`), Prozeduren, Funktionen, `DEFFN`, Labels, `DATA`, Konvertierungen → HIR |

Tests: `tests/test_frontend.cpp`, unter anderem liest er alle Prüfprogramme aus `tests/probes`
ohne Fehler.
