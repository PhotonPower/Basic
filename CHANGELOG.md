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
  Original (probe9). Golden-Tests `tests/programs` (7 Programme, Ausgaben noch nicht am Original
  abgeglichen).
- `gfa_run.py`: Programme ohne `datei$`-Zeile werden umgeschrieben (`PRINT` in eine Datei, Kanal in
  Fehlerroutinen neu öffnen); wartet, bis FS-UAE den Fokus hat; erstes Return doppelt.
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
