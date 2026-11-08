;! module    rt_strval
;! provides  rt_str_val rt_str_valq
;! requires  rt_val rt_val_used
;! group     float
;! clobbers  AF BC DE HL
;! doc       VAL(a$) and VAL?(a$): HL = result, DE = descriptor.

; HL = result (binary64), DE = descriptor
rt_str_val:
        PUSH    HL
        EX      DE,HL
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        INC     HL
        LD      C,(HL)
        INC     HL
        LD      B,(HL)                  ; DE = characters, BC = length
        POP     HL
        JP      rt_val

; HL = result (32 bits), DE = descriptor: the number of characters VAL reads
rt_str_valq:
        PUSH    HL
        LD      HL,rt_strval__x
        CALL    rt_str_val
        POP     HL
        LD      BC,(rt_val_used)
        LD      (HL),C
        INC     HL
        LD      (HL),B
        INC     HL
        LD      (HL),0
        INC     HL
        LD      (HL),0
        RET

;! section bss
rt_strval__x:
        DS      8
