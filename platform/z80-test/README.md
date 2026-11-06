# platform/z80-test

Nackter Z80 für Emulator-Tests und als Vorlage für FPGA-Boards: Einsprung `&4000`, Stapel ab `&FFF0`,
`plat_putc` = `OUT (&01),A`, `plat_exit` = `DI` + `HALT`.
Siehe [docs/targets.md](../../docs/targets.md#z80-test-und-m68k-test).
