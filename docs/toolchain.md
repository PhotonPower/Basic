# Werkzeugkette und Schwesterprojekte

Der Compiler benutzt die Assembler und Emulatoren der Schwesterprojekte als **Bibliotheken**:

| Projekt | Bibliothek | Was wir benutzen |
|---|---|---|
| [PhotonPower/Z80](https://github.com/PhotonPower/Z80) | `z80core` | `z80::Assembler` (Zilog-Syntax, `ORG`/`EQU`/`DB`/`DW`/`DS`), `.sna`-Ausgabe, `z80::Cpu` (ZEXDOC/ZEXALL-geprüft, T-States, Port-Handler, `setExecHook` für Firmware-Stubs), CPC-Wissen (`z80::cpc`, Firmware-Symbole, Bildschirm-Rendering) |
| [PhotonPower/MC68000](https://github.com/PhotonPower/MC68000) | `m68kcore` | `m68k::assemble` (Motorola-Syntax, `SECTION` → Hunk), `writeHunkFile`/`loadHunkFile`, `m68k::Cpu` (gegen Musashi geprüft), `Amiga500` (Speicherkarte), `AmigaOs` (ExecBase, `OpenLibrary`, dos `Output`/`Write`/`Exit` …) |

Beide sind MIT-lizenziert, in C++20 geschrieben und folgen denselben Konventionen wie dieses Projekt.

## Einbindung

`CMakeLists.txt` holt beide Projekte mit `FetchContent`, **fest auf einen Commit** gesetzt. Eine
Änderung in einem Schwesterprojekt kann diesen Build also nicht unbemerkt brechen; das Anheben des
Commits ist eine bewusste Änderung (mit Eintrag im `CHANGELOG.md`).

Gegen einen lokalen Checkout entwickeln:

```sh
cmake -S . -B build \
  -DFETCHCONTENT_SOURCE_DIR_Z80TOOLS=../Z80 \
  -DFETCHCONTENT_SOURCE_DIR_M68KTOOLS=../MC68000
```

Ohne Emulatoren bauen (nur Compiler, keine Harness-Tests): `-DBASIC_WITH_EMULATORS=OFF`.
Die Kommandozeilenwerkzeuge `z80tool` und `m68ktool` werden dabei mitgebaut und sind zum Untersuchen
erzeugter Programme nützlich (Disassembler, Monitor).

## Was die Werkzeuge nicht können – und wie der Compiler damit umgeht

| Einschränkung | Folge für den Compiler |
|---|---|
| kein Linker, kein `XREF`, keine Objektdateien | eigener Quelltext-Linker in `src/link` ([runtime.md](runtime.md#linken)) |
| kein `INCLUDE`, keine Makros, keine bedingte Assemblierung | der Linker fügt Module zusammen; Varianten entstehen durch Modulauswahl |
| Z80-Assembler ohne lokale Labels | Label-Hygiene `<modul>__<name>`, vom Linker geprüft |
| Z80-Assembler nur dokumentierte Befehle | Flag `z80-undoc` erst nutzbar, wenn der Assembler sie annimmt |
| 68000-Assembler: keine Befehle des 68010+ | Flag `m68020-muldiv` braucht eine Erweiterung des Assemblers |
| 68000-Assembler: Sektionsadressen nur als lange absolute Adresse/Direktwert/`DC.L` | Backend benutzt PC-relative Adressierung innerhalb von `code`, `.L` für Daten/BSS |
| 68000-Assembler wählt nicht von selbst `MOVEQ`/`ADDQ` | Backend schreibt die kurzen Formen selbst |
| Z80-Emulator: flache 64 KiB, kein Banking | 6128-Banking erst testbar, wenn das Schwesterprojekt es kann |
| 68000-Emulator: kein Timing | Laufzeitvergleiche nur über Befehlszahlen |
| `AmigaOs`: nur wenige Funktionen, keine Fenster/Grafik/Dateien | Harness-Tests auf dem Amiga-Ziel nur für Textausgabe; nicht emulierte Aufrufe werden gemeldet |

## Wünsche an die Schwesterprojekte

Nicht nötig für den Start, aber hilfreich:

- Z80: Ausgabe eines Binarys mit AMSDOS-Header; optional `.dsk`-Image.
- Z80: Speicher mit 6128-Bankumschaltung im Emulator.
- Z80: Firmware-Stubs für `KM READ KEY`/`KM WAIT KEY` und die `CAS`-Routinen als wiederverwendbare
  Komponente (die Harness hat vorerst eigene Stubs).
- MC68000: weitere `dos.library`-Funktionen (Dateien) in `AmigaOs`, damit Dateitests im Emulator laufen.
- MC68000: Zyklenzählung pro Befehl (für Laufzeitvergleiche der Runtime).
