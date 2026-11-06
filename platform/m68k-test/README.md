# platform/m68k-test

Nackter MC68000 für Emulator-Tests und als Vorlage für FPGA-Boards: Einsprung `$1000`, Stapel ab
`$80000`, `plat_putc` = `MOVE.B D0,$00FF0000`, `plat_exit` = `STOP #$2700`.
Siehe [docs/targets.md](../../docs/targets.md#z80-test-und-m68k-test).
