# tests/probes

Prüfprogramme, die am echten GFA-BASIC 3.5 (FS-UAE) laufen, um offene Sprachfragen zu klären.
Ausführen mit `tools/gfa/gfa_run.py`, Einrichtung und Regeln in [docs/gfa-original.md](../../docs/gfa-original.md).

| Programm | Fragen |
|---|---|
| `probe1.lst` | Ganzzahl-Überlauf, Float → Integer, `DIV`/`MOD`, `CINT`, `FOR`-Semantik, Zahlenausgabe, `==`, `MKD$`/`MKS$`-Format, Über-/Unterlauf, Fehlernummern, `VAL` – ausgewertet (2026-11-07) |
| `probe2.lst` | Folgefragen: Integer- oder Float-Rechnung in Ausdrücken, Byte-Variablen, `FOR` mit negativer/geänderter Schrittweite und am Bereichsende, Wert nach Float-Überlauf, `PI` und Ausgabe-Rundung, Schwellen der Exponentdarstellung, `DEFNUM`, Regel von `==`, `MKS$`/`CVS` – ausgewertet (2026-11-07) |
| `probe3.lst` | Regel von `==` mit exakten Zweierpotenzen, Genauigkeit von `^`, Verhalten am Ende des Exponentbereichs (Über- und Unterlauf) – ausgewertet (2026-11-07) |
| `probe4.lst` | `SELECT` mit Strings, `CONT`, `PRINT USING`, `MODE`, `STR$(x,y,z)`, Operatoren, String-Funktionen – ausgewertet (2026-11-07) |
| `probe5.lst` | Rangfolge der Operatoren, `INSTR` mit leerem Suchstring, `STR$(x,n)`, Randfälle von `PRINT USING` – ausgewertet (2026-11-07) |

Ergebnisse: `results/<name>.txt` (erzeugt am Original).
