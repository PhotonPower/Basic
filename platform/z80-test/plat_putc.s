;! module    plat_putc
;! provides  plat_putc plat_flush
;! group     core
;! clobbers  -
;! doc       z80-test: A = character to the console port &01 (all registers are kept).

plat_putc:
        OUT     (&01),A
plat_flush:
        RET
