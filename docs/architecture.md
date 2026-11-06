# Architektur

Dieses Dokument beschreibt den geplanten Aufbau des Compilers. Was davon schon existiert, steht in der
[Roadmap](roadmap.md); die Gründe für die Entscheidungen in [decisions.md](decisions.md).

## Überblick

```
Quelltext (.LST, ASCII)
   │  Lexer, Parser                                   src/frontend
   ▼
AST
   │  Semantik: Typen (! | & % # $), DEFxxx, Aliase,  src/frontend
   │  Labels, Prozeduren/Funktionen, VAR-Parameter,
   │  Befehlsgruppen, Konstantenfaltung
   ▼
HIR – strukturiert (Schleifen, IF, SELECT,            src/ir
      Prozeduren noch erkennbar)
   │                    └──► C++-Backend              src/backend/cpp
   │  Lowering
   ▼
LIR – Drei-Adress-Code in Basisblöcken,               src/ir
      typisierte virtuelle Register, Runtime-Aufrufe
   ├──► IR-Interpreter (Referenz, Tests)              src/ir
   ├──► MC68000-Backend → Assembler-Quelltext         src/backend/m68k
   └──► Z80-Backend     → Assembler-Quelltext         src/backend/z80
            │
            ▼
Quelltext-Linker: erzeugter Code + benötigte          src/link
Runtime-Module (Hüllenbildung über requires)
            │
            ▼
Assembler der Schwesterprojekte (Bibliothek)          z80core / m68kcore
            │
            ▼
Hunk-Datei (Amiga) · Binary mit AMSDOS-Header (CPC) · Raw/S-Record (Testplattformen)
```

Der Compiler ist wie die Schwesterprojekte aufgebaut: eine Bibliothek ohne Konsolen-Ein-/Ausgabe
(`basiccore`, Header unter `include/basic/`), ein Kommandozeilenwerkzeug (`basicc`, `app/`) und Tests.

## Zielsystem = CPU × Plattform

Ein Zielsystem ist die Kombination aus einem CPU-Backend und einer Plattformschicht
([targets.md](targets.md)):

```
            ┌────────────────────────────────────────────┐
Programm    │  vom Compiler erzeugter Code               │
            ├────────────────────────────────────────────┤
Runtime     │  Mathe, Integer, Strings, Formatierung      │  ← hängt nur von der CPU ab
(CPU-Ebene) │  runtime/ref (C), runtime/m68k, runtime/z80 │
            ├────────────────────────────────────────────┤
Plattform   │  Startcode, Speicheraufteilung,            │  ← pc, amiga, cpc,
            │  Zeichenausgabe, Tastatur, Timer, Ende      │     z80-test, m68k-test,
            │  optional: Dateien, Grafik, Fenster         │     später FPGA-Boards
            └────────────────────────────────────────────┘
```

