# runtime/ref

C-Referenzimplementierung der Runtime in portablem C99, nur Ganzzahlarithmetik (kein `float`/`double`,
kein `libm`). Sie **definiert die Semantik**: Das PC-Ziel benutzt sie direkt, der Compiler faltet
Konstanten damit, und die Assembler-Ports werden bitgenau gegen sie getestet.
Siehe [docs/numerics.md](../../docs/numerics.md) und [docs/runtime.md](../../docs/runtime.md).
Modulkopf in C: `//! module …`, `//! provides …` usw. Noch leer (Meilenstein 0.1).
