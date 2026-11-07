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
| `probe6.lst` | Annahmen der Referenz: `VAL` mit Präfix, Gleichstand an der 13. Stelle, `DEFNUM` 1 und 3, `ROUND` – ausgewertet (2026-11-07) |
| `probe7.lst` | `PRINT USING`: Festlegungen aus D13 (Format ohne Feld, falsche Feldart, Rest des Formats, `-0.00`, langer Exponent) – ausgewertet (2026-11-07) |
| `probe8.lst` | `PRINT USING`: Lage des Exponenten bei 0 bis 4 Stellen vor dem Punkt, Vorzeichen im Exponentformat – ausgewertet (2026-11-07) |
| `probe9.lst` | Fehlernummern (`READ`, `DIM`, Index), `ERR$`-Texte 7–35, Bit- und Zahlfunktionen – ausgewertet (2026-11-07) |
| `probe17.lst` | Matrixbefehle: `MAT PRINT`-Format, `MAT BASE`, Fehlernummern, `DET`/`QDET`/`RANG`/`INV`/`NORM` an Grenzfällen; Spurzeilen vor jedem Befehl, weil `MAT ONE` das Original abstürzen lässt – ausgewertet (2026-11-07) |
| `probe18.lst` | `MAT INPUT #`: Zurücklesen von `MAT PRINT #`, CR vor dem Zeilenende bei `INPUT #`, zu wenige Werte, keine Zahl, `MAT BASE 0` – ausgewertet (2026-11-07) |
| `probe19.lst` | `ARRPTR`/`*x`, String- und Feld-Deskriptoren, `SWAP *p,a()`, `TYPE(*x)`, `ABSOLUTE`; `MALLOC` mit einem Argument – ausgewertet (2026-11-07) |
| `probe10.lst` | `VAR`-Parameter: Geltung für folgende Parameter, Referenz oder Kopie, unzulässige Argumente – ausgewertet (2026-11-07) |

Ergebnisse: `results/<name>.txt` (erzeugt am Original).
