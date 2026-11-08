;! module    plat_putc
;! provides  plat_putc plat_flush
;! group     core
;! clobbers  -
;! doc       cpc: a character through TXT OUTPUT (&BB5A); a line feed gets a carriage return in front.

plat_putc:
        CP      10
        JR      NZ,plat_putc__out
        PUSH    AF
        LD      A,(plat_putc__last)
        CP      13
        LD      A,13
        CALL    NZ,&BB5A                ; TXT OUTPUT (keeps all registers)
        POP     AF
plat_putc__out:
        LD      (plat_putc__last),A
        JP      &BB5A
plat_flush:
        RET

;! section data
plat_putc__last:
        DB      0
