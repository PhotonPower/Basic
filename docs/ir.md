# Zwischencode (HIR und LIR)

> **Entwurf.** Die Operationen werden mit dem ersten Backend festgelegt; Änderungen gehören in
> [decisions.md](decisions.md).

## Typen

| IR-Typ | GFA-Typ | Postfix | Größe | Bemerkung |
|---|---|---|---|---|
| `b1` | Boolean | `!` | 1 Byte | Werte 0 / -1 (TRUE) |
| `u8` | Byte | `\|` | 1 Byte | 0…255 |
| `i16` | Word | `&` | 2 Byte | vorzeichenbehaftet |
| `i32` | Integer | `%` | 4 Byte | vorzeichenbehaftet, Zweierkomplement |
| `f64` | Float | `#` / keins | 8 Byte | Format und Semantik: [numerics.md](numerics.md) |
| `str` | String | `$` | Deskriptor | Länge 16 Bit, Speicher im String-Heap |
| `ptr` | Adresse | – | 4 Byte (Z80: 2) | nur intern und für `VARPTR`, `*x`, `MALLOC` |

Arrays sind Deskriptoren (Typ, Dimensionen, Grenzen, Datenzeiger). Ihr Layout ist Teil der
Runtime-Schnittstelle, weil `*a()`, `SWAP a(),b()`, `TYPE()` und `VAR a()` darauf zugreifen.

## HIR

