;! module    plat_timer
;! provides  plat_timer
;! group     core
;! clobbers  AF BC DE HL
;! doc       cpc: TIMER in 1/200 s from KL TIME PLEASE (&BD0D, 1/300 s): time * 2 / 3.

; HL = result: TIMER
plat_timer:
        PUSH    HL
        CALL    &BD0D                   ; KL TIME PLEASE: DEHL = time in 1/300 s
        LD      (plat_timer__t),HL
        LD      (plat_timer__t+2),DE
        LD      HL,plat_timer__t        ; * 2
        SLA     (HL)
        INC     HL
        RL      (HL)
        INC     HL
        RL      (HL)
        INC     HL
        RL      (HL)
        LD      B,32                    ; / 3 (restoring division, bit by bit)
        LD      C,0                     ; C = remainder
plat_timer__d:
        LD      HL,plat_timer__t
        SLA     (HL)
        INC     HL
        RL      (HL)
        INC     HL
        RL      (HL)
        INC     HL
        RL      (HL)
        RL      C
        LD      A,C
        SUB     3
        JR      C,plat_timer__n
        LD      C,A
        LD      HL,plat_timer__t
        INC     (HL)                    ; quotient bit (bit 0 is free after the shift)
plat_timer__n:
        DJNZ    plat_timer__d
        POP     DE
        LD      HL,plat_timer__t
        LD      BC,4
        LDIR
        RET

;! section bss
plat_timer__t:
        DS      4
