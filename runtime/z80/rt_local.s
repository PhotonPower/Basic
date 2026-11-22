;! module    rt_local
;! provides  rt_lsave rt_lrest rt_zero8
;! group     core
;! clobbers  AF BC DE HL
;! doc       Compiled code: LOCAL and parameters, the 8 bytes of a variable on the stack (address in a DW after the CALL).

; CALL rt_lsave / DW v: pushes the words v+6, v+4, v+2, v+0 (v+0 on top), as PUSHes in the caller would
rt_lsave:
        POP     BC                      ; -> DW v
        LD      A,(BC)
        LD      L,A
        INC     BC
        LD      A,(BC)
        LD      H,A
        INC     BC                      ; BC = return address
        LD      DE,7
        ADD     HL,DE                   ; HL -> v+7 (unrolled: LOCAL is on the path of every call)
        LD      D,(HL)
        DEC     HL
        LD      E,(HL)
        DEC     HL
        PUSH    DE
        LD      D,(HL)
        DEC     HL
        LD      E,(HL)
        DEC     HL
        PUSH    DE
        LD      D,(HL)
        DEC     HL
        LD      E,(HL)
        DEC     HL
        PUSH    DE
        LD      D,(HL)
        DEC     HL
        LD      E,(HL)
        PUSH    DE
        PUSH    BC
        RET

; CALL rt_lrest / DW v: pops the words saved by rt_lsave back into v
rt_lrest:
        POP     BC                      ; -> DW v
        LD      A,(BC)
        LD      L,A
        INC     BC
        LD      A,(BC)
        LD      H,A
        INC     BC                      ; BC = return address
        POP     DE
        LD      (HL),E
        INC     HL
        LD      (HL),D
        INC     HL
        POP     DE
        LD      (HL),E
        INC     HL
        LD      (HL),D
        INC     HL
        POP     DE
        LD      (HL),E
        INC     HL
        LD      (HL),D
        INC     HL
        POP     DE
        LD      (HL),E
        INC     HL
        LD      (HL),D
        PUSH    BC
        RET

; HL -> 8 bytes, all set to 0 (LOCAL)
rt_zero8:
        XOR     A
        LD      (HL),A
        INC     HL
        LD      (HL),A
        INC     HL
        LD      (HL),A
        INC     HL
        LD      (HL),A
        INC     HL
        LD      (HL),A
        INC     HL
        LD      (HL),A
        INC     HL
        LD      (HL),A
        INC     HL
        LD      (HL),A
        RET
