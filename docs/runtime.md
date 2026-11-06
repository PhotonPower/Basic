# Laufzeitbibliothek (Runtime)

Der größte Teil der Sprache (Strings, Zahlenformatierung, Mathematik, Dateien, Grafik) wird nicht zu
Inline-Code, sondern zu Aufrufen der Runtime. Ziel ist eine **feinkörnige** Bibliothek: Ein Programm
enthält nur die Routinen, die es tatsächlich benutzt. Besonders wichtig ist das für Mathematik und
Ausgabe, weil auf dem CPC nur rund 40 KB für Programm, Runtime und Daten bleiben.

## Ebenen

```
runtime/ref/    C-Referenz: definiert die Semantik aller Routinen (Spezifikation + PC-Ziel)
runtime/m68k/   dieselben Routinen als MC68000-Module
runtime/z80/    dieselben Routinen als Z80-Module
platform/*/     Startcode und Plattformprimitive (die einzige plattformabhängige Schicht)
```

- `runtime/ref` ist in portablem C99 geschrieben, rechnet nur mit Ganzzahlen und benutzt nirgends
  `float`/`double` oder `libm`. Das C++-Ziel verwendet genau diesen Code; der Compiler benutzt ihn
  zur Konstantenfaltung.
- Die Assembler-Module müssen **bitgenau** dasselbe liefern wie die Referenz. Das prüfen die
  Konformanztests ([testing.md](testing.md#konformanz-der-runtime)).
- Ein Assembler-Modul darf eine C-Funktion nicht nur nachbilden, sondern muss dieselbe Folge von
  Grundoperationen ausführen, wo es auf die Rundung ankommt (transzendente Funktionen, Zahl ↔ Text).

## Modulformat

Eine Routine (oder eine kleine Gruppe untrennbarer Routinen) je Datei. Der Kopf besteht aus
Kommentarzeilen mit `;!` (Assembler) bzw. `//!` (C), die der Linker liest:

```asm
;! module    rt_print_str
;! provides  rt_print_str
;! requires  plat_putc
;! group     print
;! clobbers  A,HL
;! doc       Gibt den String-Deskriptor in HL aus.

rt_print_str:
        ...
rt_print_str__loop:          ; interne Labels: <modul>__<name>
        ...
```

| Kopfzeile | Bedeutung |
|---|---|
| `module` | Name des Moduls, gleich dem Dateinamen ohne Endung |
| `provides` | exportierte Symbole (meist genau eines) |
| `requires` | Symbole anderer Module oder der Plattform |
| `group` | [Befehlsgruppe](language.md#befehlsgruppen) für die Prüfung gegen das Zielsystem |
| `clobbers` | Register, die das Modul verändert (Teil der Aufrufkonvention, wird getestet) |
| `section` | optional `code` (Standard), `data` oder `bss` für einzelne Blöcke, siehe unten |
| `doc` | eine Zeile Beschreibung für die generierte Modulübersicht |

**Label-Hygiene:** Jedes Label in einem Modul ist entweder in `provides` genannt oder beginnt mit
`<modul>__`. Der Linker prüft das, weil der Z80-Assembler keine lokalen Labels kennt und alle Module in
einen gemeinsamen Quelltext gelangen.

**Daten und BSS:** Auf dem 68000 benutzen Module `SECTION code,CODE`, `SECTION data,DATA` und
`SECTION bss,BSS` (der Assembler setzt Sektionen fort). Auf dem Z80 gibt es keine Sektionen; ein
Modul kennzeichnet Blöcke mit `;! section data` bzw. `;! section bss`, der Linker sortiert sie hinter
den Code. BSS-Bereiche werden nicht als `DS` ins Binary geschrieben, sondern per `EQU` hinter das
Programmende gelegt und vom Startcode gelöscht.

## Linken

Der Quelltext-Linker (`src/link`) arbeitet so:

1. Startmenge: die vom Backend referenzierten Symbole plus `plat_start` und `plat_exit`.
2. Transitive Hülle über `requires`; Fehler bei fehlenden oder doppelt bereitgestellten Symbolen.
3. Prüfung der Gruppen gegen das Zielsystem (zweite Sicherung nach der Prüfung im Frontend).
4. Zusammenfügen: Startcode, Programmcode, Runtime-Code, Daten, BSS.
5. Map-Datei: Modul, Adresse, Größe, wer es angefordert hat (`basicc --map`).

### Regeln für kleine Programme

- **Formatieren und Ausgeben sind getrennt.** `rt_fmt_int`, `rt_fmt_flt` schreiben in einen Puffer;
  nur die `rt_print_*`-Module hängen von `plat_putc` ab. `STR$` braucht so keine Plattform.
- **`PRINT` wird zerlegt** in `rt_print_str`, `rt_print_int`, `rt_print_flt`, `rt_print_tab`,
  `rt_print_comma`, `rt_print_nl`. Wer nur Strings und Integer ausgibt, bekommt keine Float-Formatierung.
- **Gemeinsame Hilfsroutinen** der Fließkomma-Arithmetik (Normalisieren, Runden und Zusammensetzen,
  Mantissen-Multiplikation) sind eigene Module. `SIN` zieht `fmul`, `fadd` und die Argumentreduktion
  herein, aber nicht `LOG`.
- **Fehlertexte sind teuer.** Routinen lösen nur Fehlernummern aus (`rt_error`). Die Texttabelle für
  `ERR$` und die Standardmeldung bei unbehandelten Fehlern sind eigene Module; auf kleinen Systemen
  kann der Standardhandler per Option nur die Nummer ausgeben.
- **Initialisierung nur bei Bedarf.** String-Heap, Zufallsgenerator und Dateikanäle werden nur
  initialisiert, wenn das Programm sie benutzt; der Compiler erzeugt die Init-Aufrufe gezielt.
- **Größenbudgets** für Referenzprogramme werden in der CI geprüft ([testing.md](testing.md#größenbudgets)).

## Namenskonventionen

| Präfix | Bedeutung | Beispiel |
|---|---|---|
| `rt_` | Runtime-Routine, vom erzeugten Code aufgerufen | `rt_concat$`, `rt_print_int` |
| `fp_` | Fließkomma-Kern | `fp_add`, `fp_mul`, `fp_div`, `fp_cmp`, `fp_from_i32` |
| `fm_` | Fließkomma-Funktionen | `fm_sin`, `fm_exp`, `fm_log` |
| `ix_` | Ganzzahl-Hilfen | `ix_mul32`, `ix_divmod32` |
| `plat_` | Plattformprimitive | `plat_putc`, `plat_exit` |

Ein `$` im Symbolnamen ist im Assembler nicht erlaubt; der Linker übersetzt `rt_concat$` in
`rt_concat_S`.

## Plattformschnittstelle

Jede Plattform stellt mindestens bereit:

| Symbol | Aufgabe |
|---|---|
| `plat_start` | Einsprung: Stapel, BSS löschen, Init, Aufruf von `main`, danach `plat_exit` |
| `plat_exit` | Programmende (Amiga: Rückkehr zu AmigaDOS; CPC: `RET` zu BASIC; Testplattformen: `HALT`/`STOP`) |
| `plat_putc` | ein Zeichen ausgeben |
| `plat_getc` | ein Zeichen lesen (blockierend), nur mit Gruppe `input` |
| `plat_poll` | Taste abfragen ohne Warten (`INKEY$`) |
| `plat_timer` | Zeitbasis für `TIMER` |

Optional, je nach Gruppen der Plattform: Dateien (`plat_fopen` …), Grafik (`plat_plot`,
`plat_line` …), Fenster und Ereignisse (nur `amiga`). Die Zuordnung steht in
[targets.md](targets.md).

## Aufrufkonventionen

> **Vorschlag**, wird mit dem jeweils ersten Backend festgelegt.

**MC68000:** Ganzzahlige Argumente in D0, D1; Zeiger in A0, A1; Ergebnis in D0. D0, D1, A0, A1 sind
frei verwendbar (wie bei AmigaOS), alle anderen Register bleiben erhalten. Ein `f64` wird als
Registerpaar D0:D1 (höherwertiger Teil in D0) übergeben und zurückgegeben; Fließkomma-Routinen mit
zwei Operanden erhalten den zweiten in D2:D3 und retten D2/D3 nicht.

**Z80:** 16-Bit-Werte in HL (und DE als zweites Argument), 8-Bit-Werte in A, 32-Bit-Werte als
Zeiger auf vier Bytes. Fließkomma-Routinen arbeiten auf Speicher: HL = Ziel, DE = erster Operand,
BC = zweiter Operand (Zeiger auf je 8 Bytes, Ziel darf gleich einem Operanden sein). IX ist der
Rahmenzeiger des erzeugten Codes und bleibt erhalten. Auf dem CPC sind die Schattenregister (`EXX`,
`EX AF,AF'`) für die Firmware reserviert, weil deren Interrupt-Routine sie benutzt; die Runtime
verwendet sie dort nicht (auf `z80-test` sind sie frei).

**C (`runtime/ref`):** gewöhnliche C-Funktionen; `f64` ist eine Struktur bzw. ein `uint64_t` mit dem
Bitmuster, nie `double`.
