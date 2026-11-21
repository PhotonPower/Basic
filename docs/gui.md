# Grafische Oberfläche („Basic IDE“)

`gui/basic_ide.py` ist ein Editor für GFA-BASIC-3.5-Programme mit automatischer Einrückung. Interpreter
und Compiler (`basicc`) starten per Menü. Das Programm läuft unter **Windows, macOS und Linux**; es
braucht nur Python ab 3.9 mit tkinter, keine weiteren Pakete.

```sh
cmake -S . -B build && cmake --build build     # erzeugt basicc
python3 gui/basic_ide.py [programm.lst]
```

| System | tkinter |
|---|---|
| Windows | im Python-Installer von python.org enthalten (Haken „tcl/tk“ nicht abwählen); Start mit `py gui\basic_ide.py` |
| macOS | Python von python.org, oder `brew install python-tk` |
| Linux | `sudo apt install python3-tk` bzw. `sudo dnf install python3-tkinter` |

`basicc` wird gesucht in: Einstellungen → *Pfad zu basicc*, Umgebungsvariable `BASICC`, `build/` (auch
`build/Release` für Visual Studio), `PATH`. Einstellungen liegen in `config.json` unter
`%APPDATA%\basic-ide`, `~/Library/Application Support/basic-ide` bzw. `~/.config/basic-ide`.

## Editor

- **Automatische Einrückung** (zwei Leerzeichen, einstellbar): Nach `Enter` beginnt die neue Zeile auf
  der Ebene, die der Blockaufbau verlangt. Block-Wörter wie `ENDIF`, `NEXT`, `ELSE`, `CASE`, `RETURN`
  rücken beim Tippen sofort an ihre Ebene. *Bearbeiten → Alles neu einrücken* bringt ein ganzes
  Programm in Form, *Auswahl neu einrücken* nur die markierten Zeilen. `Tab` / `Umschalt+Tab` rücken
  die Auswahl um eine Stufe ein bzw. aus.
- Regeln (`gui/gfa_indent.py`, folgt dem Parser): öffnend `IF FOR WHILE REPEAT DO SELECT PROCEDURE
  SUB FUNCTION`; schließend `ENDIF NEXT WEND UNTIL LOOP ENDSELECT ENDFUNC RETURN` samt Aliasen
  (`ENDWHILE`, `ENDPROC` …); `RETURN` schließt nur eine `PROCEDURE`, in einer `FUNCTION` liefert es
  den Wert. `ELSE`/`ELSE IF` stehen auf der Ebene ihres `IF`, `CASE`/`DEFAULT` auf der von `SELECT`
  (wie in `tests/programs`). Kommentare, Zeichenketten und `DATA` zählen nicht;
  mehrere Befehle je Zeile (`:`) werden einzeln betrachtet; `SUB a%,1` (Befehl) ist kein Block; das
  `>` vor eingeklappten Prozeduren aus dem Original-Editor bleibt erhalten.
- Farbhervorhebung, Zeilennummern, Suchen/Ersetzen (`Strg+F`, `F3`), Gehe zu Zeile (`Strg+G`),
  Schriftgröße (`Strg +/-`), dunkles Farbschema. Auf dem Mac gilt `Cmd` statt `Strg`.
- Dateien: Dateikodierung (UTF-8 oder Latin-1 wie auf dem Amiga) und Zeilenende (LF/CRLF) bleiben
  beim Speichern erhalten.

## Programm-Menü

| Eintrag | Taste | Wirkung |
|---|---|---|
| Im Interpreter starten | `F5` | speichert, startet `basicc --run`; Ausgabe im unteren Fenster, `INPUT` über das Eingabefeld; Grafik erscheint nach dem Lauf in einem eigenen Fenster |
| Stoppen | `Umschalt+F5` | beendet das laufende Programm |
| Kompilieren (letztes Ziel) | `F6` | wie unten, mit dem zuletzt gewählten Ziel |
| Kompilieren für … | | `basicc --target …`: Amiga (Hunk / `.s`), Amstrad CPC (`.bin` / `.sna` / `.s`), PC (`.cpp`), Z80- und MC68000-Testplattform; Ausgabe neben dem Quelltext |
| Syntax prüfen | `F7` | übersetzt nur das Frontend |

Der CPC-Snapshot braucht das 32-KiB-ROM der CPC 6128 (*Einstellungen → CPC-ROM*; wird nie
eingecheckt). Fehlermeldungen (`datei:zeile:spalte: text`) stehen rot in der Ausgabe; ein Klick
darauf, beim ersten Fehler automatisch, setzt den Cursor in die Zeile. Ein unbenannter Text wird für
`F5` und `F7` in einer temporären Datei ausgeführt, zum Kompilieren muss er gespeichert sein.

## Tests

`tests/gui` prüft Einrückung (auch gegen alle Programme in `tests/programs`: sie müssen unverändert
bleiben), Farbhervorhebung und das Starten von `basicc` (Eingabeaufforderung vor der Eingabe, Fehlerzeilen,
Compiler-Lauf); ctest führt sie als `gui_logic` aus. Das tkinter-Fenster selbst braucht ein Display und
wird nicht automatisch getestet.
