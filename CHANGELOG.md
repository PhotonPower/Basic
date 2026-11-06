# Changelog

Alle nennenswerten Änderungen an diesem Projekt werden hier festgehalten.
Das Format orientiert sich an [Keep a Changelog](https://keepachangelog.com/de/1.1.0/),
die Versionierung an [Semantic Versioning](https://semver.org/lang/de/).

## [Unreleased]

### Hinzugefügt
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
