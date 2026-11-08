;! module    fp_canon
;! provides  fp_canon fp_abs
;! requires  fp_zero8 rt_error
;! group     float
;! clobbers  AF BC DE HL
;! doc       fp_canon: a binary64 from memory (denormal, -0 -> 0; Inf/NaN -> error 1); fp_abs. HL = destination, DE = a.

fp_canon:
        PUSH    HL
        EX      DE,HL                   ; HL = a, DE = destination
        LD      BC,8
        LDIR
        POP     HL
        CALL    fp_canon__be            ; DE = be
        LD      A,D
        OR      E
        JR      Z,fp_canon__zero
        LD      A,E                     ; be = &7FF: overflow
        CP      &FF
        RET     NZ
        LD      A,D
        CP      7
        RET     NZ
        CALL    fp_zero8
        LD      A,1
        JP      rt_error
fp_canon__zero:
        JP      fp_zero8

; HL = destination, DE = a: |a|
fp_abs:
        PUSH    HL
        EX      DE,HL
        LD      BC,8
        LDIR
        DEC     DE
        LD      A,(DE)
        AND     &7F
        LD      (DE),A
        POP     HL
        RET

; HL = binary64 -> DE = be (keeps HL)
fp_canon__be:
        PUSH    HL
        LD      BC,6
        ADD     HL,BC
        LD      A,(HL)
        INC     HL
        LD      D,(HL)
        LD      E,A
        RES     7,D
        LD      B,4
fp_canon__s:
        SRL     D
        RR      E
        DJNZ    fp_canon__s
        POP     HL
        RET
