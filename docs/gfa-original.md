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
   die GFA-BASIC-Diskette (hier `~/Documents/FS-UAE/Floppies/GFABasic35.adf`). Die Boot-Priorität `-128` für `DH0:` sorgt dafür, dass
   weiter von der Diskette gestartet wird. Alternativ mit `--config` bzw. `GFA_FSUAE_CONFIG` auf eine
   eigene Konfiguration zeigen.
3. **Bedienungshilfen:** In den Systemeinstellungen unter *Datenschutz & Sicherheit → Bedienungshilfen*
   dem Terminal (bzw. der App, aus der Claude Code läuft) erlauben, den Computer zu steuern. Sonst
   lehnt `osascript` die Tastendrücke ab. Für Bildschirmfotos zusätzlich *Bildschirmaufnahme*.
4. **Tastaturbelegung:** `--amiga-layout us` (Standard) oder `de`, passend zur Tastaturbelegung, die
   auf dem Amiga eingestellt ist. Die vorhandene Diskette (GFA-BASIC 3.5 englisch) startet mit der
   US-Belegung; mit `de` kommt `:` als `>` an. Das Skript tippt nur Buchstaben, Ziffern, `.` und `:`.
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
  ├─ osascript: Return (schließt die Meldung „Program End“ oder einen Fehler des letzten Laufs),
  │            New (Shift+F4), Return („NEW - Kill program?“ → Kill), Merge (F2),
  │            40 × Del (das Dateifeld ist mit dem Diskettennamen vorbelegt, Cursor am Anfang),
  │            "dh0:probe1.lst", Return, Run (Shift+F10)
  ├─ warten, bis gfa-share/probe1.txt eine Zeile "ende" enthält (--timeout)
  └─ Ergebnis ausgeben (--out kopiert es zusätzlich); bei Zeitüberschreitung Bildschirmfoto probe1.png
```

**Während eines Laufs (rund 15 Sekunden je Programm) kein anderes Fenster aktivieren.** Das Skript
holt FS-UAE nach vorn und prüft vor jedem einzelnen Tastendruck, ob es noch vorn ist; sonst bricht
es mit Rückgabewert 3 ab, statt in ein anderes Fenster zu tippen. Danach den Lauf wiederholen.

Das FS-UAE-Menü (F12) darf nicht offen sein, sonst gehen die Tastendrücke dorthin; Escape schließt
es.

Rückgabewert: `0` Ergebnis vollständig, `1` Zeitüberschreitung (Bildschirmfoto ansehen), `2`
Einrichtungsfehler, `3` FS-UAE hat während der Tastenfolge den Fokus verloren.

## Regeln für Prüfprogramme

- Reines ASCII, eine Anweisung pro Zeile, wie mit `SAVE,A` gespeichert.
- Eine Zeile `datei$="…"` nennt die Ergebnisdatei; alles wird mit `PRINT #1` dorthin geschrieben.
- Letzte Zeile der Ergebnisdatei ist `ende` (daran erkennt das Skript das Ende).
- Fehler fängt eine `ON ERROR GOSUB`-Prozedur ab, schreibt `ERR` und `ERR$(ERR)` und setzt mit
  `RESUME NEXT` fort; sie muss `ON ERROR GOSUB` erneut setzen (Handbuch).
- Nach einem abgefangenen Fehler ist der Ausgabekanal unbrauchbar („file not open“). Die
  Fehlerprozedur merkt sich deshalb zuerst `ERR`, schließt den Kanal und öffnet ihn mit `OPEN "A"`
  neu (siehe `probe1.lst`, Prozedur `fehler`).
- Jeder Test hat eine Kennung (`T1`, `T2` …), damit Ergebnisse eindeutig zugeordnet werden können.
- Prüfprogramme mit Tastatureingabe (`INPUT`, `INKEY$`) aktivieren vor der ersten Eingabe ihr
  Ausgabefenster mit `~ActivateWindow(WINDOW(0))`, sonst landen die Tasten im Editor (probe15). Die
  Eingaben kommen über `gfa_run.py --after` (z. B. `--after type:12 --after key:return`).
- Stürzt das Original ab („Software error“), bleibt die gepufferte Ergebnisdatei leer. Abhilfe:
  vor riskanten Befehlen eine Spurzeile schreiben und die Datei schließen und mit `OPEN "A"` wieder
  öffnen (probe17–20, Prozedur `flush`). FS-UAE vor jedem Lauf beenden, ein liegengebliebener
  Zustand lässt GFA-BASIC beim Start abstürzen.

## Grafikprogramme auf dem Amiga

`tools/gfa/amiga_run.py` startet die Programme nacheinander: Ein Grafikprogramm öffnet ein aktives
Fenster und bekäme sonst die vorab getippten Befehle. Wer ein Bild prüfen will, lässt das Programm
am Ende mit `PAUSE` warten (die Ausgabe erscheint erst am Programmende in der Datei) und macht das
Bildschirmfoto in dieser Zeit.

## Tokenisierte Programme in Listings umwandeln

`tools/gfa/gfa2lst.py prog.gfa … --out-dir <verzeichnis>` lädt `.GFA`-Dateien (Kennung „GFA-AMIGAB“)
im Direktmodus des Editors (`LOAD`) und schreibt sie mit `LIST` als ASCII-`.LST`. Fremde Programme
nur lokal halten (außerhalb des Repositorys) und die Kopien im Austauschverzeichnis danach löschen.

## Eigene Programme auf dem Amiga

`tools/gfa/amiga_run.py` startet mit `basicc --target amiga` übersetzte Programme im FS-UAE unter
AmigaOS: Kaltstart, an der Shell (`1>`) für jedes Programm `dh0:<name> >dh0:<name>.txt`, dann
Warten, bis alle Ergebnisdateien fertig sind, zuletzt `1` (GFA-BASIC wieder starten).

```sh
basicc --target amiga tests/programs/numbers.lst -o build/numbers
python3 tools/gfa/amiga_run.py build/numbers --out-dir build/amiga-results
```

Das aktuelle Verzeichnis bleibt die Startdiskette (auf der GFA-Diskette fehlt `CD`): Programme,
die Dateien schreiben, brauchen Namen mit `DH0:`, sonst landen die Dateien im Disketten-Image.

Stand 2026-11-07: alle Programme unter `tests/programs` laufen so unter Kickstart 1.2 und geben Byte
für Byte die erwartete Ausgabe aus. Gefunden dabei: Die Shell gibt einem Programm nur 4000 Bytes
Stapel; die Plattform holt sich deshalb einen eigenen (`platform/amiga/plat_start.s`).

## Auswertung

Die Ergebnisdatei wird unter `tests/probes/results/<name>.txt` eingecheckt (Kopf:
`; source: GFA-BASIC 3.5`). Geklärte Punkte werden in [numerics.md](numerics.md) und
[language.md](language.md) eingetragen; Festlegungen, die sich dadurch ändern, bekommen einen Eintrag in
[decisions.md](decisions.md). Was sich als Golden-Test eignet, wandert nach `tests/programs/`.
