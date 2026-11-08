# Changelog

Alle nennenswerten Änderungen an diesem Projekt werden hier festgehalten.
Das Format orientiert sich an [Keep a Changelog](https://keepachangelog.com/de/1.1.0/),
die Versionierung an [Semantic Versioning](https://semver.org/lang/de/).

## [Unreleased]

### Geändert
- `gfa_run.py`: Tastenfolge am Original geprüft und korrigiert (Rückfrage bei *New*, vorbelegtes
  Dateifeld im Merge-Requester); Tastenwiederholung `key:<name>*<n>`; Standard-Tastaturbelegung `us`.
- Fehlerprozedur der Prüfprogramme schließt den Ausgabekanal und öffnet ihn neu, weil er nach einem
  abgefangenen Fehler im Original unbrauchbar ist.

### Hinzugefügt
- MC68000, zweiter Teil: LIR mit Lowering (`--emit-lir`), 68000-Backend
  (`basicc --target m68k-test`), Runtime-Module für `PRINT`, Ganzzahlhilfen, `FOR`, Potenz; die
  Module sind in `basicc` eingebettet. Das erste Golden-Programm (`numbers`) läuft übersetzt im
  Emulator mit derselben Ausgabe wie der Interpreter; `test_m68k_programs` vergleicht.
- MC68000: Strings nach D18 (Deskriptoren, Rückverweise, Zusammenschieben, Zwischenergebnisse je
  Anweisung) mit den wichtigsten String-Funktionen; `control` läuft im Emulator.
- MC68000: Bit- und Ganzzahlfunktionen (`ADD`/`SUB`/`MUL`/`DIV`/`MOD`, `SHL`/`SHR`/`ROL`/`ROR` in
  8/16/32 Bit, `BTST`/`BCLR`/`BSET`/`BCHG`, `SWAP`, `WORD`, `CARD`, `BYTE`) in `rt_bits`, im
  Konformanztest gegen die Semantik von `platform/pc` geprüft; `arith` läuft im Emulator.
- MC68000: Arrays (bis 3 Dimensionen, alle Typen, `DIM?`, `ERASE`, `OPTION BASE`), `VAR`-Parameter
  (auch ganze Arrays), `SWAP`, `CLR`; `procs` läuft im Emulator. Neues Testprogramm `arrays`.
- MC68000: `VAL`/`VAL?` bitgenau (Konformanztest mit exakten Mittelpunkten und über 800 Ziffern),
  `DATA`/`READ`/`RESTORE`; `dataread` läuft im Emulator.
- MC68000: `ON ERROR GOSUB`, `RESUME` (wiederholen, `NEXT`, Marke), `ERR`, `ERR$`; Sätze der
  laufenden Anweisung und eine Kette der gesicherten Variablen, nur in Programmen mit `ON ERROR`.
  Die Plattform stellt jetzt `plat_error` statt `rt_error`. `errors` läuft im Emulator; neues
  Testprogramm `errors2` für Grenzfälle.
- MC68000: `PRINT USING`, `MODE`, `STR$(x,n)`, `STR$(x,y,z)`, `MID$=`, `LSET`, `RSET`;
  Konformanztest mit zufälligen Formaten in allen vier Modi. Alle Testprogramme laufen jetzt im
  Emulator mit derselben Ausgabe wie Interpreter und Original.
- Fehlerbehandlung nach dem Original (probe11, D19): `RESUME NEXT` nach einem Fehler in einer
  Kopfzeile (`WHILE`, `IF`, `ELSE IF`, `DO WHILE`/`UNTIL`, `FOR`) geht in den Rumpf, `RESUME`
  wiederholt nur die Kopfzeile; in Interpreter, C++-Ziel und 68000. Neue Testprogramme `resume`
  (am Original gleich) und `errnest`; `arrays` am Original gleich.
- `RESUME` setzt `ERR` auf 0 (probe12 T1); damit ist `errors2` am Original gleich.
- `gfa_run.py`: Startmenü der Diskette nach einem Kaltstart (`boot` in `keys.json`), längere
  Wartezeiten für den Merge-Dialog und das Einlesen.
- Z80 (Meilenstein 0.5, D30): Fließkomma-Kern in `runtime/z80` (Addition, Subtraktion,
  Multiplikation, Division, Vergleich, Umwandlungen, Wurzel, `TRUNC`/`INT`/`FRAC`/`CINT`, `==`,
  `ABS`), bitgenau zur C-Referenz; Backend `src/backend/z80`, Plattform z80-test, 32-Bit-Integer
  (`zi`, `rt_int`), `PRINT` von Strings und Integern; `basicc --target z80-test`; Testtreiber
  `z80_programs` und Testprogramm `intcore`; Fließkomma-Ausgabe (`ix_bn` mit 1280 Bit,
  `rt_flt_dec`, `rt_fmt_flt`, `rt_print_flt`, `DEFNUM`), Text für Text gleich der Referenz;
  Strings nach D18 (`rt_str`, `rt_strfn`, Halde der Plattform), Testprogramm `strcore`; Felder
  (`rt_arr`, Feldbereich der Plattform), Testprogramm `arrcore`; Prozeduren und Funktionen mit
  Parametern, `LOCAL`, `VAR`-Parameter; Linker-Label `prg_end`; `ON ERROR`/`RESUME`/`ERR`/`ERR$`
  (`rt_err`, `rt_onerr`, `rt_resume`, `rt_errtext`); `FOR` mit Integer-Variable (`rt_for`), `ROUND`,
  `VAL`/`VAL?` (`rt_val` mit 40 Ziffern, D30), `DATA`/`READ`/`RESTORE`; transzendente Funktionen
  (`rt_math`) und `^` (`rt_power`), bitgleich mit der Referenz; Tastatur (`rt_kbd`, Eingabe-Ports von
  z80-test), `WRITE`; Bitfunktionen (`BTST` … `ROR`, `SWAP`, `WORD`, `CARD`, `BYTE`), `SELECT` mit Strings,
  `MKI$`/`MKL$`/`MKS$`/`MKD$` und `CVI`/`CVL`/`CVS`/`CVD` (bitgleich mit der Referenz), `HEX$`/`BIN$`/`OCT$`;
  `PRINT USING`, `MODE`, `STR$(x,y,z)`, `STR$(x,n)` (bitgleich mit der Referenz), `LSET`/`RSET`/`MID$()=`,
  `RESERVE`/`FRE`, `TIMER` auf z80-test; Matrizen (`MAT` ohne Dateien, bitgleich mit der Referenz);
  kleinerer Code (Konstanten in geteilten Zellen statt im Rahmen, Plätze nach Lebensdauer geteilt,
  `rt_ix*`, `rt_err_line`, `rt_str_relx`, direkte Argumente; etwa ein Drittel weniger);
  Plattform `cpc`: `basicc --target cpc` schreibt eine Binärdatei mit AMSDOS-Header (Ladeadresse
  `&0100`), Ausgabe über `TXT OUTPUT`, Tastatur mit Echo über `KM WAIT CHAR`/`KM READ CHAR`, `TIMER`
  über `KL TIME PLEASE`; alle Testprogramme laufen zusätzlich auf cpc; Cursor und Bildschirm
  (`CLS`, `LOCATE`, `PRINT AT`, `HTAB`/`VTAB`, `CRSCOL`/`CRSLIN`) über `plat_locate`/`plat_cls`,
  Testprogramm `screen` (wie `console`, ohne Dateien); Aufrufhilfe
  `Z80Routines`, Konformanztest `conformance_z80`; Linker-Anordnung `PlainZ80`.
- Maus und Joystick (D29): `MOUSEX`, `MOUSEY`, `MOUSEK`, `MOUSE`, `STICK()`, `STRIG()` statt Variablen
  und Feldern; auf dem Amiga aus der Window-Struktur und der Hardware. Testprogramm `mouse`.
- AmigaOS-Bibliotheksfunktionen beim Namen (D28): Tabelle mit LVO und Registern, `rt_lib` und
  `plat_libcall`; `WINDOW()`, `SCREEN()`, `DIR$()`. Testprogramm `libcall`. Fehler behoben:
  `WINDOW()`/`SCREEN()` lieferten beim ersten Aufruf 0 (Register von `plat_gfx__init` überschrieben).
  Alle drei Anwenderprogramme übersetzen sich jetzt für den Amiga und starten dort.
- `PRINT` im Grafikfenster auf dem Amiga (probe26, probe27): Zellen 8 × 8, `LOCATE`/`PRINT AT`/`CLS`,
  Rollen, Stift 1 auf 0; `OPENS` mit Bildschirmart, `OPENW` mit Flags (Rahmenkoordinaten) und auf
  dem zuletzt geöffneten Screen; `CIRCLE` nach dem Seitenverhältnis des Screens (auch PC-Modell).
  SCENERY_GENERATOR zeigt damit seinen Startbildschirm.
- Verzeichnisbefehle auf dem 68000: `MKDIR`, `RMDIR`, `CHDIR`, `FILES`/`DIR [TO]` (`rt_dir`); Amiga
  über dos.library, m68k-test mit Verzeichnissen in der RAM-Disk. `dirs` läuft auf m68k-test und
  unter Kickstart 1.2.
- Fehler behoben: `RESUME` in einem Programm ohne `ON ERROR` (auskommentiertes `ON ERROR GOSUB`)
  ließ sich auf dem 68000 nicht linken (`prg_rlab` fehlte). Testprogramm `resumenoerr`.
- Requester und Menüs auf dem Amiga (D26 Stufe 2b): `plat_gui` mit Intuition-Fenstern für `ALERT`
  (bis 3 Knöpfe), `FILESELECT`, `FORM INPUT` und einer Menüleiste für `MENU`/`ON MENU`; unter
  Kickstart 1.2 mit der Tastatur geprüft. `amiga_run.py --after` für Tastenschritte.
- Requester und Menüs auf dem 68000 (D26 Stufe 2a): `rt_gui` mit Port-Protokoll auf m68k-test;
  `gui1` läuft dort. `ON MENU GOSUB` als direkter Aufruf hinter `ON MENU`.
- probe25 am Original: `RESERVE`/`FRE` (D27), Verzeichnisbefehle, `FILES`-Format, `SOUND`, Polygone.
  Neu: `POLYLINE`/`POLYFILL` mit `OFFSET` in allen Ausführungen (auch Amiga: `AreaMove`/`AreaDraw`);
  `RESERVE`, `FRE`, `CLEAR`, `SOUND`, `WAVE` auch auf dem 68000; `MKDIR`, `RMDIR`, `CHDIR`,
  `FILES`/`DIR [TO]` im Interpreter und C++-Ziel; `EXIST` gilt für Verzeichnisse. Testprogramme
  `poly1`, `dirs`, `reserve`.
- Requester und Menüs Stufe 1 (D26): `ALERT`, `FILESELECT`, `FORM INPUT`, `MENU m$()`/`MENU x,y`/
  `MENU x,s$`/`MENU KILL`, `ON MENU [t]`, `ON MENU GOSUB`, `MENU(x)` im Interpreter und C++-Ziel als
  Textdialoge auf der Tastatureingabe (`platform/pc/rt_gui`). Testprogramm `gui1` mit Eingabe.
- Grafik auf dem Amiga (D25 Stufe 2): eigener Screen und Fenster, Zeichnen über die ROM-Bibliotheken;
  `gfx1` gibt unter Kickstart 1.2 genau die am Original bestätigten Werte aus. `PAUSE`/`DELAY` warten
  auf dem Amiga (dos `Delay`). `amiga_run.py` startet die Programme nacheinander.
- Grafik auf dem 68000: `rt_gfx` sammelt die Argumente, `plat_gfx` zeichnet; m68k-test über Ports,
  die der Testtreiber mit `brt::Gfx` ausführt (gleiche Zusammenfassung wie Interpreter und C++-Ziel).
  Amiga folgt (Stufe 2), bis dahin zeichnet es nichts.
- Grafik Stufe 1 (D25): Screens, Fenster, Stift, Zeichenmodi und Grundformen im Interpreter und
  C++-Ziel (`platform/pc/rt_gfx`), Rasterregeln am Original gemessen (probe22–24) und in `test_gfx`
  Pixel für Pixel verglichen. `basicc --run … --gfx bild` bzw. `BASIC_GFX_OUT` schreiben Bild (PPM)
  und Zusammenfassung; Testprogramme vergleichen sie mit `<name>.gfx`. Testprogramm `gfx1`.
- Frontend nach echten Amiga-Listings: Latin-1-Buchstaben in Namen, Prozedurnamen mit Ziffern am
  Anfang, zugeklappte Prozeduren (`> PROCEDURE`), klein geschriebene Prozeduraufrufe mit dem Namen
  eines Schlüsselworts, Builtin-Namen ohne `(` als Variablen, getrennte Namen für Variablen und
  Prozeduren; `VSYNC` wird übergangen. Testprogramm `umlaut`. Werkzeug `tools/gfa/gfa2lst.py`
  (`.GFA` → `.LST` mit dem Original).
- Behoben (probe12 T2 am Original): Eine Fehlerroutine, die ohne `RESUME` endet, setzte das Programm
  nach der Anweisung fort; jetzt endet es mit der Fehlermeldung, in allen Ausführungen. Die
  Testtreiber kennen dafür `<name>.err` (erwarteter Fehler am Ende); Testprogramm `errret`.
- probe15 am Original: `INPUT`-Verhalten aus D20 bestätigt.
- Textausgabe nach probe20/probe21 (D24): `POS`, `CRSCOL`, `CRSLIN`, `PRINT AT`, `LOCATE`, `HTAB`, `VTAB`,
  `CLS` (ANSI-Folgen), `TIMER` (Amiga über `DateStamp`, unter Kickstart 1.2 geprüft), `RINSTR` in allen
  Ausführungen. Behoben: Das Komma in `PRINT` zählt seine Zonen ab dem Anfang der Anweisung, nicht ab
  der Spalte; `CRSCOL`, `CRSLIN` und `LPOS` galten bisher als Variablen. Testprogramm `console`.
- `ARRPTR`/`*x`, `SWAP *p,a()`, `TYPE(p)` in allen Ausführungen (probe19 am Original). Die Felder des
  68000 haben jetzt die Speicherform des Originals (Deskriptor: Blockadresse und Dimensionen; im
  Block die Ausdehnungen vor den Elementen); Interpreter und C++-Ziel bilden dieselben Bytes nach.
  `MALLOC` verlangt zwei Argumente wie das Original. Testprogramm `ptr` mit der Ausgabe des Originals.
- `MAT INPUT #` in allen Ausführungen (probe18 am Original); auf dem 68000 eigene Module für
  `MAT READ`/`INPUT`/`PRINT`. Behoben: `INPUT #` meldete bei einem CR vor dem Zeilenende Fehler 35;
  ein CR beendet jetzt einen Wert, ein LF direkt danach gehört dazu (wie am Original). Testprogramm
  `matinput` mit der Ausgabe des Originals.
- Gruppe `matrix` auf dem 68000 (`runtime/m68k/rt_mat.s`), im Konformanztest bitgleich zu
  `rt_mat.c`; Testprogramm `matfile` (`MAT PRINT #`). Behoben: Ein Programm mit `MAT READ`, aber ohne
  `READ`/`RESTORE`, bekam keine `DATA`-Tabelle.
- PhotonPower/MC68000 auf `f41f822` festgelegt: Der Verlauf dort wurde umgeschrieben, `0074b8b`
  gibt es nicht mehr; inhaltlich gleich bis auf entfernte Abfalldateien im Wurzelverzeichnis.
- Gruppe `matrix` (`MAT BASE CLR SET ONE READ PRINT CPY XCPY TRANS ADD SUB MUL NORM DET QDET RANG
  INV`) im Interpreter und C++-Ziel (`runtime/ref/rt_mat.c`, D23); probe17 am Original, Ausgabe Byte
  für Byte gleich. Testprogramm `matrix`.
- MC68000: ganzzahlige Zwischenwerte in D4–D7 (D22), Rechnen direkt im Zielregister, `ASL` für
  `* 2^k`. Testprogramm `regs`.
- MC68000: Ketten aus `+ - *` über Ganzzahlen rechnen in 32 Bit mit Rücksprung auf den Float-Weg bei
  Überlauf (D16, Nachtrag); neue LIR-Art `OpOvf`. Grenzfälle in `intfast`.
- Gruppe `memory`, Stufe 1 (D21): `VARPTR`/`V:`, `PEEK`/`DPEEK`/`LPEEK`, `POKE`/`DPOKE`/`LPOKE`,
  `MALLOC`/`MFREE`, `BMOVE`, `BYTE{}` `CARD{}` `INT{}`/`WORD{}` `LONG{}`/`{}` `FLOAT{}` `SINGLE{}`
  `DOUBLE{}` `CHAR{}` (lesen und schreiben) in allen Ausführungen; Interpreter und C++-Ziel mit Fenstern in einem
  nachgebildeten Adressraum (Amiga-Layout), 68000 auf echtem Speicher. Testprogramm `memory`.
- `MKI$`, `MKL$`, `MKS$`, `MKD$`, `CVI`, `CVL`, `CVS`, `CVD` (`runtime/ref/rt_mk.c`, GFA-8-Byte-Format
  mit den Bytes des Originals, IEEE single nach D11) in allen Ausführungen; 68000 im Konformanztest
  bitgleich. Testprogramm `binary`.
- MC68000-Backend: Konstanten als Direktwerte, Vergleich und Sprung zusammengezogen, Laden und
  Speichern einer `%`-Variablen direkt neben ihrer Verwendung gefaltet (Zählschleife nochmals
  ein Viertel weniger Befehle).
- MC68000: Ganzzahl-Abkürzungen (D16, Nachtrag): `a%+b%`, `-`, `*` mit Überlaufprüfung statt über
  Float, Vergleiche zweier Ganzzahlen als `CMP`, `FOR` mit `%`-Variable in 32 Bit. Eine einfache
  Zählschleife braucht 23-mal weniger Befehle, die Testprogramme bis zu 3-mal weniger. Testprogramm
  `intfast` prüft die Überlaufgrenzen gegen den Interpreter.
- Wahlfreie Dateien nach dem Original (probe16): `OPEN "R"`/`"U"`, `FIELD … AS`, `GET #`, `PUT #`,
  `RECORD`, `SEEK`, `RELSEEK`, `LOC`, `INP(#n)`, `OUT #n`, `NAME … AS`; Interpreter, C++-Ziel und
  68000 (Kanäle lesen und schreiben, Plattform: Positionieren, Umbenennen); unter Kickstart 1.2 im
  FS-UAE mit der erwarteten Ausgabe. Testprogramm `random`. Die Plattform `m68k-test` zerstört jetzt
  absichtlich die Register, die ihre Routinen verändern dürfen (fand einen Fehler, der nur auf dem
  Amiga auftrat).
- Tastatureingabe `INPUT`, `LINE INPUT` (mit Aufforderung `"text";`/`"text",`), `INKEY$`,
  `INPUT$(n)` und `WRITE ...;` in Interpreter, C++-Ziel und 68000 (`rt_kbd`; Plattform: Eingabeport
  bei `m68k-test`, `Read(Input())` beim Amiga); Semantik nach der Befehlsreferenz (D20).
  Testprogramme können eine Eingabedatei `name.in` haben; `gfa_run.py --after` tippt nach dem Start.
- Sequentielle Dateien nach dem Original (probe14): `OPEN` (`I`, `O`, `A`), `CLOSE`, `PRINT #`,
  `WRITE #`/`WRITE`, `INPUT #`, `LINE INPUT #`, `INPUT$(n,#c)`, `EOF`, `LOF`, `EXIST`, `KILL`, mit
  den Fehlern des Originals (21–26, −51, −54). Interpreter, C++-Ziel und 68000 (`rt_file`;
  Plattform: RAM-Platte für `m68k-test`, `dos.library` für den Amiga, dort im FS-UAE mit der
  Ausgabe des Originals); `""` in Strings. Testprogramm `files`.
- MC68000: Registerplätze im Rahmen werden nach Lebensdauer geteilt (auch über Schleifen und die
  Rücksprünge von `RESUME`); der größte Rahmen der Testprogramme schrumpft von mehreren KB auf
  104 Bytes (`main` von `arrays`; `main` von `arith` von 2880 auf 40 Bytes).
- MC68000: Zahlenausgabe 2,6-mal schneller (`rt_flt_dec` rechnet mit so vielen Langwörtern, wie
  Exponent und Stellenzahl verlangen, statt immer mit 40); im Mittel 13 700 statt 35 100 Befehle je
  Zahl, bitgleich. Die Testprogramme brauchen 2- bis 4-mal weniger Befehle.
- Transzendente Funktionen `EXP`, `LOG`, `LOG10`, `SIN`, `COS`, `TAN`, `ATN`, `ASIN`, `ACOS`,
  `SINQ`, `COSQ`, `DEG`, `RAD` und `x^y` mit gebrochenem Exponenten: deterministisch aus den
  Grundoperationen (`runtime/ref/rt_math.c`, höchstens 3 ulp von der libm), im Interpreter, im
  C++-Ziel und auf dem 68000 (erzeugt von `tools/gen/gen_rt_math.py`, im Konformanztest bitgleich).
  Ränder und Fehler wie im Original (probe13, neuer Fehler 67 „asin/acos error“); Testprogramm
  `math`.
- Amiga-Ziel: `basicc --target amiga` schreibt ein AmigaDOS-Programm (Hunk). `platform/amiga`:
  `dos.library`, gepufferte Ausgabe über `Write()`, eigener Stapel von 64 KiB, Speicher über
  `AllocMem`, Ende aus jeder Tiefe. String-Heap und Array-Bereich kommen jetzt von `plat_alloc`
  der Plattform. Alle Testprogramme laufen in der OS-Emulation und unter Kickstart 1.2 im FS-UAE
  (neues Werkzeug `tools/gfa/amiga_run.py`) mit der Ausgabe des Originals.
- Testprogramm `strheap` für den String-Heap; dabei behoben: `rt_str_set_ret` erwartete den
  Deskriptor in A1 statt A0, das Ergebnis von String-Funktionen ging verloren (Fehler 8).
- MC68000, erster Teil: Quelltext-Linker `src/link` (Modulköpfe, Hülle über `requires`,
  Label-Hygiene), Plattform `m68k-test`, Konformanz-Harness (Routinen direkt im Emulator gegen
  `runtime/ref`), Fließkomma-Kern in 68000-Assembler: Grundrechenarten, `fp_sqrt`, Vergleiche, `==`,
  Konvertierungen, `TRUNC`/`INT`/`FRAC`/`CINT`/`ROUND`, Zufallsgenerator – bitgenau gegen die
  Referenz bei jeweils 50 000 Zufallsfällen; Zahlenausgabe `rt_fmt_flt` mit Big-Integer-Arithmetik
  (`ix_bn`, `rt_flt_dec`), ebenfalls bitgenau.
- C++-Ziel `pc` (`basicc --target pc`): lesbarer C++17-Code aus der HIR; gemeinsame
  Laufzeitschicht `platform/pc` für Interpreter und erzeugte Programme; Differenztests `pc_<name>`
  übersetzen alle Golden-Tests und vergleichen mit den Ausgaben des Originals.
- Frontend, erster Teil: Lexer und Parser für den Sprachkern (Ausdrücke mit der am Original
  ermittelten Rangfolge, Zuweisung, `PRINT`, alle Schleifen, `IF`/`SELECT`, Prozeduren und
  Funktionen, `DATA`/`READ`, Labels, Fehlerbehandlung, allgemeine Befehle), `basicc --emit-ast`;
  Tests lesen alle Prüfprogramme ohne Fehler.
- Semantik und HIR: Blöcke und Strukturprüfung mit Meldungen wie im Editor (`ENDIF missing`,
  `NEXT without FOR` …), Typen aus Postfix und `DEFxxx`, Prozeduren, Funktionen, `DEFFN`,
  `VAR`-Parameter, Labels und `GOTO`-Prüfung, `DATA`, `SELECT` mit gepackten Strings, eingefügte
  Konvertierungen; `basicc --emit-hir`. Entscheidungen D15–D17.
- Referenz-Interpreter auf der HIR (`basicc --run`): alle Numerik über `runtime/ref`, dynamische
  Bindung mit Sichern/Wiederherstellen, `ON ERROR GOSUB` mit `RESUME`/`RESUME NEXT`/`RESUME marke`,
  `PRINT` mit Zonen, `TAB`, `SPC`, `USING`, `DATA`/`READ`, Fehlernummern und `ERR$`-Texte vom
  Original (probe9). Golden-Tests `tests/programs` (7 Programme), Ausgaben am Original erzeugt; der
  Interpreter stimmt Byte für Byte überein.
- `VAR` gilt für alle folgenden Parameter und ist eine echte Referenz (probe10); `STR$` beachtet
  `DEFNUM`.
- `gfa_run.py`: Programme ohne `datei$`-Zeile werden umgeschrieben (`PRINT` in eine Datei, Kanal in
  Fehlerroutinen neu öffnen); wartet, bis FS-UAE den Fokus hat; erstes Return doppelt; prüft vor
  jedem Tastendruck, ob FS-UAE noch vorn ist, und bricht sonst ab (Rückgabewert 3), statt in ein
  anderes Fenster zu tippen; kürzere Pausen.
- Numerische Referenz `runtime/ref` (C99, nur Ganzzahlarithmetik): binary64-Grundoperationen
  korrekt gerundet, `==`, Umwandlungen und Rundungsfunktionen, `ROUND`, Zahl → Text mit 13 Stellen und
  `DEFNUM`, `VAL`/`VAL?` korrekt gerundet, Zufallsgenerator; Tests gegen die IEEE-Arithmetik des
  Rechners, `strtod`/`printf` und die Ergebnisse am Original (`tests/test_runtime_ref.cpp`).
- `PRINT USING` (Zahlen- und Stringfelder, `MODE`, Wiederholung des Formats), `STR$(x,n)` und
  `STR$(x,y,z)` in `runtime/ref`; Festlegungen für vom Original nicht gezeigte Fälle in D13,
  nach probe7/probe8 am Original korrigiert (D14): Rest des Formats als Text, `?` für Werte ohne
  passendes Feld, `-0.00`, Exponentlage und -breite, Minus im Exponentformat.
- probe6 am Original ausgewertet: `VAL` mit Präfix (32 Bit, kein Vorzeichen davor), Gleichstand bei
  der Ausgabe von 0 weg, `DEFNUM` mindestens 2 Stellen, `ROUND` rundet aufwärts. Entscheidung D13.
- Abgleich mit dem Original: `tools/gfa/gfa_run.py` führt `.lst`-Programme im echten GFA-BASIC 3.5
  unter FS-UAE (macOS) aus (gemeinsames Verzeichnis als `DH0:`, Tastendrücke über `osascript`,
  Bildschirmfoto bei Zeitüberschreitung); Beispielkonfiguration, Tastenfolge `keys.json`, Python-Tests.
- Erstes Prüfprogramm `tests/probes/probe1.lst` (54 Tests zu Überlauf, Rundung, `FOR`, Zahlenausgabe,
  `==`, Float-Format, Fehlernummern).
- probe1 am Original ausgewertet (`tests/probes/results/probe1.txt`): Ganzzahl-Überlauf und
  Rundungsregeln, `FOR`-Semantik, Ausgabeformat, Fehlernummern und das GFA-8-Byte-Float-Format
  (48 Bit Mantisse) in `docs/numerics.md` und `docs/language.md` eingetragen; neue offene Frage zu D4
  in `docs/roadmap.md`. Folgefragen als `tests/probes/probe2.lst`.
- probe2 am Original ausgewertet (`tests/probes/results/probe2.txt`): Ausdrücke ohne 32-Bit-Überlauf,
  Byte-Überlauf (Fehler 3), `FOR` mit negativer Schrittweite und am Bereichsende, 16-Bit-Exponent,
  ungenaue Konstante `PI`, Schwellen der Exponentdarstellung, `DEFNUM`, fehlerhaftes `MKS$`.
  Folgefragen als `tests/probes/probe3.lst`.
- probe3 am Original ausgewertet (`tests/probes/results/probe3.txt`): Regel von `==` bestätigt
  (28 Bit, Hälfte aufwärts statt gerade Rundung), Fehler 1 „overflow“, Unterlauf zu 0,
  `VAL` außerhalb des Bereichs ergibt 0.
- probe4 am Original ausgewertet (`tests/probes/results/probe4.txt`): `SELECT` packt Strings
  Big-Endian, `CONT`, `PRINT USING` (Rundung über die Dezimaldarstellung), `MODE` vertauscht `.`/`,`
  auch im Format, `STR$(x,y,z)`, Operator-Rangfolge, String-Funktionen, Fehler 10.
- probe5 am Original ausgewertet (`tests/probes/results/probe5.txt`): vollständige Rangfolge der
  Operatoren (`AND`/`OR`/`XOR` gleichrangig, `NOT` am schwächsten), `STR$(x,n)` behält die linken
  Zeichen, Eigenheiten von `INSTR` und `PRINT USING`.
- Entscheidung D12: Ersatzverhalten für `INSTR` mit leerem Suchstring, `PRINT USING` ohne Stelle vor
  dem Punkt und mit mehreren Werten.
- Entscheidung D11: IEEE binary64 bleibt, Eigenheiten des Originals (`PI`, `MKS$`, ungenaue
  Funktionen) werden nicht nachgebildet.
- `CLAUDE.md` mit Hinweisen für Agenten; `docs/gfa-original.md`.
- Projektgerüst: Verzeichnisstruktur, CMake, CI (Linux/macOS/Windows, Sanitizer, Build ohne Emulatoren).
- Einbindung der Schwesterprojekte PhotonPower/Z80 (`e74cf00`) und PhotonPower/MC68000 (`0074b8b`)
  über `FetchContent` mit festem Commit. Unter Windows baut die CI vorerst ohne Emulatoren, weil der
  MC68000-Commit Dateinamen enthält, die Windows nicht erlaubt ([docs/toolchain.md](docs/toolchain.md)).
- Zielregister mit Befehlsgruppen (`include/basic/target.hpp`, `basicc --targets`): `pc`, `amiga`,
  `cpc`, `z80-test`, `m68k-test`.
- Emulator-Harness (`tests/harness`) für z80-test, cpc (Firmware-Stub `TXT OUTPUT`), m68k-test und
  amiga (Hunk-Programme mit der OS-Emulation des Schwesterprojekts), mit Smoke-Tests.
- Dokumentation: Architektur, Zwischencode, Runtime und Modulformat, Numerik, Zielsysteme,
  Sprachumfang und Befehlsgruppen, Werkzeugkette, Tests, Bedienung, Entscheidungen, Roadmap.
