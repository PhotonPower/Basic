# Entscheidungen

Festgehaltene Entwurfsentscheidungen mit Begründung. Eine Änderung bekommt einen neuen Eintrag, der
den alten ausdrücklich ersetzt.

<a id="d1"></a>
## D1 – Zwischencode in zwei Ebenen

Drei sehr verschiedene Ziele (C++, MC68000, Z80) teilen sich ein Frontend. Die **HIR** behält die
Programmstruktur für lesbaren C++-Code; die **LIR** (Drei-Adress-Code, explizite Typen bis `i8`/`i16`,
Runtime-Aufrufe als eigene Operation) dient den Assembler-Backends. → [ir.md](ir.md)

<a id="d2"></a>
## D2 – Zielsystem = CPU × Plattform

Mathematik, Strings und Formatierung hängen nur von der CPU ab, alles Maschinenspezifische liegt in
einer kleinen Plattformschicht (`plat_*`). Neue Ziele mit bekannter CPU, insbesondere FPGA-Implementierungen
von Z80 und MC68000, brauchen nur eine neue Plattform. → [architecture.md](architecture.md), [targets.md](targets.md)

<a id="d3"></a>
## D3 – Eigene, bitgleiche Numerik auf allen Zielen

Weder CPC-Firmware noch Amiga-Mathe-Bibliotheken noch `libm` liefern gleiche Ergebnisse. Die Numerik
wird selbst geschrieben; eine C-Referenz definiert die Semantik, die Assembler-Ports werden bitgenau
dagegen getestet. → [numerics.md](numerics.md)

<a id="d4"></a>
## D4 – IEEE binary64 ohne Denormale, Unendlich und NaN

Speicherformat IEEE (einfache Konvertierung an `DOUBLE{}`/`SINGLE{}`-Schnittstellen, mehr Stellen als
GFA), aber vereinfachte Semantik (nur Rundung zur nächsten geraden Zahl, Denormale → 0, Überlauf und
Division durch 0 → GFA-Laufzeitfehler, kein −0). Das spart auf dem Z80 viel Code.
Verworfen: eigenes 8-Bit-freundliches Format (Konvertierungskosten an jeder IEEE-Schnittstelle).

<a id="d5"></a>
## D5 – Feinkörnige Runtime, gelinkt auf Quelltextebene

Eine Routine je Modul, explizite `requires`; Formatieren und Ausgeben getrennt; `PRINT` zerlegt;
Fehlertexte und Initialisierung nur bei Bedarf. Weil die Assembler der Schwesterprojekte keinen Linker
haben, bildet der Compiler die Hülle der benötigten Module selbst und fügt sie zu einem Quelltext
zusammen. → [runtime.md](runtime.md)

<a id="d6"></a>
## D6 – Emulierter Big-Endian-Speicher im C++-Ziel

Damit `PEEK`/`POKE`, `VARPTR`, `*x`, `BYTE{}` …, `MKx$`/`CVx` und `TYPE()` auf dem PC dieselben Ergebnisse
liefern wie auf dem Amiga, liegen alle BASIC-Daten im C++-Ziel in einem Byte-Array mit Amiga-Layout.
Sonst taugt das C++-Ziel nicht als Testreferenz.

<a id="d7"></a>
## D7 – Befehlsgruppen statt Stubs

Jede Plattform deklariert ihre Gruppen; der Compiler lehnt Programme ab, die eine fehlende Gruppe
benutzen, statt Stubs zu erzeugen, die zur Laufzeit nichts tun. → [language.md](language.md)

<a id="d8"></a>
## D8 – Zielsysteme Amiga und Amstrad CPC

Der MC68000 wird für den Amiga übersetzt (Heimat von GFA-BASIC, voller Funktionsumfang), der Z80 für
den Amstrad CPC (eingeschränkter Umfang: kein `file-random`, keine GUI, Firmware-Grafik).

<a id="d9"></a>
## D9 – Werkzeuge der Schwesterprojekte als Bibliothek, fest auf einen Commit

