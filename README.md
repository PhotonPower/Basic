# Basic – ein GFA-BASIC-Compiler

[![CI](https://github.com/PhotonPower/Basic/actions/workflows/ci.yml/badge.svg)](https://github.com/PhotonPower/Basic/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

Ein Compiler für **GFA-BASIC 3.0/3.5 (Amiga)** in modernem C++20, der über einen Zwischencode auf drei
Arten von Zielsystemen übersetzt:

| Ziel | CPU | Ausgabe |
|---|---|---|
| **Amiga** | MC68000 | AmigaDOS-Programm (Hunk) |
| **Amstrad CPC** | Z80 | Binary mit AMSDOS-Header, `.sna`-Snapshot |
| **PC** | C++ | C++-Quelltext – Referenz für die Tests |

dazu nackte Z80- und MC68000-Testplattformen, die auch als Vorlage für **FPGA-Implementierungen** dienen.

> **Status: Version 0.0.1 – Gerüst.** Es gibt die Projektstruktur, das Zielregister
> (`basicc --targets`), die Emulator-Harness auf Basis der Schwesterprojekte und die Dokumentation des
> Entwurfs. Übersetzt wird noch nichts; der Weg dahin steht in der [Roadmap](docs/roadmap.md).

## Grundideen

- **Zwei Ebenen Zwischencode:** eine strukturierte HIR für lesbaren C++-Code, eine LIR
  (Drei-Adress-Code mit Typen bis `i8`/`i16`) für die Assembler-Backends ([docs/ir.md](docs/ir.md)).
- **Zielsystem = CPU × Plattform:** Mathematik, Strings und Formatierung hängen nur von der CPU ab;
  alles Maschinenspezifische liegt in einer kleinen Plattformschicht ([docs/targets.md](docs/targets.md)).
- **Bitgleiche Numerik auf allen Zielen:** eigene Fließkomma-Bibliothek (IEEE binary64, vereinfacht),
  definiert durch eine C-Referenz, gegen die die Assembler-Ports bitgenau getestet werden
  ([docs/numerics.md](docs/numerics.md)).
- **Feinkörnige Runtime:** eine Routine je Modul; der Compiler linkt auf Quelltextebene nur, was das
  Programm braucht – wichtig auf dem CPC mit rund 40 KB ([docs/runtime.md](docs/runtime.md)).
- **Befehlsgruppen statt Stubs:** Benutzt ein Programm etwas, das das Ziel nicht kann (z. B. Fenster
  auf dem CPC), meldet das der Compiler ([docs/language.md](docs/language.md)).
- **Differenztests:** jedes Testprogramm auf IR-Interpreter, PC, 68000- und Z80-Emulator, alle
  Ausgaben müssen gleich sein ([docs/testing.md](docs/testing.md)).

## Schwesterprojekte

Assembler und Emulatoren kommen aus [PhotonPower/Z80](https://github.com/PhotonPower/Z80) und
[PhotonPower/MC68000](https://github.com/PhotonPower/MC68000); sie werden beim Konfigurieren über
`FetchContent` (fester Commit) geholt und als Bibliotheken benutzt ([docs/toolchain.md](docs/toolchain.md)).

## Schnellstart

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure

./build/basicc --targets        # Zielsysteme und ihre Befehlsgruppen
```

Voraussetzungen: CMake ≥ 3.16, C++20-Compiler (GCC ≥ 11, Clang ≥ 14, MSVC 2022), Netzzugang beim
ersten Konfigurieren.

## Dokumentation

| Dokument | Inhalt |
|---|---|
| [docs/architecture.md](docs/architecture.md) | Aufbau: Frontend, Zwischencode, Backends, Linker |
| [docs/ir.md](docs/ir.md) | HIR und LIR: Typen, Operationen, Regeln |
| [docs/runtime.md](docs/runtime.md) | Laufzeitbibliothek: Modulformat, Linken, Plattformschnittstelle, Aufrufkonventionen |
| [docs/numerics.md](docs/numerics.md) | Fließkommaformat, Ganzzahlen, `==`, Zahl ↔ Text, Zufallszahlen |
| [docs/targets.md](docs/targets.md) | Zielsysteme pc, amiga, cpc, z80-test, m68k-test, Feature-Flags |
| [docs/language.md](docs/language.md) | Sprachumfang, Befehlsgruppen, Unterstützung je Ziel, offene Sprachfragen |
| [docs/toolchain.md](docs/toolchain.md) | Schwesterprojekte, Einbindung, Grenzen der Werkzeuge |
| [docs/testing.md](docs/testing.md) | Emulator-Harness, Differenz- und Konformanztests, Größenbudgets |
| [docs/usage.md](docs/usage.md) | Kommandozeile (geplant) |
| [docs/decisions.md](docs/decisions.md) | Entwurfsentscheidungen mit Begründung |
| [docs/roadmap.md](docs/roadmap.md) | Meilensteine und offene Fragen |
| [CONTRIBUTING.md](CONTRIBUTING.md) | Bauen, Regeln |
| [CHANGELOG.md](CHANGELOG.md) | Änderungen |

## Projektstruktur

```
app/                  basicc (Kommandozeile)
include/basic/        öffentliche Header von basiccore
src/frontend/         Lexer, Parser, AST, Semantik
src/ir/               HIR, LIR, Lowering, IR-Interpreter
src/backend/          C++-, MC68000- und Z80-Backend
src/link/             Quelltext-Linker für Runtime-Module
runtime/ref/          C-Referenz (definiert die Semantik)
runtime/m68k/, z80/   Runtime-Module in Assembler
platform/             pc, amiga, cpc, z80-test, m68k-test
tests/                Unit-Tests, Emulator-Harness, Testprogramme, Konformanztests
examples/             Beispielprogramme (.lst)
docs/                 Dokumentation
tools/                Entwicklerwerkzeuge
```

## Lizenz

[MIT](LICENSE)
