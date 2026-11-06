# Mitarbeit

## Bauen und testen

```sh
cmake -S . -B build -DBASIC_WARNINGS_AS_ERRORS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Voraussetzungen: CMake ≥ 3.16, ein C++20-Compiler (GCC ≥ 11, Clang ≥ 14, MSVC 2022) und Netzzugang
beim ersten Konfigurieren (die Schwesterprojekte werden über `FetchContent` geholt, siehe
[docs/toolchain.md](docs/toolchain.md)).

| Option | Standard | Bedeutung |
|---|---|---|
| `BASIC_BUILD_TESTS` | `ON` | Unit-Tests bauen |
| `BASIC_WITH_EMULATORS` | `ON` | Schwesterprojekte Z80/MC68000 einbinden (Assembler, Emulator-Harness) |
| `BASIC_WARNINGS_AS_ERRORS` | `OFF` | Compilerwarnungen als Fehler (in der CI auf Linux `ON`) |
| `BASIC_SANITIZE` | `OFF` | AddressSanitizer + UBSan (GCC/Clang) |

## Regeln

- Neue Funktionalität bekommt Tests in `tests/`. Fehlerkorrekturen bekommen zuerst einen Test, der
  den Fehler zeigt.
- `runtime/ref` definiert die Semantik. Eine Änderung dort braucht einen Eintrag in
  [docs/decisions.md](docs/decisions.md) und neue Sollwerte für die Konformanztests aller Ports.
- Runtime-Module halten das Modulformat ein (Kopfzeilen, Label-Hygiene `<modul>__<name>`,
  [docs/runtime.md](docs/runtime.md#modulformat)).
- Optimierungen müssen die Differenztests bestehen.
- Die Bibliothek (`include/basic`, `src`) hat keine Ein-/Ausgabe auf der Konsole und keine globalen
  Zustände; Benutzerinteraktion gehört nach `app/`.
- Quelltext, Bezeichner und Kommentare sind Englisch, die Dokumentation unter `docs/` Deutsch.
- Formatierung: siehe `.clang-format` / `.editorconfig`.
- Das Anheben des Commits eines Schwesterprojekts in `CMakeLists.txt` ist eine eigene Änderung mit
  Eintrag im `CHANGELOG.md`.
