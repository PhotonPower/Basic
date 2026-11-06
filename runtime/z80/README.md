# runtime/z80

Runtime-Module für den Z80, eine Routine je Datei mit Modulkopf. Interne Labels heißen
`<modul>__<name>` (der Assembler kennt keine lokalen Labels); Daten- und BSS-Blöcke werden mit
`;! section data` / `;! section bss` gekennzeichnet.
Siehe [docs/runtime.md](../../docs/runtime.md#modulformat). Noch leer (Meilenstein 0.5).
