# Roadmap

> Pläne, keine fertigen Funktionen. Reihenfolge und Umfang sind Vorschläge.

## 0.0 – Gerüst (erledigt)

Verzeichnisstruktur, CMake mit den Schwesterprojekten (fester Commit), CI, Zielregister
(`basicc --targets`), Emulator-Harness für z80-test, cpc, m68k-test und amiga mit Smoke-Tests,
Dokumentation von Architektur, Zwischencode, Runtime, Numerik, Zielsystemen und Tests.

## 0.1 – Numerische Referenz

- `runtime/ref`: `fp_add/sub/mul/div/sqrt/cmp`, Konvertierungen Integer ↔ Float, `INT`/`TRUNC`/`FRAC`/`CINT`.
- Zahl → Text (13 Stellen) und Text → Zahl (`VAL`), korrekt gerundet.
- Zufallsgenerator.
- Generator für Konformanzfälle (`tools/`) und Unit-Tests der Referenz gegen bekannte Werte.

## 0.2 – Frontend und IR

- Lexer, Parser, Semantik für den Sprachkern ([language.md](language.md#reihenfolge-der-umsetzung)).
- HIR, Lowering, LIR, Textausgabe (`--emit-*`).
- IR-Interpreter; erste Golden-Tests unter `tests/programs/`.

## 0.3 – C++-Ziel

- C++-Backend aus der HIR, `platform/pc`, emulierter Speicher.
- Differenztest IR-Interpreter ↔ pc.

## 0.4 – MC68000 / Amiga

- Quelltext-Linker (`src/link`) mit Map-Datei und Label-Prüfung.
- LIR → 68000-Assembler, `runtime/m68k` (Integer, Strings, Ausgabe, Fließkomma-Kern), `platform/amiga`
  und `platform/m68k-test`.
- Konformanztests der Fließkomma-Module, Differenztests auf m68k-test und amiga.

## 0.5 – Z80 / Amstrad CPC

- LIR → Z80-Assembler, `runtime/z80`, `platform/cpc` (AMSDOS-Header, `.sna`) und `platform/z80-test`.
- Konformanz- und Differenztests, Größenbudgets.

## Danach

- Transzendente Funktionen, `PRINT USING`, Arrays, `ON ERROR`/`RESUME`, `INPUT`, Dateien, `memory`, `matrix`.
- Grafik (Amiga, CPC), dann GUI, Sprites, Sound, Kapitel 11 (Amiga).
- Optimierungen (Wertebereiche, Peephole).
- FPGA-Plattformen auf Basis von `z80-test`/`m68k-test`; Feature-Flags.
- CPC 6128: Bankumschaltung für Strings/Arrays.

## Offene Fragen

1. **CPC-Modelle:** nur 6128 oder auch 464/664? (Betrifft Speichergrenzen und ob Banking geplant
   werden kann; die Firmware-Sprungleiste für Text/Grafik/Dateien ist gleich.)
2. **Befehlsreferenz im Repository?** Die Referenz ist aus dem Handbuch von 1988 extrahiert. Soll sie
   unter `docs/` liegen, oder bleibt sie außerhalb (Rechtefrage des Handbuchtextes)?
3. **Zugang zum Original:** Gibt es ein lauffähiges GFA-BASIC 3.5 (z. B. im Amiga-Emulator), um die
   offenen Sprachfragen ([language.md](language.md#offene-sprachfragen)) zu klären und Golden-Ausgaben
   zu erzeugen?
4. **GFA-Float-Format** (`FLOAT{}`, `MKD$`/`CVD`): genaue Bitbelegung.
5. **Amiga-Ausgabe:** Shell-Ausgabe (`Output()`) als Standard oder von Anfang an ein eigenes Fenster
   wie im Original?
