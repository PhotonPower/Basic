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

**MC68000** (festgelegt): Ganzzahlige Argumente der Reihe nach in D0–D3, Adressen (String-
Deskriptoren, Variablen, Array-Elemente) der Reihe nach in A0, A1; ein `f64` belegt ein Registerpaar
D0:D1 bzw. D2:D3 (höherwertiger Teil vorn). Ergebnis in D0 (D0:D1 bei `f64`, A0 bei Adressen). D0–D3,
A0, A1 sind frei verwendbar, alle anderen Register bleiben erhalten (der Konformanztest prüft das).

Die Plattform `m68k-test` hinterlässt in ihren Routinen (`plat_f*`, `plat_alloc`, `plat_getc`)
absichtlich Müll in den Registern, die sie laut Konvention verändern darf. So fallen Aufrufer auf,
die sich auf diese Register verlassen; AmigaDOS tut das auch (gefunden so: der Satzindex von `GET`
in D3, den `Seek` auf dem Amiga überschrieb).

Speicherform der erzeugten Programme auf dem 68000: Jede Variable hat einen 8-Byte-Platz. Variablen,
die irgendwo `VAR`-Parameter sind, werden über einen Zeiger angesprochen (wie `brt::Var`); `LOCAL`
und Parameter sichern Wert, Zeiger und ein Merkzeichen, das die Bindung an die eigene Variable
(`@p(x)` mit `PROCEDURE p(VAR x)`) kennzeichnet. Ein Array-Platz ist der Deskriptor wie im Original
(probe19 T3): +0 Adresse des Blocks im Array-Bereich (0: nicht dimensioniert), +4 Anzahl der
Dimensionen (Wort), +6 Elementgröße (Wort; Float und String 8, Int 4, Word 2, Byte und Bool 1). Der
Block enthält die Ausdehnungen (Obergrenze − `OPTION BASE` + 1, je ein Langwort, die letzte
Dimension zuerst), dann die Elemente (erster Index am schnellsten). `LPEEK(*a())` zeigt so
4 · Dimensionen Bytes vor das erste Element. String-Elemente sind Deskriptoren nach D18.

Fehlerbehandlung (nur in Programmen mit `ON ERROR`): Jede Routine außer `DEFFN` hat im Rahmen einen
Satz „laufende Anweisung“ (24 Bytes: Satz des Aufrufers, Adresse der Anweisung, Adresse danach, SP,
A6, Adresse der Routine); `rt_err_ctx` zeigt auf den aktuellen Satz. Jede Anweisung trägt sich zu
Beginn ein, Schleifen vor Bedingung und Schritt erneut (die gehören zur Schleife). Jedes Sichern
einer Variablen (`LOCAL`, Parameter) kommt zusätzlich in eine Kette auf dem Stapel (`rt_err_log`).
`rt_error` ruft die Fehlerroutine auf; `RESUME`, `RESUME NEXT` und das Ende der Fehlerroutine holen
alle Variablen zurück, die nach Beginn der Zielanweisung gesichert wurden, und setzen SP und A6 des
Satzes. `RESUME Marke` sucht in der Kette der Aufrufer den Rahmen der Routine mit der Marke.
Ohne Fehlerroutine endet das Programm über `plat_error` der Plattform.

**Z80** ([D30](decisions.md#d30)): Alle Werte liegen im Speicher, das niederwertige Byte zuerst
(Integer 4 Bytes, Fließkomma 8 Bytes binary64). Jede Laufzeitroutine bekommt Zeiger: HL = Ergebnis,
DE = erstes, BC = zweites Argument; weitere Argumente über eine Zeigertabelle. Das Ziel darf gleich
einem Operanden sein. Fehler: `rt_error` mit der Nummer in A. IX ist der Rahmenzeiger des erzeugten
Codes und bleibt erhalten, IY ebenso (die Laufzeit benutzt es nicht). Auf dem CPC sind die Schattenregister (`EXX`,
`EX AF,AF'`) für die Firmware reserviert, weil deren Interrupt-Routine sie benutzt; die Runtime
verwendet sie dort nicht (auf `z80-test` sind sie frei).

**C (`runtime/ref`):** gewöhnliche C-Funktionen; `f64` ist eine Struktur bzw. ein `uint64_t` mit dem
Bitmuster, nie `double`.
