# Zielsysteme

Ein Zielsystem ist **CPU-Backend × Plattform** ([architecture.md](architecture.md#zielsystem--cpu--plattform)).
`basicc --targets` zeigt die Liste mit den Befehlsgruppen; sie ist in `src/target.cpp` hinterlegt
und wird von `tests/test_targets.cpp` geprüft.

| Name | CPU | Plattform | Ausgabe | Zweck |
|---|---|---|---|---|
| `pc` | Host (C++) | `platform/pc` | C++-Quelltext + `runtime/ref` | Referenz, Tests, Entwicklung |
| `amiga` | MC68000 | `platform/amiga` | AmigaDOS-Programm (Hunk) | Commodore Amiga, Heimat von GFA-BASIC |
| `cpc` | Z80 | `platform/cpc` | Binary mit AMSDOS-Header, optional `.sna` | Amstrad CPC |
| `z80-test` | Z80 | `platform/z80-test` | Raw-Binary | Emulator-Tests, Vorlage für FPGA-Boards |
| `m68k-test` | MC68000 | `platform/m68k-test` | Raw-Binary / S-Record | Emulator-Tests, Vorlage für FPGA-Boards |

Welche Befehlsgruppen jedes Ziel unterstützen soll, steht in [language.md](language.md#unterstützung-je-zielsystem).

## pc (C++)

Das C++-Backend erzeugt aus der HIR lesbaren C++-Code, der mit `runtime/ref` und `platform/pc`
übersetzt wird. Fließkomma läuft über die Referenzimplementierung, nicht über die FPU.

Damit `PEEK`/`POKE`, `VARPTR`, `*x`, `BYTE{}` … und `ABSOLUTE` dieselben Ergebnisse liefern wie auf
dem Amiga, liegen alle BASIC-Variablen in einem **emulierten Big-Endian-Speicher** (ein Byte-Array
mit 32-Bit-Adressen) mit demselben Deskriptor-Layout ([decisions.md](decisions.md#d6)). Adressen
außerhalb dieses Speichers lösen einen Fehler aus.

## amiga (MC68000)

- Ausgabe: AmigaDOS-Hunk-Programm, erzeugt über `SECTION` mit dem Assembler von PhotonPower/MC68000.
- Big-Endian, 32-Bit-Adressen und Deskriptor-Layout entsprechen dem Original; `PEEK`/`POKE` usw.
  greifen direkt auf den Speicher zu.
- Kapitel 11 (Exec, DOS, Graphics, Intuition, Layers, DiskFont) wird zu Bibliotheksaufrufen über
  Library-Basis und LVO-Offset; Screens, Fenster, Menüs, Requester und BOBs werden auf Intuition und
  Graphics abgebildet.
- Fließkomma über die eigene Runtime (nicht `mathieeedoubbas.library`), damit die Ergebnisse
  bitgleich zu den anderen Zielen sind.
- Grundbefehlssatz: dokumentierter MC68000. Erweiterungen (z. B. `MULS.L`/`DIVS.L` des 68020) nur
  über Feature-Flags.
- Tests: textbasierte Programme laufen in der OS-Emulation des Schwesterprojekts
  (`dos.library` `Output`/`Write` → Konsole); Grafik und Fenster sind dort nicht emuliert und müssen
  in einem vollständigen Emulator (z. B. FS-UAE) oder auf echter Hardware geprüft werden.

## cpc (Z80)

- Ausgabe: Binary mit AMSDOS-Header (Laden mit `RUN"name"` bzw. `MEMORY`/`LOAD`/`CALL`); für
  Emulatoren zusätzlich ein `.sna`-Snapshot über den Assembler des Schwesterprojekts. `.dsk`-Images
  erzeugen externe Werkzeuge (z. B. iDSK).
- Speicher: Programm, Runtime, Variablen und Strings teilen sich grob 40 KB zwischen dem unteren
  Systembereich und dem Firmware-Bereich unter `&C000`. Ladeadresse und Grenzen legt
  `platform/cpc` fest. Die 128 KB des 6128 (Bankumschaltung `&4000–&7FFF`) sind ein späterer Ausbau.
- Plattformprimitive über die Firmware-Sprungleiste: `TXT OUTPUT` (`&BB5A`) für `plat_putc`,
  `KM WAIT KEY`/`KM READ KEY` für Eingaben, die `GRA`-Routinen für Grafik, `CAS`-Routinen (AMSDOS)
  für Dateien.
- Einschränkungen:
  - **Dateien:** AMSDOS erlaubt nur je eine Eingabe- und eine Ausgabedatei und keinen wahlfreien
    Zugriff. Gruppe `file-seq` ja (mit dieser Grenze, Fehler bei einem dritten Kanal),
    `file-random` nein.
  - **Grafik:** `PLOT`, `DRAW`, `LINE`, `BOX`, `CIRCLE`, `COLOR`, `CLS`, `TEXT` werden auf
    Firmware-Grafik abgebildet (Koordinaten skalieren, Farben auf die Palette des Modus);
    Screens, Fenster, Menüs, Sprites, BOBs und Kapitel 11 gibt es nicht.
  - **Byte-Reihenfolge:** Der Z80 ist Little-Endian. `MKI$`/`CVI`, `MKL$`/`CVL` usw. sind trotzdem
    immer Big-Endian (Datenformat). `PEEK`/`POKE`/`LONG{}` greifen auf den nativen Speicher zu, weil
    solche Programme ohnehin maschinenspezifische Adressen verwenden.
  - **Register:** Die Schattenregister gehören der Firmware ([runtime.md](runtime.md#aufrufkonventionen)).
- Firmware 464 vs. 664/6128: Die Sprungleiste für Text, Tastatur, Grafik und Kassette/AMSDOS ist auf
  allen Modellen gleich. Die Mathe-Sprungleiste der Firmware (die sich unterscheidet) wird nicht
  benutzt. Welche Modelle unterstützt werden, ist noch offen ([roadmap.md](roadmap.md#offene-fragen)).
- Tests: In der Harness werden Firmware-Aufrufe als Stubs emuliert (`TXT OUTPUT` → Konsole); ein
  nicht emulierter Aufruf bricht den Lauf mit Meldung ab. Das Schwesterprojekt kann außerdem die echte
  6128-Firmware booten und den Bildschirm rendern, was für Grafiktests genutzt werden kann.

## z80-test und m68k-test

Nackte Prozessoren ohne Betriebssystem, gedacht für automatische Tests und als **Vorlage für
FPGA-Implementierungen**:

| | z80-test | m68k-test |
|---|---|---|
| Ladeadresse/Einsprung | `&4000` | `$1000` |
| Stapel | `&FFF0` abwärts | `$80000` abwärts |
| `plat_putc` | `OUT (&01),A` | `MOVE.B D0,$00FF0000` |
| `plat_exit` | `DI` + `HALT` | `STOP #$2700` |

Ein FPGA-Board bekommt ein eigenes Verzeichnis unter `platform/` mit eigenem Startcode, eigener
Speicheraufteilung und eigenen Primitiven (z. B. `plat_putc` auf einen UART). Mathematik, Strings und
Formatierung werden unverändert übernommen. Über die serielle Ausgabe laufen dieselben
Konformanz- und Programmtests wie im Emulator.

## Feature-Flags der CPU-Backends

Standard ist der dokumentierte Befehlssatz. FPGA-Kerne unterscheiden sich (zyklengenaue Nachbauten,
Kerne mit 68020-Befehlen, Z80-Kerne mit oder ohne undokumentierte Befehle), deshalb schaltet das
Backend Erweiterungen nur gezielt ein:

| Flag | Wirkung |
|---|---|
| `m68020-muldiv` | 32-Bit-`MULS.L`/`DIVS.L` statt Runtime-Aufruf |
| `z80-undoc` | undokumentierte Befehle (`IXH`/`IXL` …) im erzeugten Code |
| `fpu=<name>` | eigene Fließkomma-Hardware ersetzt einzelne `fp_`-Module (muss die Konformanztests bestehen) |

Hinweis: Der Assembler des Z80-Schwesterprojekts akzeptiert derzeit nur dokumentierte Befehle, der des
MC68000-Projekts nur den 68000-Befehlssatz ([toolchain.md](toolchain.md)).
