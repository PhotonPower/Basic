;! module    plat_putc
;! provides  plat_putc
;! group     core
;! clobbers  -
;! doc       m68k-test: writes the character in D0 to the console port $00FF0000.

plat_putc:
        MOVE.B  D0,$00FF0000
        RTS
