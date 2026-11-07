# platform/m68k-test

Nackter MC68000 für Emulator-Tests und als Vorlage für FPGA-Boards ([docs/targets.md](../../docs/targets.md)):
Einsprung `$1000`, Stapel ab `$80000` abwärts, `plat_putc` schreibt auf `$00FF0000`, `plat_exit`
ist `STOP #$2700`.
