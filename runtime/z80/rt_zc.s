;! module    rt_zc
;! provides  zc3 zc2 zc2d
;! group     core
;! clobbers  AF BC DE HL
;! doc       Compiled code: calls a runtime routine with operands in frame slots (DB d ... and DW routine after the CALL).

; The routine gets what a direct call would give it: DE = IX+a, BC = IX+b, HL = IX+d (the address
; of the result). It is entered by RET with the return address behind the operands on the stack, so
; it returns straight to the compiled code; flags and results are its own.

; CALL zc3 / DB a,b,d / DW routine: two operands and a numeric result
zc3:
        POP     HL                      ; -> a, b, d, routine
        CALL    rt_zc__adr
        LD      (rt_zc__a),BC
        CALL    rt_zc__adr
        PUSH    BC                      ; b
        CALL    rt_zc__adr              ; BC = d
        JR      rt_zc__go

; CALL zc2d / DB a,d / DW routine: one operand and a numeric result (BC = the result address too)
zc2d:
        POP     HL                      ; -> a, d, routine
        CALL    rt_zc__adr
        LD      (rt_zc__a),BC
        CALL    rt_zc__adr              ; BC = d
        PUSH    BC
rt_zc__go:                              ; (SP) = b, BC = d, HL -> routine
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        INC     HL                      ; DE = routine, HL = return address
        EX      (SP),HL                 ; HL = b, (SP) = return address
        PUSH    DE                      ; the routine on top: RET enters it
        PUSH    HL
        LD      H,B
        LD      L,C                     ; HL = d
        POP     BC                      ; BC = b
        LD      DE,(rt_zc__a)           ; DE = a
        RET

; CALL zc2 / DB a,b / DW routine: two operands, no numeric result
zc2:
        POP     HL                      ; -> a, b, routine
        CALL    rt_zc__adr
        LD      (rt_zc__a),BC
        CALL    rt_zc__adr              ; BC = b
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        INC     HL
        PUSH    HL                      ; return address
        PUSH    DE                      ; the routine
        LD      DE,(rt_zc__a)
        RET

; BC = IX + d (d = the byte at HL, -128..-1), HL advanced; keeps DE
rt_zc__adr:
        LD      A,(HL)
        INC     HL
        PUSH    HL
        PUSH    IX
        POP     HL
        ADD     A,L
        LD      C,A
        LD      A,H
        JR      C,rt_zc__nb             ; a carry means no borrow from H
        DEC     A
rt_zc__nb:
        LD      B,A
        POP     HL
        RET

;! section bss
rt_zc__a:
        DS      2