Mathematik und Formatierung (Zahl → Text) kennen keine Plattform. Ein neues Zielsystem mit
bekannter CPU (z. B. ein FPGA-Board mit Z80-Kern) braucht deshalb nur ein neues Verzeichnis unter
`platform/` mit Startcode und den Primitiven aus [runtime.md](runtime.md#plattformschnittstelle).

## Frontend

- **Lexer:** Groß-/Kleinschreibung egal, Zahlenliterale `&H`/`$`, `&O`, `&X`/`%`, Kommentare
  (`REM`, `'`, `!` am Zeilenende außer bei `DATA`/`INLINE`), eine Anweisung pro Zeile.
- **Parser:** rekursiver Abstieg mit der Operator-Hierarchie aus Kapitel 3 der Referenz. Aliase
  (`ENDWHILE`→`WEND`, `SUB`→`PROCEDURE`, `=<`→`<=` …) werden schon hier vereinheitlicht.
- **Semantik:** Typ jeder Variablen aus Postfix bzw. `DEFxxx`; Prozedur- und Funktionstabelle;
  `VAR`-Parameter; Strukturprüfung (wie F10 im Editor); jede Anweisung bekommt ihre
  [Befehlsgruppe](language.md#befehlsgruppen), und der Compiler bricht ab, wenn das Zielsystem eine
  verwendete Gruppe nicht anbietet.
- **Konstantenfaltung** mit der C-Referenzbibliothek (`runtime/ref`), damit gefaltete und zur
  Laufzeit gerechnete Werte bitgleich sind ([numerics.md](numerics.md#konstantenfaltung)).

## Zwischencode

Zwei Ebenen, beschrieben in [ir.md](ir.md):

- **HIR** behält die Struktur des Programms. Das C++-Backend erzeugt daraus lesbaren Code, der
  sich gut debuggen lässt.
- **LIR** ist Drei-Adress-Code mit expliziten Typen bis hinunter zu `i8`/`i16`, damit das
  Z80-Backend Byte- und Word-Variablen mit 8/16-Bit-Code übersetzen kann. Runtime-Aufrufe sind eigene
  Operationen (`rtcall`) und werden nicht früh expandiert; jedes Backend entscheidet selbst, was es
  inline erzeugt.

Der **IR-Interpreter** führt LIR direkt aus. Er prüft Frontend und Lowering unabhängig von jedem
Backend und ist neben dem C++-Ziel die Referenz der Differenztests ([testing.md](testing.md)).

## Backends

| Backend | Eingabe | Ausgabe | Besonderheiten |
|---|---|---|---|
| C++ | HIR | C++-Quelltext + `runtime/ref` | emulierter Big-Endian-Speicher für `PEEK`/`POKE`/`VARPTR` ([decisions.md](decisions.md#d6)) |
| MC68000 | LIR | Motorola-Assembler für `m68kcore` | Registerzuteilung über D0–D7/A0–A6, Hunk-Programm mit `SECTION` |
| Z80 | LIR | Z80-Assembler für `z80core` | virtuelle Register in IX-relativen Stapelplätzen, 32-Bit-Operationen als Runtime-Aufrufe |

Die Aufrufkonventionen zwischen erzeugtem Code und Runtime stehen in
[runtime.md](runtime.md#aufrufkonventionen).

## Linken auf Quelltextebene

Die Assembler der Schwesterprojekte haben keinen Linker, kein `XREF` und kein `INCLUDE`
([toolchain.md](toolchain.md)). Deshalb linkt der Compiler selbst, und zwar auf Quelltextebene:

1. Das Backend erzeugt den Assembler-Quelltext des Programms und die Liste der referenzierten
   Runtime-Symbole.
2. Der Linker (`src/link`) liest die Kopfzeilen aller Runtime-Module (`;! provides`, `;! requires`)
   und bildet die transitive Hülle.
3. Er fügt Programm und benötigte Module zu **einem** Assembler-Quelltext zusammen, sortiert nach
   Code, Daten und BSS, und prüft dabei doppelte und fehlende Symbole sowie die Label-Hygiene der
   Module.
4. Der Assembler der Schwesterprojekte wird als Bibliothek aufgerufen; eine Map-Datei zeigt, welches
   Modul wie viele Bytes beiträgt.

Damit ist die Bibliothek beliebig feinkörnig, ohne dass die Werkzeuge Objektdateien kennen müssen.
Das Format der Module beschreibt [runtime.md](runtime.md#modulformat).

## Verzeichnisse

```
app/                  basicc (Kommandozeile)
include/basic/        öffentliche Header von basiccore
src/frontend/         Lexer, Parser, AST, Semantik
src/ir/               HIR, LIR, Lowering, IR-Interpreter
src/backend/cpp/      C++-Backend
src/backend/m68k/     MC68000-Backend
src/backend/z80/      Z80-Backend
src/link/             Quelltext-Linker für Runtime-Module
runtime/ref/          C-Referenzimplementierung (definiert die Semantik)
runtime/m68k/         MC68000-Module (Assembler)
runtime/z80/          Z80-Module (Assembler)
platform/<name>/      Plattformschicht je Zielsystem
tests/                Unit-Tests, Emulator-Harness, BASIC-Testprogramme, Konformanztests
examples/             Beispielprogramme
docs/                 Dokumentation
tools/                Entwicklerwerkzeuge (Testgeneratoren, CI-Hilfen)
```
