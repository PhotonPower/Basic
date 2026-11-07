;! module    rt_int
;! provides  rt_idiv rt_imod rt_imul rt_conv_word rt_conv_byte rt_wrap_word rt_wrap_byte
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
