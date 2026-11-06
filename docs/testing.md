# Tests

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Die Tests brauchen kein Framework (`tests/test_util.hpp` mit `CHECK`/`CHECK_EQ`, wie in den
Schwesterprojekten).

## Ebenen

| Ebene | Ort | Was geprüft wird |
|---|---|---|
| Unit-Tests | `tests/test_*.cpp` | Lexer, Parser, Semantik, IR, Linker, Zielregister |
| Golden-Tests | `tests/programs/` | BASIC-Programm → erwartete Ausgabe (`.lst` + `.out`) |
| Differenztests | `tests/programs/` | dasselbe Programm auf allen Zielen, Ausgaben müssen gleich sein |
| Konformanz der Runtime | `tests/conformance/` | Assembler-Module bitgenau gegen `runtime/ref` |
| Größenbudgets | CI | Programmgröße von Referenzprogrammen je Ziel |

## Emulator-Harness

`tests/harness/` führt erzeugten Code in den Emulatoren der Schwesterprojekte aus und liefert die
Konsolenausgabe ([toolchain.md](toolchain.md)):

| Funktion | Plattform | Ausgabe | Ende |
|---|---|---|---|
| `runZ80Source/Image(…, Z80Platform::Test)` | z80-test | `OUT (&01),A` | `DI` + `HALT` |
| `runZ80Source/Image(…, Z80Platform::Cpc)` | cpc | `CALL &BB5A` (Firmware-Stub) | `RET` vom Einsprung |
| `runM68kSource/Image(…, M68kPlatform::Test)` | m68k-test | `MOVE.B D0,$00FF0000` | `STOP` |
| `runM68kSource/Image(…, M68kPlatform::Amiga)` | amiga | `dos.library` `Write` (OS-Emulation) | Rückkehr / `Exit()` |

Ergebnis: `finished`, `console`, `error`, Befehlszahl, auf dem Z80 auch T-States. Nicht emulierte
CPC-Firmware-Aufrufe brechen mit Meldung ab; nicht emulierte Amiga-Bibliotheksaufrufe werden in
`unemulatedCalls` aufgelistet. `tests/test_harness_z80.cpp` und `tests/test_harness_m68k.cpp` prüfen
die Harness mit handgeschriebenen Programmen (darunter ein echtes AmigaDOS-Hunk-Programm).

## Differenztests

Jedes Programm unter `tests/programs/` wird übersetzt und ausgeführt auf:

1. dem **IR-Interpreter** (Referenz für Frontend und Lowering),
2. dem Ziel **pc** (C++ + `runtime/ref`),
3. **m68k-test** und **amiga** im 68000-Emulator,
4. **z80-test** und **cpc** im Z80-Emulator.

Alle Ausgaben müssen Byte für Byte gleich sein und gleich der Datei `.out`. Programme, die eine
Gruppe benutzen, die ein Ziel nicht hat, werden dort übersprungen (die Gruppen stehen in einer
Kopfzeile des Testprogramms). Zufallszahlen laufen mit festem Startwert.

Erwartete Ausgaben, die das Verhalten des Originals festhalten, werden am echten GFA-BASIC 3.5 (im
Amiga-Emulator FS-UAE, [gfa-original.md](gfa-original.md)) erzeugt und mit dem Vermerk `; source: GFA-BASIC 3.5` gekennzeichnet; so sind sie von
Ausgaben unterscheidbar, die nur unsere eigene Festlegung dokumentieren.

## Konformanz der Runtime

Für jede Routine mit numerischem Ergebnis (Grundoperationen, Konvertierungen, Zahl ↔ Text,
transzendente Funktionen, Zufallsgenerator):

- Ein Generator (`tools/`) erzeugt Operanden: gezielte Grenzfälle (0, kleinste/größte Exponenten,
  Rundungsgrenzen, ±1, Vielfache von π/2, Ganzzahlgrenzen) und viele Zufallswerte mit festem Startwert.
- Die Referenz `runtime/ref` berechnet die Sollwerte.
- Die Harness ruft dieselbe Routine im Emulator direkt auf (Operanden in den Speicher, Aufruf, Ergebnis
  lesen) und vergleicht **bitgenau**.

Der Emulator wird dafür als Bibliothek in das Testprogramm eingebunden, nicht als ganzes System
gestartet; so laufen Millionen Fälle in vertretbarer Zeit. Die CI fährt eine kleinere Stichprobe,
ein eigener Lauf (manuell oder nächtlich) die volle Menge.

Für FPGA-Boards läuft derselbe Test über die serielle Schnittstelle: `plat_putc` schreibt auf den
UART, der PC vergleicht.

## Größenbudgets

Für Referenzprogramme (`PRINT "Hallo"`, ein reines Integer-Programm, eines mit Float-Ausgabe, eines
mit Strings) wird die Programmgröße je Ziel gemessen und gegen eine Obergrenze geprüft. Steigt eine
Größe unerwartet, zeigt die Map-Datei des Linkers, welches Modul hereingezogen wurde.

## Regeln

- Neue Funktionalität bekommt Tests; Fehlerkorrekturen zuerst einen Test, der den Fehler zeigt.
- Jede Optimierung muss die Differenztests bestehen.
- Eine Änderung an `runtime/ref` ist eine Änderung der Semantik: Sie braucht einen Eintrag in
  [decisions.md](decisions.md) und neue Sollwerte für alle Ports.
