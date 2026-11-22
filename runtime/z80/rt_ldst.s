;! module    rt_ldst
;! provides  rt_ld8 rt_ld4 rt_st8 rt_st4
;! group     core
;! clobbers  AF BC DE HL
;! doc       Compiled code: copies a variable (Float, Int) into a frame slot and back (DB d, DW v after the CALL).

; CALL rt_ld8 / DB d / DW v: the 8 bytes of v into the slot IX+d (rt_ld4: 4 bytes)
rt_ld8:
        LD      C,8
        JR      rt_ldst__ld
rt_ld4:
        LD      C,4
rt_ldst__ld:
        CALL    rt_ldst__op             ; HL = v (source), DE = IX+d (destination)
        LD      B,0
        LDIR
        RET

; CALL rt_st8 / DB d / DW v: the 8 bytes of the slot IX+d into v (rt_st4: 4 bytes)
rt_st8:
        LD      C,8
        JR      rt_ldst__st
rt_st4:
        LD      C,4
rt_ldst__st:
        CALL    rt_ldst__op             ; HL = v, DE = IX+d
        EX      DE,HL                   ; source the slot, destination v
        LD      B,0
        LDIR
        RET

; the operands after the CALL of the caller of rt_ld8/rt_st8 (its return address is at 2(SP)):
; HL = v, DE = IX+d, the return address advanced by 3; keeps C
rt_ldst__op:
        POP     DE                      ; own return
        POP     HL                      ; -> d, v
        LD      A,(HL)
        INC     HL
        PUSH    DE                      ; own return back
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        INC     HL
        EX      (SP),HL                 ; HL = own return, (SP) = the caller's return address
        PUSH    HL                      ; own return on top again
        PUSH    DE                      ; v
        PUSH    IX
        POP     DE
        ADD     A,E
        LD      E,A
        JR      C,rt_ldst__nb           ; d < 0: a carry means no borrow from D
        DEC     D
rt_ldst__nb:
        POP     HL                      ; HL = v, DE = IX+d
        RET
