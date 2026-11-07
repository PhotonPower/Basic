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
