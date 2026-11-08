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
Kopfzeile des Testprogramms). Zufallszahlen laufen mit festem Startwert. Eine Datei `<name>.in` liefert die
Tastatureingabe. Zeichnet ein Programm, steht die erwartete Zusammenfassung der Grafik (D25) in
`<name>.gfx`. Endet ein Programm absichtlich mit einem nicht abgefangenen Fehler, steht dessen
Nummer in `<name>.err`: Die Ausgabe muss dann `.out` sein, und das Programm muss mit genau diesem
Fehler enden (auf den 68000-Zielen folgt in der Konsole `runtime error <n>`).

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

## Messprogramme

`tests/programs/bench_*.lst` laufen wie alle Testprogramme; `tools/bench.py` sammelt Befehle,
T-States und Größe auf z80-test und m68k-test und vergleicht mit `tests/bench/baseline.json`
([performance.md](performance.md)).

## Z80

`test_conformance_z80` vergleicht die Module von `runtime/z80` mit `runtime/ref` (wie beim 68000,
zusätzlich die Integer-Routinen `zi_*`/`rt_int` gegen C). `test_z80_programs` übersetzt jedes
Testprogramm für z80-test und für cpc und vergleicht die Ausgabe; was Backend oder Laufzeit noch
nicht können, wird übersprungen und mit dem Grund aufgeführt (`BASIC_Z80_PROGRAM=<datei>` für ein
einzelnes Programm). Auf cpc laufen die Firmware-Aufrufe als Stubs (`TXT OUTPUT`, `KM WAIT CHAR`,
`KM READ CHAR`, `KL TIME PLEASE`), das Programm wird wie aus der Datei ohne BSS in einen mit &E5
gefüllten Speicher geladen, und CR vor LF zählt nicht. Programme mit Tastatureingabe laufen nur auf
z80-test, weil der CPC die getippten Zeichen auf dem Bildschirm wiederholt. Der Treiber nennt die
Größe jedes Programms (`prg_end` minus Ladeadresse). Dateien landen auf z80-test in einer RAM-Disk
des Testtreibers (Anfrageblock über Port `&10`, Fehler wie die RAM-Disk von m68k-test), auf cpc in
einem AMSDOS-Modell (`CAS IN …`/`CAS OUT …`, `|ERA`). Wo die Grenzen von AMSDOS ein Programm
treffen, gilt auf cpc `<name>.cpc.out` (`files`).

`test_z80_firmware` (`z80_firmware`) lässt die cpc-Programme ohne Tastatureingabe und ohne Dateien auf
der **echten CPC-6128-Firmware** laufen. Die Firmware wird im Emulator des Schwesterprojekts einmal bis
zum BASIC-Prompt gebootet, jedes Programm wird geladen und wie von BASIC mit `CALL &100` gestartet
(unteres ROM aus, 300-Hz-Interrupts). Mitgeschrieben wird, was an `TXT OUTPUT`, `TXT SET CURSOR` und
`TXT CLEAR WINDOW` ankommt; die Firmware zeichnet es dabei wirklich. `numbers` und `matrix` laufen
zusätzlich über einen Snapshot (`basicc -o x.sna`), der in eine frische Maschine geladen wird. Der Test
braucht das 32-KiB-ROM-Abbild in `CPC_ROM`; ohne meldet er „skipped“ und gilt als bestanden. AMSDOS
ist ein eigenes ROM und fehlt dort, deshalb laufen Dateiprogramme nur gegen das AMSDOS-Modell.
Das ROM eines 464 oder 664 geht ebenso (`<name>.cpc464.out`, wo der 464 abweicht: keine
Linienmuster). Grafikprogramme laufen nur hier (`gfx1.cpc.out`); mit `BASIC_CPC_SCREENS=<verzeichnis>` schreibt der
Test den Bildschirm nach jedem Programm als PNG, zum Ansehen. Wo der Z80 andere Bytes sieht (Gruppe `memory`:
Little-Endian, Deskriptoren mit 16-Bit-Adressen), gilt `<name>.z80.out` statt `<name>.out`; diese
Dateien sind von Hand Zeile für Zeile gegen das Speicherlayout geprüft (`memory`, `ptr`).