> Umgesetzt in `src/ir/hir.hpp`, erzeugt von `src/frontend/sema.cpp`, Textform mit
> `basicc --emit-hir`. Variablen nach [D15](decisions.md#d15), Ganzzahlausdrücke nach
> [D16](decisions.md#d16).

Die HIR ist ein typisierter Baum je Prozedur. Sie entspricht weitgehend dem AST, aber mit
aufgelösten Namen, festen Typen, expliziten Konvertierungen und vereinheitlichten Aliasen:

```
proc main
  local i%: i32
  for i% = 1 to 10 step 1            ; FOR/NEXT mit GFA-Semantik der Grenzen
    print [ (conv f64 i%) , ";" ]
  next
  print_nl
end
```

Knoten: `proc`, `func`, `local`, `assign`, `if/elseif/else`, `select/case/default`, `for`, `while`,
`repeat/until`, `do/loop`, `exit_if`, `goto`, `label`, `gosub`/`call`, `return`, `on_error`,
`resume`, `data`/`read`/`restore` und Ausdrucksknoten (Operatoren, Funktionsaufrufe, `rtcall`).

Das C++-Backend arbeitet auf der HIR, damit der erzeugte Code die Struktur des BASIC-Programms behält.

`MAT`-Befehle ([D23](decisions.md#d23)) sind `cmd "MAT …"`; Felder sind `Load` der Feldvariablen,
mit Versatz-Indizes als `args`, Skalarziele `Load`/`LoadElem`:

| Befehl | `mode` | `targets` | `args` |
|---|---|---|---|
| `MAT BASE n` | | | n |
| `MAT CLR/ONE/READ a()` | | a | |
| `MAT SET a()=x` | | a | x |
| `MAT PRINT [#c,]a()[,g,n]` (`channel`) | | a | [g, n] |
| `MAT CPY/XCPY a([i,j])=b([k,l])[,h,w]` | | a, b | [h, w] |
| `MAT TRANS a()[=b()]`, `MAT INV b()=a()` | | a [, b] | |
| `MAT ADD/SUB` | 0 `a()=b()±c()`, 1 `a(),b()`, 2 `a(),x` | a, b, c / a, b / a | x (2) |
| `MAT MUL` | 0 `a()=b()*c()`, 1 `x=a()*b()`, 2 `x=a()*b()*c()`, 3 `a(),x` | … (Skalar zuerst) | x (3) |
| `MAT NORM a(),k` | | a | k |
| `MAT DET/QDET/RANG x=a([i,j])[,n]` | | x, a | [n] |

## LIR

> Umgesetzt in `src/ir/lir.hpp` (Textform `basicc --emit-lir`), Lowering in `src/ir/lower.cpp`.
> Erster Umfang: Zahlen, Kontrollfluss, Prozeduren und Funktionen mit Wertparametern, `LOCAL`,
> `PRINT` von Zahlen und Stringkonstanten. Variablen laden/speichern (`ld`/`st`), Ganzzahl-`Op`
> und `Cmp`, alles andere als `rtcall`; `save`/`clear`/`restore` für `LOCAL` und Parameter (D15).

Drei-Adress-Code in Basisblöcken mit unbegrenzt vielen typisierten virtuellen Registern (`t1`, `t2`, …)
und benannten Variablen. Textform (für `basicc --emit-lir` und Tests):

```
; a% = b% + c% * 2
t1 = mul.i32  c%, 2
t2 = add.i32  b%, t1
     st.i32   a%, t2

; a$ = LEFT$(b$,3) + c$
t3 = rtcall rt_left$    b$, 3        [string]
t4 = rtcall rt_concat$  t3, c$       [string]
     rtcall rt_sassign  a$, t4       [string]

; IF x# > 1.5 THEN ...
t5 = cmp.gt.f64  x#, 1.5             ; Ergebnis i32: -1 oder 0
     br.nz t5, L1, L2
```

### Operationen

| Gruppe | Operationen |
|---|---|
| Konstanten, Kopien | `const`, `mov` |
| Arithmetik | `add sub mul div mod neg` (`div`/`mod` ganzzahlig), `fdiv` (Float) – jeweils mit Typ |
| Bitoperationen | `and or xor not imp eqv shl shr rol ror` mit Breite `.8/.16/.32` |
| Vergleiche | `cmp.eq/ne/lt/le/gt/ge.<typ>` → `i32` (-1/0); `cmp.approx.f64` für `==` |
| Konvertierungen | `conv.<von>.<nach>` mit fester Rundungsart (`trunc`, `floor`, `round`) |
| Speicher | `ld`/`st` auf Variable, Array-Element (mit Indexprüfung), Adresse (`PEEK`/`POKE`, `BYTE{}` …) |
| Kontrollfluss | `br`, `br.z`, `br.nz`, `switch`, `call`, `ret`, `gosub`, `return` |
| Runtime | `rtcall <symbol> args… [gruppe]` |
| Fehler | `onerror`, `resume`, `resume.next`, `resume.label`, `error n` |
| Metadaten | `stmt <zeile>` (Anweisungsgrenze), `loc <datei>:<zeile>` |

### Regeln

- **Typen sind explizit.** Es gibt keine implizite Konvertierung im LIR; das Frontend setzt `conv` ein.
  Byte- und Word-Variablen bleiben `u8`/`i16`, damit das Z80-Backend kurzen Code erzeugen kann.
- **Runtime-Aufrufe bleiben Aufrufe.** `rtcall` trägt das Runtime-Symbol und die
  [Befehlsgruppe](language.md#befehlsgruppen). Ein Backend darf einen Aufruf inline erzeugen
  (z. B. `rt_mul32` als `MULS` auf dem 68000, wenn beide Operanden 16 Bit sind), muss aber dasselbe
  Ergebnis liefern.
- **Anweisungsgrenzen sind markiert.** `stmt` steht vor jeder BASIC-Anweisung. `RESUME` und
  `RESUME NEXT` setzen an solchen Grenzen wieder auf, und die Fehlermeldung kann die Zeile nennen.
  Optimierungen dürfen Operationen nicht über eine `stmt`-Grenze hinweg verschieben, wenn dazwischen
  ein Fehler ausgelöst werden kann.
- **Ganzzahl-Überlauf** ist eine Eigenschaft der Operation: `add.i32` (mit Überlaufprüfung, wo GFA
  prüft) und `add.wrap.i32` (ohne Prüfung, z. B. für `INC`, `ADD x,y`, `ADD()`), siehe
  [numerics.md](numerics.md#ganzzahlen).

## Lowering HIR → LIR

- Schleifen werden zu Basisblöcken; `FOR` berechnet Endwert und Schrittweite einmal vorab
  (GFA-Semantik in [language.md](language.md#offene-sprachfragen) zu prüfen).
- `SELECT` wird zu einer Vergleichskette oder `switch`.
- Ausdrücke werden in Auswertungsreihenfolge linearisiert; String-Zwischenergebnisse bekommen
  Freigabepunkte für den String-Heap.
- Prozeduren mit `LOCAL` und Rekursion bekommen einen Stapelrahmen; `VAR`-Parameter werden zu
  Zeigern.

## Optimierungen (später)

Konstantenfaltung (mit `runtime/ref`), Weitergabe von Kopien, Entfernen toten Codes, Einengen von
Wertebereichen (z. B. Schleifenzähler mit konstanten Grenzen als `i16` auf dem Z80) und
Peephole-Optimierungen im jeweiligen Backend. Jede Optimierung muss die Differenztests bestehen.
