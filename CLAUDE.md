# Hinweise für Agenten (Claude Code)

Dieses Repository ist ein Compiler für GFA-BASIC 3.5 (Amiga) mit den Zielen MC68000/Amiga, Z80/Amstrad CPC
und C++. Überblick: `README.md`, Aufbau: `docs/architecture.md`, Stand und Plan: `docs/roadmap.md`.

## Bauen und testen

```sh
cmake -S . -B build -DBASIC_WARNINGS_AS_ERRORS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Vor jedem Commit müssen alle Tests laufen.

## Konventionen

- Code, Bezeichner und Kommentare Englisch; Dokumentation unter `docs/` Deutsch.
- Neue Funktionalität mit Tests; Fehlerkorrektur zuerst mit einem Test, der den Fehler zeigt.
- `runtime/ref` definiert die Semantik; Änderungen dort brauchen einen Eintrag in `docs/decisions.md`.
- `CHANGELOG.md` unter `[Unreleased]` pflegen.
- Die Schwesterprojekte (PhotonPower/Z80, PhotonPower/MC68000) nur lesen, nicht ändern.

## Am Original prüfen (nur auf dem Mac mit FS-UAE)

Offene Sprachfragen werden am echten GFA-BASIC 3.5 geklärt, siehe `docs/gfa-original.md`.

```sh
python3 tools/gfa/gfa_run.py tests/probes/probe1.lst --out tests/probes/results/probe1.txt
```

- Bei Rückgabewert 1 das Bildschirmfoto im gemeinsamen Verzeichnis
  (`~/Documents/FS-UAE/gfa-share/<name>.png`) ansehen und herausfinden, wo es hängt (Syntaxfehler im
  Editor, Rückfrage, Dateiauswahl). Tastenfolge in `tools/gfa/keys.json` anpassen, mit `--dry-run`
  prüfen, erneut versuchen.
- Lehnt der GFA-Editor eine Zeile ab, das Prüfprogramm korrigieren (Syntax laut Befehlsreferenz) und den
  Grund in der Commit-Nachricht nennen.
- Kickstart-ROM, ADF-Images und Bildschirmfotos nie einchecken.
- Ergebnisse auswerten: geklärte Punkte in `docs/numerics.md` und `docs/language.md` eintragen (mit
  Testkennung, z. B. „probe1 T7“), offene Fragen in `docs/roadmap.md` abhaken, neue Fragen als
  weiteres Prüfprogramm `tests/probes/probeN.lst` formulieren.
