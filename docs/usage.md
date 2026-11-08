# Bedienung

> Stand: Umgesetzt sind `--version`, `--help`, `--targets`, `--emit-ast <programm.lst>` und
> `--emit-hir <programm.lst>`, `--run <programm.lst> [--seed <n>]` (Referenz-Interpreter) und
> `--target pc <programm.lst> [-o <datei.cpp>]` (C++-Quelltext, siehe `platform/pc/README.md`),
> `--target amiga <programm.lst> [-o <programm>]` (AmigaDOS-Programm als Hunk-Datei; mit `-o x.s`
> der Assembler-Quelltext), `--target m68k-test <programm.lst> [-o <datei.s>]` (MC68000-Assembler
> für den nackten Prozessor), `--target cpc <programm.lst> [-o <datei>]` (Binärdatei mit
> AMSDOS-Header; `-o x.s` Quelltext; `-o x.sna --rom <6128.rom>` Snapshot), `--target z80-test` und
> `--emit-lir <programm.lst>`
> (Syntaxbaum, eine Zeile je Anweisung; Syntaxfehler als `datei:zeile:spalte: meldung` auf stderr,
> Rückgabewert 1). Der Rest beschreibt die geplante Kommandozeile.

```sh
basicc --targets                                   # Zielsysteme und ihre Befehlsgruppen
basicc --target cpc   programm.lst -o PROGRAMM.BIN # Amstrad CPC: Binary mit AMSDOS-Header
basicc --target cpc   programm.lst -o prog.sna --rom cpc6128.rom  # CPC-Snapshot für Emulatoren
basicc --target amiga programm.lst -o programm     # Amiga: AmigaDOS-Programm (Hunk)
basicc --target pc    programm.lst -o programm.cpp # C++-Quelltext, mit runtime/ref zu übersetzen
```

## Geplante Optionen

| Option | Bedeutung |
|---|---|
| `--target <name>` | Zielsystem (`pc`, `amiga`, `cpc`, `z80-test`, `m68k-test`) |
| `-o <datei>` | Ausgabedatei |
| `--feature <flag>` | CPU-Erweiterungen, z. B. `m68020-muldiv`, `z80-undoc` ([targets.md](targets.md#feature-flags-der-cpu-backends)) |
| `--emit-ast`, `--emit-hir`, `--emit-lir` | Zwischenstufen als Text ausgeben ([ir.md](ir.md)) |
| `--emit-asm` | den zusammengelinkten Assembler-Quelltext ausgeben |
| `--map <datei>` | Map: Module, Adressen, Größen, wer welches Modul angefordert hat |
| `--rom <datei>` | (cpc, umgesetzt) ROM-Abbild der CPC 6128 (32 KiB: unteres ROM und BASIC) für `-o x.sna` |
| `--seed <n>` | fester Startwert für den Zufallsgenerator (Tests) |
| `--short-errors` | unbehandelte Fehler nur als Nummer ausgeben (spart die Texttabelle) |
| `--run` | (z80-test, m68k-test, cpc, amiga) direkt im Emulator der Schwesterprojekte ausführen |

## Rückgabewerte

`0` Erfolg, `1` Fehler im Programm (Syntax, Struktur, nicht unterstützte Befehlsgruppe),
`2` Bedienfehler (unbekannte Option oder Ziel, Datei nicht lesbar).
