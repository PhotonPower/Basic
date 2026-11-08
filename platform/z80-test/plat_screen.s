;! module    plat_screen
;! provides  plat_locate plat_cls
;! requires  plat_putc
;! group     print
;! clobbers  AF BC DE HL
;! doc       z80-test: cursor and CLS as ANSI sequences on the console (as the Amiga console).

; DE = column, HL = line (1..32767): ESC [ line ; column H
plat_locate:
        PUSH    DE
        LD      A,27
        CALL    plat_putc
        LD      A,'['
        CALL    plat_putc
        CALL    plat_screen__num
        LD      A,';'
        CALL    plat_putc
        POP     HL
        CALL    plat_screen__num
        LD      A,'H'
        JP      plat_putc

; CLS: ESC [ H ESC [ J
plat_cls:
        LD      HL,plat_screen__cls
        LD      B,6
plat_screen__c:
        LD      A,(HL)
        CALL    plat_putc
        INC     HL
        DJNZ    plat_screen__c
        RET

; HL = 1..65535: its decimal digits
plat_screen__num:
        LD      B,0                     ; B = 1 after the first digit
        LD      DE,10000
        CALL    plat_screen__dig
        LD      DE,1000
        CALL    plat_screen__dig
        LD      DE,100
        CALL    plat_screen__dig
        LD      DE,10
        CALL    plat_screen__dig
        LD      A,L
        ADD     A,'0'
        JP      plat_putc
plat_screen__dig:                       ; A = HL / DE, HL = remainder
        LD      A,'0'-1
        OR      A
plat_screen__d1:
        INC     A
        SBC     HL,DE
        JR      NC,plat_screen__d1
        ADD     HL,DE
        CP      '0'
        JR      NZ,plat_screen__d2
        INC     B
        DEC     B
        RET     Z                       ; no leading zeros
plat_screen__d2:
        LD      B,1
        JP      plat_putc

;! section data
plat_screen__cls:
        DB      27,"[H",27,"[J"