Assembler und Emulatoren kommen aus PhotonPower/Z80 und PhotonPower/MC68000 über `FetchContent` mit
festem Commit. Kein Aufruf externer Programme, keine fremden Toolchains (SDCC, vbcc) nötig. Die Runtime
für MC68000 und Z80 ist deshalb handgeschriebener Assembler; C gibt es nur in der Referenz und im
PC-Ziel.
→ [toolchain.md](toolchain.md)

<a id="d11"></a>
## D11 – IEEE binary64 bleibt; Eigenheiten des Originals werden nicht nachgebildet

Bestätigt [D4](#d4) nach dem Abgleich mit dem Original (probe1–probe3). GFA-BASIC 3.5 rechnet mit
48 Bit Mantisse und eigenem Exponentformat; wir bleiben bei binary64. Damit weichen einzelne
Ergebnisse vom Original ab, z. B. läuft `FOR x=0 TO 1 STEP 0.1` 11- statt 10-mal, und die Grenzen
für Über- und Unterlauf liegen anders.

Nicht nachgebildet werden Eigenheiten des Originals, die als Fehler oder Ungenauigkeit gelten:
die ungenaue Konstante `PI` (wir nehmen den nächstgelegenen binary64-Wert), das fehlerhafte `MKS$`
(wir liefern IEEE single), die ungenauen transzendenten Funktionen.

Übernommen wird dagegen alles, was Sprachsemantik ist und sich im binary64-Format darstellen lässt:
Ganzzahl- und Rundungsregeln, `FOR`-Semantik, Ausgabeformat, Fehlernummern und -texte, die Regel von
`==` (siehe [numerics.md](numerics.md)).

<a id="d20"></a>
## D20 – Tastatureingabe auf einer Textkonsole

`INPUT`, `LINE INPUT`, `INKEY$` und `INPUT$(n)` lesen aus einem Zeichenstrom (Terminal, Datei,
Eingabeport von `m68k-test`, `Input()` auf dem Amiga). Das Echo der Eingabe ist Sache der Konsole;
das Programm gibt nur die Aufforderung aus und setzt danach die Spalte auf 0.

Nach der Befehlsreferenz (Kap. 5): Aufforderung „text? “ bei `"text";`, „text“ bei `"text",`,
„? “ ohne Text (am Original gesehen: `T1 Zahl? `, probe15). Werte von `INPUT` durch Kommas getrennt,
fehlende aus weiteren Zeilen. Unsere Festlegungen, wo die Referenz schweigt und probe15 nicht weiter
kam (die Tasten erreichten das Ausgabefenster nicht): Folgezeilen fordern mit „? “ auf; eine
ungültige Zahl (auch eine leere Eingabe) wiederholt Aufforderung und Eingabe, ohne Piepton; Werte
über die Zahl der Variablen hinaus werden verworfen; ein String behält seine Leerzeichen;
Anführungszeichen haben keine Bedeutung. `INKEY$` liefert das nächste Zeichen oder "" am Ende
(auf einer zeilengepufferten Konsole erst nach Return), ein Zeilenvorschub zählt als Return (13).
Das Ende der Eingabe während `INPUT` ist Fehler 26.

<a id="d19"></a>
## D19 – Fehler in Kopfzeilen; Abweichungen der Fehlerbehandlung vom Original

`RESUME NEXT` setzt wie im Original (probe11) in der nächsten Programmzeile fort: Nach einem Fehler
in der Bedingung von `WHILE`, `IF`, `ELSE IF`, `DO WHILE`/`DO UNTIL` geht es im Rumpf weiter, nach
einem Fehler in der `FOR`-Zeile läuft der Rumpf einmal, `NEXT` erhöht die Laufvariable und verlässt
die Schleife. `RESUME` wiederholt nur die Kopfzeile. Fehler in `UNTIL`, `LOOP UNTIL`, `NEXT` und im
`SELECT`-Ausdruck gehen hinter die Struktur. Interpreter, C++-Ziel und 68000 setzen das gleich um.

Bewusst **anders** als das Original (Fehler des Originals, keine Sprachregeln):
- Schlägt in der `FOR`-Zeile die Schrittweite fehl, rechnet `NEXT` mit 1 statt mit einem zufälligen
  Wert.
- Nach einem Fehler in einer `DEFFN`-Formel wird deren Parameter wie bei jedem Rücksprung
  wiederhergestellt; das Original lässt den Wert aus dem Aufruf stehen.
- Ein Fehler in der Fehlerroutine nach erneutem `ON ERROR GOSUB` wird behandelt
  (`tests/programs/errnest.lst`); das Original stürzt ab.

<a id="d18"></a>
## D18 – Strings der Assembler-Ziele: Deskriptoren, Rückverweise, Zusammenschieben

Wie im Original hält jede String-Variable einen **Deskriptor** (Adresse, Länge). Jeder String-Block
im Heap beginnt mit einem **Rückverweis** auf den Deskriptor, dem er gehört, und seiner Länge. Ist der
Heap voll, schiebt die **Speicherbereinigung** alle Blöcke mit Besitzer zusammen und trägt die neue
Adresse im jeweiligen Deskriptor ein; Blöcke ohne Besitzer sind Abfall. Reicht der Platz danach nicht,
folgt Fehler 8 „out of memory“.

- **Zwischenergebnisse** (`LEFT$(a$,3)`, `a$+b$` …) gehören Deskriptoren in einem **Temp-Bereich** mit
  Stapeldisziplin. An jeder Anweisungsgrenze werden die Zwischenergebnisse der laufenden Routine
  freigegeben; eine Routine merkt sich beim Eintritt den Stand und gibt beim Verlassen alles darüber
  frei. Eine String-Funktion übergibt ihr Ergebnis über einen eigenen Rückgabe-Deskriptor.
- **Zuweisung** übernimmt den Block eines Zwischenergebnisses (kein Kopieren), Variablen und
  Konstanten werden kopiert.
- **`LOCAL` und Parameter** sichern den Deskriptor auf dem Stapel und setzen den Rückverweis des
  Blocks auf die gesicherte Kopie, beim Wiederherstellen zurück.
- Ein virtuelles Register vom Typ String hält die Adresse eines Deskriptors.

Das passt zu beiden CPUs: Es gibt keine Referenzzähler, und die Bereinigung läuft nur, wenn
Speicher fehlt.

<a id="d17"></a>
## D17 – Interpreter auf der HIR, LIR mit dem ersten Assembler-Backend

Ersetzt die Reihenfolge in [architecture.md](architecture.md) und der Roadmap: Der Referenz-Interpreter
führt zunächst die **HIR** aus. LIR und Lowering entstehen mit dem MC68000-Backend (0.4), wo sie
gebraucht werden; dann wird die LIR gleich passend für Registerzuteilung und Runtime-Aufrufe
entworfen. So laufen BASIC-Programme früher, und Frontend und Semantik sind gegen die Golden-Tests
geprüft, bevor das Lowering dazukommt. Sobald es die LIR gibt, läuft der Interpreter auch auf ihr
(Differenztest HIR ↔ LIR).

<a id="d16"></a>
## D16 – Ganzzahlausdrücke werden als Float typisiert

Das Original rechnet `+ - *` mit Ganzzahlen ohne 32-Bit-Überlauf (probe2 T1–T4); erst die Zuweisung
prüft den Bereich. Die HIR typisiert diese Operationen deshalb als Float. Das ist bitgenau: Summe und
Differenz zweier 32-Bit-Zahlen sind in binary64 exakt, ein Produkt ist das korrekt gerundete exakte
Produkt. `DIV`, `MOD`, `\` und die logischen Operatoren arbeiten auf 32-Bit-Ganzzahlen (Operanden
werden abgeschnitten).

Nachtrag (Optimierung der Assembler-Ziele, gleiche Ergebnisse): Geht `a+b`, `a-b` oder `a*b` zweier
Ganzzahlen (oder ganzzahliger Konstanten) direkt in eine Ganzzahl (`%`, `&`, `|`, `TOINT`), rechnet
das Lowering mit 32 Bit und Überlaufprüfung: Das exakte Ergebnis passt in 32 Bit oder die Umwandlung
schlägt fehl, mit dem Fehler der Zielbreite (2, 4, 3). Vergleiche zweier Ganzzahlen sind `CMP`.
`FOR` mit `%`-Variable, ganzzahligem Endwert und konstanter ganzzahliger Schrittweite zählt in 32 Bit;
ein Überlauf heißt, dass der exakte Wert hinter dem Ende liegt (die Variable behält den umgebrochenen
Wert). Ketten wie `a+b-c` bleiben beim Float-Weg, weil ein Zwischenergebnis überlaufen könnte.
`tests/programs/intfast.lst` prüft die Grenzen gegen den Interpreter, der immer den Float-Weg geht.

<a id="d15"></a>
## D15 – Dynamische Bindung: ein Speicherplatz je Variable

Laut Handbuch sind `LOCAL`-Variablen und Parameter auch in den aufgerufenen Routinen sichtbar. Jede
Variable (Name + Typ, Arrays getrennt) hat deshalb genau einen festen Speicherplatz. `LOCAL` und
Wertparameter sichern beim Eintritt den alten Wert und stellen ihn beim Verlassen wieder her
(„shallow binding“). Das passt zum Z80, weil alle Variablen statisch liegen, und erlaubt Rekursion.
`VAR`-Parameter binden den Parameternamen an die übergebene Variable (echte Referenz, am Original
bestätigt, probe10); `VAR` gilt für alle folgenden Parameter.

<a id="d14"></a>
## D14 – `PRINT USING` nach dem Abgleich mit dem Original

Ersetzt den Punkt `PRINT USING` in [D13](#d13). Am Original geprüft (probe7, probe8) und übernommen:

- **Rest des Formats:** Nach dem letzten Wert wird der Rest des Formats als Text ausgegeben, auch
  Felder (`USING "A## B## C",1` → `A 1 B## C`).
- **Kein passendes Feld:** Fehlt ein Feld oder passt seine Art nicht (Zahl in `&`, String in `##`),
  wird das Feld als Text ausgegeben, dann `?` und der Wert wie bei `PRINT` (`Summe:?5`, `&?1.5`,
  `##?x`).
- **Vorzeichen:** Ein negativer Wert behält sein Minus, auch wenn er auf 0 gerundet wird (`-0.00`).
  Im Exponentformat erscheint das Minus nur mit eigener Vorzeichenstelle (`-#.##^^^^`).
- **Exponent:** genau so viele Ziffern wie `^` minus 2; vordere Ziffern fallen weg
  (1E100 mit `^^^^` → `E+00`). Der Exponent ist ein Vielfaches k·⌊d/k⌋ der k Stellen vor dem Punkt;
  verursacht das Runden eine Stelle zu viel, wird einmal neu normalisiert (`#.##^^^^`, 9,999 →
  `1.00E+01`).
- **Leerer String mit `!`:** ein Leerzeichen.

Nicht nachgebildet (Eigenheiten nach [D11](#d11)):
- Bei 3 Stellen vor dem Punkt und |x| < 1 wählt das Original den Exponenten anders, ohne
  erkennbare Regel (0,5 → `5.00E-01`, 0,05 → `500.00E-04`); wir bleiben bei k·⌊d/k⌋.
- Formate ohne Stelle vor dem Punkt geben im Exponentformat falsche Werte aus (`.##^^^^`, 16 →
  `.16E+00`); wir geben `.16E+02` aus.

Die Wiederholung des Formats für weitere Werte ([D12](#d12)) bleibt.

<a id="d13"></a>
## D13 – Festlegungen der numerischen Referenz (0.1)

Mit `runtime/ref` festgelegt, wo weder Handbuch noch Original eine Antwort geben:

- **Unterlauf:** Das Ergebnis wird mit unbegrenztem Exponenten auf 53 Bit gerundet; ist es dann
  kleiner als 2⁻¹⁰²², wird es 0 (ohne Fehler, wie im Original).
- **`ROUND(x,n)`** mit |x·10ⁿ| ≥ 2⁵² liefert x unverändert (es gibt keine Stelle mehr zu runden; so
  kann auch x·10ⁿ nicht überlaufen). 10ⁿ entsteht durch fortgesetzte Multiplikation mit 10.
- **`VAL`** behält höchstens 800 signifikante Ziffern; weitere wirken nur als Rundungsinformation.
  Das ist exakt, weil jeder Mittelpunkt zwischen zwei binary64-Zahlen höchstens 767 Ziffern hat.
- **`DEFNUM`** über 13 zählt als 13.
- **`PRINT USING`** (ersetzt durch [D14](#d14)):
  - Ein Format ohne Feld gibt seinen Text und danach den Wert wie `PRINT` aus.
  - Eine Zahl in einem Stringfeld erscheint in der `PRINT`-Form, ein String in einem Zahlenfeld
    unverändert.
  - Nach dem letzten Wert wird der Text bis zum nächsten Feld bzw. zum Formatende ausgegeben.
  - Ein auf 0 gerundeter negativer Wert erscheint ohne Minus (`0.00`, nicht `-0.00`).
  - Braucht der Exponent mehr Ziffern als `^` vorhanden sind, wird er breiter (`1.00E+100`).
  - `**$##.##` füllt mit `*` bis zum `$`.

<a id="d12"></a>
## D12 – Ersatzverhalten für Eigenheiten von `INSTR` und `PRINT USING`

Ergänzt [D11](#d11) für Fälle, in denen das Original sich offensichtlich fehlerhaft verhält und wir
trotzdem ein Verhalten festlegen müssen (probe4, probe5):

| Fall | Original | Festlegung |
|---|---|---|
| `INSTR(a$,"",n)` | `INSTR("abc","",n)` für n = 1…5: 0, 1, 2, 4, 0 | n, solange 1 ≤ n ≤ `LEN(a$)`+1, sonst 0 (Startposition 0 zählt wie 1, negative → Fehler 10 wie im Original); `INSTR(a$,"")` = 1 |
| Format ohne Stelle vor dem Punkt | `USING ".##"`, 0,5 → `. 1` | korrekt formatiert: `.50` |
| mehrere Werte, ein Format | `USING "##.#",1.25,2.75` → ` 1.3?2.75` | Format für jeden Wert wiederholen: ` 1.3 2.8` |

<a id="d10"></a>
## D10 – Testplattformen als FPGA-Vorlage

`z80-test` und `m68k-test` sind nackte Prozessoren mit minimaler Ein-/Ausgabe. Sie dienen den
automatischen Tests und sind die Vorlage für Plattformen auf FPGA-Boards.

<a id="d21"></a>
## D21 – Gruppe `memory`, Stufe 1: Fenster statt Speicherabbild

Der volle Umbau nach [D6](#d6) (alle BASIC-Daten im Interpreter und C++-Ziel in einem Byte-Array) ist
groß. Als erste Stufe bekommt jede Variable, jeder String und jedes Array bei `VARPTR`/`V:` ein
**Fenster** in einem nachgebildeten Adressraum (`brt::Memory`): `PEEK`/`POKE`/`DPEEK`/`DPOKE`/
`LPEEK`/`LPOKE` und `BMOVE` darauf lesen und schreiben die Bytes des Werts im Amiga-Layout
(höchstwertiges Byte zuerst, IEEE binary64 für Fließkommazahlen, `%` 4, `&` 2, `|` und `!` 1 Byte).
`MALLOC`-Blöcke sind echte, gelöschte Bytes in diesem Raum; `MFREE` liefert 0, für eine Adresse, die
kein `MALLOC`-Block ist, -1. Auf den 68000-Zielen ist es echter Speicher (Amiga: `AllocMem`/
`FreeMem` mit `MEMF_CLEAR`).

- Die Adressen sind in den Ausführungen verschieden; Programme dürfen sich nur auf die Bytes und auf
  Abstände innerhalb eines Arrays verlassen.
- `V:a$` zeigt auf die Zeichen; hinter dem Stringende liest der Interpreter 0, der Amiga, was dort steht.
- Ein Zugriff außerhalb aller Fenster bricht Interpreter und C++-Ziel mit einer Meldung ab (auf dem
  Amiga wäre es ein Absturz oder liest Fremdes); `DPEEK`/`LPEEK` auf ungeraden Adressen ist auf dem
  68000 ein Adressfehler und bleibt unbestimmt.

Offen für die nächste Stufe: `BYTE{}`/`WORD{}`/`LONG{}`/`FLOAT{}`, `*x`, `ABSOLUTE`, `VARPTR` auf
den Descriptor statt die Zeichen, Adressen, die über Bereiche hinweg gelten.
