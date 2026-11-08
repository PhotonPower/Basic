;! module    plat_timer
;! provides  plat_timer
;! group     core
;! clobbers  AF BC DE HL
;! doc       z80-test: TIMER without a clock: counts up by one on every call (deterministic, never stands).

; HL = result: TIMER
plat_timer:
        PUSH    HL
        LD      HL,plat_timer__n
        LD      B,4
plat_timer__inc:
        INC     (HL)
        JR      NZ,plat_timer__out
        INC     HL
        DJNZ    plat_timer__inc
plat_timer__out:
        POP     DE
        LD      HL,plat_timer__n
        LD      BC,4
        LDIR
        RET

;! section bss
plat_timer__n:
        DS      4
