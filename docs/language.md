# Sprache und Befehlsgruppen

Der Compiler übersetzt **GFA-BASIC 3.0/3.5 (Amiga)** in der ASCII-Form (`.LST`). Grundlage ist die
Befehlsreferenz `GFA-BASIC_3_5_Amiga_Befehlsreferenz.md`, die aus dem Handbuch (GFA Systemtechnik,
1988, mit den 3.5-Kapiteln 13 und 14) extrahiert wurde. Sie liegt derzeit nicht in diesem Repository;
ihr Anhang C sammelt OCR-Unsicherheiten und Widersprüche, die vor der Implementierung eines Befehls
zu prüfen sind.

Das tokenisierte `.GFA`-Format wird nicht gelesen (es ist im Handbuch nicht beschrieben); Programme
werden im Original mit `SAVE,A` als `.LST` gespeichert.

## Befehlsgruppen

Jeder Befehl und jede Funktion gehört zu genau einer Gruppe. Eine Plattform unterstützt eine Menge
von Gruppen; benutzt ein Programm eine Gruppe, die das Zielsystem nicht hat, bricht der Compiler mit
einer Meldung ab (statt Stubs zu erzeugen, die zur Laufzeit stillschweigend nichts tun).

| Gruppe | Inhalt (Kapitel der Referenz) |
|---|---|
| `core` | Variablen und Typen, Zuweisung, Operatoren, Ganzzahl- und Bitfunktionen, Arrays, `DIM`/`ERASE`/`INSERT`/`DELETE`/`SWAP`/`QSORT`, Kontrollfluss, Prozeduren/Funktionen, `DATA`/`READ`, Fehlerbehandlung (Kap. 2, 3, 4 teilweise, 7) |
| `float` | Fließkomma-Arithmetik und -Funktionen, `RND`, `FACT`/`VARIAT`/`COMBIN` (Kap. 4, 14) |
| `string` | String-Variablen und -Funktionen, `MKx$`/`CVx` (Kap. 2, 4) |
| `print` | `PRINT`, `PRINT USING`, `WRITE`, `TAB`, `POS`, `DEFNUM`, `MODE`, `LOCATE`, `PRINT AT` (Kap. 5) |
| `input` | `INPUT`, `LINE INPUT`, `FORM INPUT`, `INKEY$` (Kap. 5) |
| `file-seq` | `OPEN` (sequentiell), `PRINT #`, `INPUT #`, `LINE INPUT #`, `WRITE #`, `CLOSE`, `EOF`, Dateiverwaltung (Kap. 6) |
| `file-random` | wahlfreier Zugriff: `FIELD`, `GET`, `PUT`, `SEEK`, `LOF`, mehrere Kanäle gleichzeitig (Kap. 6) |
| `memory` | `PEEK`/`POKE`, `VARPTR`/`V:`, `*x`, `BYTE{}` …, `ABSOLUTE`, `MALLOC`/`MFREE`, `BMOVE` (Kap. 2) |
| `graphics` | Grafikbefehle aus Kap. 8 |
| `gui` | Screens, Fenster, Menüs, Requester, Ereignisse (Kap. 9) |
| `sprites` | Sprites, BOBs, Joystick, Maus (Kap. 5, 10) |
| `sound` | `SOUND`, `WAVE`, Sprachausgabe (Kap. 5, 10) |
| `matrix` | Vektoren und Matrizen (Kap. 13, nur 3.5) |
| `amiga-os` | direkte Aufrufe der AmigaOS-Bibliotheken (Kap. 11), `GURU` |

Die Gruppen sind in `include/basic/target.hpp` definiert. Die genaue Zuordnung jedes einzelnen
Befehls entsteht mit dem Frontend als Tabelle in `src/frontend`.

## Unterstützung je Zielsystem

Geplanter Umfang (✓ geplant, – nicht vorgesehen, (✓) mit Einschränkung):

| Gruppe | pc | amiga | cpc | z80-test | m68k-test |
|---|---|---|---|---|---|
| core | ✓ | ✓ | ✓ | ✓ | ✓ |
| float | ✓ | ✓ | ✓ | ✓ | ✓ |
| string | ✓ | ✓ | ✓ | ✓ | ✓ |
| print | ✓ | ✓ | ✓ | ✓ | ✓ |
| input | ✓ | ✓ | ✓ | – | – |
| file-seq | ✓ | ✓ | (✓) eine Ein- und eine Ausgabedatei | – | – |
| file-random | ✓ | ✓ | – | – | – |
| memory | (✓) emulierter Speicher | ✓ | (✓) Little-Endian, 16-Bit-Adressen | ✓ | ✓ |
| graphics | später | ✓ | (✓) Firmware-Grafik | – | – |
| gui | – | ✓ | – | – | – |
| sprites | – | ✓ | – | – | – |
| sound | – | ✓ | – | – | – |
| matrix | ✓ | ✓ | ✓ | ✓ | ✓ |
| amiga-os | – | ✓ | – | – | – |

## Reihenfolge der Umsetzung

1. Sprachkern: Integer, Float, Strings, `PRINT`, Schleifen, `IF`/`SELECT`, Prozeduren mit `LOCAL` und
   `VAR`, `DATA`/`READ` – komplett durch alle Stufen und auf allen Zielen.
2. Arrays, String-Funktionen, `PRINT USING`, Fehlerbehandlung mit `ON ERROR`/`RESUME`.
3. `INPUT`, Dateien.
4. `memory`, `matrix`.
5. Plattformspezifisch: Grafik (Amiga, CPC), dann GUI, Sprites, Sound und Kapitel 11 (Amiga).

## Offene Sprachfragen

Diese Punkte sind aus dem Handbuch nicht sicher zu entnehmen und müssen am Original (Emulator mit
GFA-BASIC 3.5) geklärt werden. Ergebnisse werden als Testprogramme unter `tests/programs` festgehalten.

- **Arithmetik in Ausdrücken:** Rechnet GFA `a%=b%+c%` in Integer oder in Float, und gibt es eine
  Überlaufprüfung? (Befehle `ADD`/`INC` sind laut Referenz ohne Prüfung.)
- **`FOR … NEXT`:** Laut Handbuch wird die Schleife mindestens einmal durchlaufen, die Endbedingung am
  `NEXT` geprüft (Anhang C, Punkt 15). Werden Endwert und Schrittweite einmal oder bei jedem Durchlauf
  ausgewertet? Wert der Laufvariablen nach der Schleife?
- **Zahlenausgabe:** genaue Regeln von `PRINT` für Floats (Exponentdarstellung, führendes Leerzeichen,
  `0.5` vs. `.5`) und die Wirkung von `DEFNUM` und `MODE`.
- **`CINT`:** Rundung bei genau ,5.
- **Fehlernummern:** Das Handbuch enthält keine vollständige Tabelle der GFA-Fehlermeldungen
  (Anhang C, Punkt 17); Nummern für Überlauf, Division durch 0 usw. sind zu ermitteln.
- **`SELECT` mit Mehrzeichen-Strings:** Packung der Zeichen (Anhang C, Punkt 8).
- **Ausgabe auf dem Amiga:** Ein GFA-Programm gibt in ein Fenster aus (öffnet es bei Bedarf). Für
  den ersten Stand schreibt das kompilierte Programm auf `Output()` der Shell; Fensterausgabe folgt mit
  der Gruppe `gui`.
