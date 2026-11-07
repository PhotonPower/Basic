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
- Abgleich mit dem Original: `tools/gfa/gfa_run.py` führt `.lst`-Programme im echten GFA-BASIC 3.5
  unter FS-UAE (macOS) aus (gemeinsames Verzeichnis als `DH0:`, Tastendrücke über `osascript`,
  Bildschirmfoto bei Zeitüberschreitung); Beispielkonfiguration, Tastenfolge `keys.json`, Python-Tests.
- Erstes Prüfprogramm `tests/probes/probe1.lst` (54 Tests zu Überlauf, Rundung, `FOR`, Zahlenausgabe,
  `==`, Float-Format, Fehlernummern).
- probe1 am Original ausgewertet (`tests/probes/results/probe1.txt`): Ganzzahl-Überlauf und
  Rundungsregeln, `FOR`-Semantik, Ausgabeformat, Fehlernummern und das GFA-8-Byte-Float-Format
  (48 Bit Mantisse) in `docs/numerics.md` und `docs/language.md` eingetragen; neue offene Frage zu D4
  in `docs/roadmap.md`. Folgefragen als `tests/probes/probe2.lst`.
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
