;! module    rt_ix
;! provides  rt_ixhl rt_ixde rt_ixbc
;! group     core
;! clobbers  AF
;! doc       Compiled code: the address of a frame slot, IX + d (d = the byte after the CALL, -128..-1).

; HL = IX + d (keeps BC, DE)
rt_ixhl:
        POP     HL
        LD      A,(HL)
        INC     HL
        PUSH    HL
        PUSH    IX
        POP     HL
        ADD     A,L
        LD      L,A
        RET     C                       ; d < 0: a carry means no borrow from H
        DEC     H
        RET

; DE = IX + d (keeps BC, HL)
rt_ixde:
        EX      (SP),HL
        LD      A,(HL)
        INC     HL
        EX      (SP),HL
        PUSH    IX
        POP     DE
        ADD     A,E
        LD      E,A
        RET     C
        DEC     D
        RET

; BC = IX + d (keeps DE, HL)
rt_ixbc:
        EX      (SP),HL
        LD      A,(HL)
        INC     HL
        EX      (SP),HL
        PUSH    IX
        POP     BC
        ADD     A,C
        LD      C,A
        RET     C
        DEC     B
        RET
