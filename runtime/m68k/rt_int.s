;! module    rt_int
;! provides  rt_idiv rt_imod rt_imul rt_conv_word rt_conv_byte rt_wrap_word rt_wrap_byte rt_iadd_chk rt_isub_chk rt_imul_chk
;! provides  rt_imul_v
;! requires  rt_error
;! group     core
;! clobbers  D0-D3/A0-A1
;! doc       32-bit integer helpers: DIV/MOD (error 0), multiplication, range checks (errors 4, 3), wrap.

; rt_idiv, rt_imod, rt_imul: D0 = a, D1 = b -> D0.  Others: D0 -> D0.

rt_idiv:
        BSR     rt_int__divmod
        RTS

rt_imod:
        BSR     rt_int__divmod
        MOVE.L  D1,D0
        RTS

; D0 / D1 truncated towards 0 -> D0 = quotient, D1 = remainder (sign of the dividend)
rt_int__divmod:
        TST.L   D1
        BNE     .ok
        MOVEQ   #0,D0                   ; RT_ERR_DIV_ZERO
        JSR     rt_error
        MOVEQ   #0,D0
        MOVEQ   #0,D1
        RTS
.ok:    MOVEM.L D4-D6,-(SP)
        MOVE.L  D0,D4                   ; D4 = sign of the dividend, D5 = sign of the quotient
        MOVE.L  D0,D5
        EOR.L   D1,D5
        TST.L   D0
        BPL     .a
        NEG.L   D0
.a:     TST.L   D1
        BPL     .b
        NEG.L   D1
.b:     MOVEQ   #0,D2                   ; unsigned D0 / D1: quotient D3, remainder D2
        MOVEQ   #0,D3
        MOVEQ   #31,D6
.loop:  ADD.L   D0,D0
        ADDX.L  D2,D2
        ADD.L   D3,D3
        CMP.L   D1,D2
        BCS     .next
        SUB.L   D1,D2
        ADDQ.L  #1,D3
.next:  DBRA    D6,.loop
        MOVE.L  D3,D0
        MOVE.L  D2,D1
        TST.L   D5
        BPL     .qs
        NEG.L   D0
.qs:    TST.L   D4
        BPL     .rs
        NEG.L   D1
.rs:    MOVEM.L (SP)+,D4-D6
        RTS

; low 32 bits of a * b (wraps)
rt_imul:
        MOVE.L  D0,D2
        MULU    D1,D2                   ; al * bl
        MOVE.L  D0,D3
        SWAP    D3
        MULU    D1,D3                   ; ah * bl
        SWAP    D1
        MULU    D1,D0                   ; al * bh
        ADD.L   D3,D0
        SWAP    D0
        CLR.W   D0
        ADD.L   D2,D0
        RTS

; D0 = a, D1 = b, D2 = error number -> D0 = a + b, a - b, a * b; outside 32 bits: the error. The
; same as computing in float and converting (D16): the exact result fits 32 bits or not.
rt_iadd_chk:
        ADD.L   D1,D0
        BVS     rt_int__ovf
        RTS

rt_isub_chk:
        SUB.L   D1,D0
        BVS     rt_int__ovf
        RTS

rt_int__ovf:
        MOVE.L  D2,D0
        JSR     rt_error
        MOVEQ   #0,D0
        RTS

rt_imul_chk:
        BSR     rt_imul_v
        BVS     rt_int__ovf
        RTS

; D0 = a, D1 = b -> D0 = a * b, V set if it does not fit 32 bits (D2 is kept)
; signed 32 x 32 with overflow: |a| * |b| from 16-bit products, then the sign
rt_imul_v:
        MOVEM.L D3-D5,-(SP)
        MOVE.L  D0,D5
        EOR.L   D1,D5                   ; D5 bit 31 = sign of the result
        TST.L   D0
        BPL     .a
        NEG.L   D0
.a:     TST.L   D1
        BPL     .b
        NEG.L   D1
.b:     MOVE.L  D0,D3                   ; both high words set: too large
        SWAP    D3
        MOVE.L  D1,D4
        SWAP    D4
        TST.W   D3
        BEQ     .one
        TST.W   D4
        BNE     .ovf
        EXG     D0,D1                   ; make D0 the one with the high word set... (D1 = it now)
        EXG     D3,D4
.one:   MOVE.L  D0,D3                   ; D0 = x (high word 0), D1 = y: x*y = x*yl + (x*yh << 16)
        MULU    D1,D3                   ; x * yl
        MOVE.L  D1,D4
        SWAP    D4
        MULU    D0,D4                   ; x * yh
        CMP.L   #$FFFF,D4
        BHI     .ovf
        SWAP    D4
        CLR.W   D4
        ADD.L   D4,D3
        BCS     .ovf
        TST.L   D5                      ; sign: positive results up to 2^31-1, negative down to -2^31
        BMI     .neg
        TST.L   D3
        BMI     .ovf
        MOVE.L  D3,D0
        BRA     .out
.neg:   CMP.L   #$80000000,D3
        BHI     .ovf
        NEG.L   D3
        MOVE.L  D3,D0
.out:   MOVEM.L (SP)+,D3-D5           ; MOVEM keeps the flags: V clear from the MOVE
        RTS
.ovf:   MOVEM.L (SP)+,D3-D5
        ORI     #$02,CCR                ; V
        RTS

rt_conv_word:
        CMP.L   #-32768,D0
        BLT     .err
        CMP.L   #32767,D0
        BGT     .err
        RTS
.err:   MOVEQ   #4,D0                   ; RT_ERR_NO_WORD
        JSR     rt_error
        MOVEQ   #0,D0
        RTS

rt_conv_byte:
        CMP.L   #255,D0
        BHI     .err
        RTS
.err:   MOVEQ   #3,D0                   ; RT_ERR_NO_BYTE
        JSR     rt_error
        MOVEQ   #0,D0
        RTS

rt_wrap_word:
        EXT.L   D0
        RTS

rt_wrap_byte:
        AND.L   #$FF,D0
        RTS
