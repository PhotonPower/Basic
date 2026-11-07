# src/backend/m68k

MC68000-Backend: LIR → Motorola-Assembler für `m68kcore` (Hunk-Programme über `SECTION`).
Aufrufkonvention: [docs/runtime.md](../../../docs/runtime.md#aufrufkonventionen). Noch leer (Meilenstein 0.4).

Stand: `m68k_backend.*` erzeugt aus der LIR Assembler für den Assembler des Schwesterprojekts.
Jedes virtuelle Register liegt in einem 8-Byte-Platz des Stapelrahmens (`LINK A6`); Runtime-Aufrufe
bekommen ihre Argumente in D0–D3 (ein `f64` belegt ein Paar), eigene Prozeduren über den Stapel.
Variablen liegen im BSS (8 Byte je Variable). Registerzuteilung folgt später.
`basicc --target m68k-test programm.lst -o programm.s` liefert den gelinkten Quelltext.
