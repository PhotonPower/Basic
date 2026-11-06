# platform/amiga

Plattformschicht für den Amiga (MC68000): Start als AmigaDOS-Programm, `dos.library` öffnen,
`plat_putc` über `Output()`/`Write()` (gepuffert), später Fenster, Grafik und Ereignisse über
Intuition/Graphics. Siehe [docs/targets.md](../../docs/targets.md#amiga-mc68000).
