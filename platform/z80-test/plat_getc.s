;! module    plat_getc
;! provides  plat_getc plat_inkey plat_echo
;! group     core
;! clobbers  AF
;! doc       z80-test: the next byte of the keyboard input (port &03: 1 if one is left; port &02: the byte).

; -> A = character, CF set at the end of the input (INKEY$: the same, the input is all there)
plat_inkey:
plat_getc:
        IN      A,(&03)
        OR      A
        JR      Z,plat_getc__end
        IN      A,(&02)
        OR      A                       ; NC
        RET
plat_getc__end:
        SCF
        RET

; A = character of an input line: no echo on z80-test (the output holds only what the program prints)
plat_echo:
        RET
