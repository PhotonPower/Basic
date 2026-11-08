;! module    rt_str_pack
;! provides  rt_str_pack
;! group     strings
;! clobbers  AF BC DE HL
;! doc       SELECT on a string: up to 4 characters packed big-endian into a 32-bit value.

; HL = result, DE = string descriptor
rt_str_pack:
        PUSH    HL
        EX      DE,HL
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        INC     HL
        LD      C,(HL)                  ; C = length (low byte), B = high byte
        INC     HL
        LD      B,(HL)
        LD      HL,0
        LD      (rt_str_pack__v),HL
        LD      (rt_str_pack__v+2),HL
        LD      A,B
        OR      A
        JR      NZ,rt_str_pack__4
        LD      A,C
        CP      5
        JR      C,rt_str_pack__n
rt_str_pack__4:
        LD      A,4
rt_str_pack__n:
        OR      A
        JR      Z,rt_str_pack__out
        LD      B,A
rt_str_pack__l:                         ; v = v << 8 | character
        LD      HL,(rt_str_pack__v+1)
        LD      (rt_str_pack__v+2),HL
        LD      A,(rt_str_pack__v)
        LD      (rt_str_pack__v+1),A
        LD      A,(DE)
        LD      (rt_str_pack__v),A
        INC     DE
        DJNZ    rt_str_pack__l
rt_str_pack__out:
        POP     DE
        LD      HL,rt_str_pack__v
        LD      BC,4
        LDIR
        RET

;! section bss
rt_str_pack__v:
        DS      4
