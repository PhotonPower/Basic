# Abgleich mit dem Original (GFA-BASIC 3.5 im FS-UAE)

Was das Handbuch offenlässt ([language.md](language.md#offene-sprachfragen), [numerics.md](numerics.md)),
wird am echten GFA-BASIC 3.5 geklärt. Dazu laufen Prüfprogramme (`tests/probes/*.lst`) im Emulator
FS-UAE auf dem Mac; `tools/gfa/gfa_run.py` erledigt das ohne Handarbeit und kann von einem Agenten
(Claude Code, siehe [`CLAUDE.md`](../CLAUDE.md)) benutzt werden.

Kickstart-ROM und GFA-BASIC-Diskette gehören **nicht** ins Repository. Die Läufe finden lokal statt;
eingecheckt werden nur die Ergebnisse.

## Einrichtung (einmalig, macOS)

1. **Gemeinsames Verzeichnis** anlegen: `~/Documents/FS-UAE/gfa-share`. FS-UAE bindet es als
   Festplatte `DH0:` ein.
2. **Konfiguration:** `tools/gfa/gfa.fs-uae.example` nach `tools/gfa/gfa.fs-uae` kopieren (wird von
   git ignoriert) und anpassen: Pfad zum Kickstart-ROM, `floppy_drive_0` auf
   `~/Documents/FS-UAE/GFABasic35.adf`. Die Boot-Priorität `-128` für `DH0:` sorgt dafür, dass
   weiter von der Diskette gestartet wird. Alternativ mit `--config` bzw. `GFA_FSUAE_CONFIG` auf eine
   eigene Konfiguration zeigen.
3. **Bedienungshilfen:** In den Systemeinstellungen unter *Datenschutz & Sicherheit → Bedienungshilfen*
   dem Terminal (bzw. der App, aus der Claude Code läuft) erlauben, den Computer zu steuern. Sonst
   lehnt `osascript` die Tastendrücke ab. Für Bildschirmfotos zusätzlich *Bildschirmaufnahme*.
4. **Tastaturbelegung:** `--amiga-layout de` (Standard) oder `us`, passend zur Tastaturbelegung, die
   auf dem Amiga eingestellt ist. Das Skript tippt nur Buchstaben, Ziffern, `.` und `:`.
5. **Erster Lauf von Hand beobachten:**
   ```sh
   python3 tools/gfa/gfa_run.py tests/probes/probe1.lst --screenshot
   ```
   Falls GFA-BASIC nach dem Booten nicht von selbst im Editor steht, `--boot-wait` erhöhen bzw. das
   Starten ergänzen. Falls die Tastenfolge nicht passt (Rückfrage bei *New*, Dateiauswahl mit eigenem
   Eingabefeld …), `tools/gfa/keys.json` anpassen und mit `--dry-run` prüfen.

## Ablauf eines Laufs

```
gfa_run.py probe1.lst
  ├─ probe1.lst → ~/Documents/FS-UAE/gfa-share/probe1.lst   (datei$ → "DH0:probe1.txt")
  ├─ FS-UAE starten, falls es nicht läuft; --boot-wait Sekunden warten
  ├─ osascript: Return, New (Shift+F4), Merge (F2), "dh0:probe1.lst", Return, Run (Shift+F10)
  ├─ warten, bis gfa-share/probe1.txt eine Zeile "ende" enthält (--timeout)
  └─ Ergebnis ausgeben (--out kopiert es zusätzlich); bei Zeitüberschreitung Bildschirmfoto probe1.png
```

Rückgabewert: `0` Ergebnis vollständig, `1` Zeitüberschreitung (Bildschirmfoto ansehen), `2`
Einrichtungsfehler.

## Regeln für Prüfprogramme

- Reines ASCII, eine Anweisung pro Zeile, wie mit `SAVE,A` gespeichert.
- Eine Zeile `datei$="…"` nennt die Ergebnisdatei; alles wird mit `PRINT #1` dorthin geschrieben.
- Letzte Zeile der Ergebnisdatei ist `ende` (daran erkennt das Skript das Ende).
- Fehler fängt eine `ON ERROR GOSUB`-Prozedur ab, schreibt `ERR` und `ERR$(ERR)` und setzt mit
  `RESUME NEXT` fort; sie muss `ON ERROR GOSUB` erneut setzen (Handbuch).
- Jeder Test hat eine Kennung (`T1`, `T2` …), damit Ergebnisse eindeutig zugeordnet werden können.

## Auswertung

Die Ergebnisdatei wird unter `tests/probes/results/<name>.txt` eingecheckt (Kopf:
`; source: GFA-BASIC 3.5`). Geklärte Punkte werden in [numerics.md](numerics.md) und
[language.md](language.md) eingetragen; Festlegungen, die sich dadurch ändern, bekommen einen Eintrag in
[decisions.md](decisions.md). Was sich als Golden-Test eignet, wandert nach `tests/programs/`.
