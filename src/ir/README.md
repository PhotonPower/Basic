# src/ir

HIR, LIR, Lowering HIR → LIR, Textausgabe (`--emit-hir`, `--emit-lir`) und der IR-Interpreter.
Spezifikation: [docs/ir.md](../../docs/ir.md).

| Datei | Inhalt |
|---|---|
| `hir.hpp`, `hir.cpp` | HIR (typisierter, strukturierter Baum) und ihre Textform |
| `interp.*` | Referenz-Interpreter auf der HIR (`basicc --run`); Laufzeitfunktionen aus `platform/pc` |

LIR und Lowering kommen mit dem MC68000-Backend ([D17](../../docs/decisions.md#d17)).
