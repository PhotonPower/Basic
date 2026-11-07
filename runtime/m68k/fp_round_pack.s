;! module    fp_round_pack
;! provides  fp_round_pack fp_finish
;! requires  rt_error
;! group     float
;! clobbers  D0-D3/A0-A1
;! doc       Rounds m * 2^e to binary64, ties to even (runtime/ref/fp_round_pack.c).

; In:  D0:D1 = m (64 bit, may be 0), D2.L = e, bit 31 of D3 = sign
; Out: D0:D1 = result; fp_round_pack: D2.L = 1 on overflow (result 0), else 0.
;      fp_finish raises error 1 (overflow) through rt_error instead.

fp_finish:
        BSR     fp_round_pack
        TST.L   D2
        BEQ     .ok
        MOVEQ   #1,D0                   ; RT_ERR_OVERFLOW
        JSR     rt_error
        MOVEQ   #0,D0
        MOVEQ   #0,D1
.ok:    RTS

fp_round_pack:
        MOVEM.L D4-D6,-(SP)
        MOVE.L  D0,D4
        OR.L    D1,D4
        BEQ     .zero
        TST.L   D0                      ; normalise: leading bit to bit 63 (exact)
        BNE     .norm
        MOVE.L  D1,D0
        MOVEQ   #0,D1
        SUB.L   #32,D2
.norm:  TST.L   D0
        BMI     .normed
        ADD.L   D1,D1
        ADDX.L  D0,D0
        SUBQ.L  #1,D2
        BRA     .norm
.normed:
        MOVE.L  D1,D4                   ; D4 = the 11 bits below the 53 kept ones
        AND.L   #$7FF,D4
        MOVEQ   #11,D5                  ; m >>= 11
        LSR.L   D5,D1
        MOVE.L  D0,D6
        MOVEQ   #21,D5
        LSL.L   D5,D6
        OR.L    D6,D1
        MOVEQ   #11,D5
        LSR.L   D5,D0
        ADD.L   #11,D2
        CMP.W   #$400,D4                ; round to nearest, ties to even
        BHI     .up
        BNE     .rounded
        BTST    #0,D1
        BEQ     .rounded
.up:    ADDQ.L  #1,D1
        BCC     .nocarry
        ADDQ.L  #1,D0
.nocarry:
        CMP.L   #$00200000,D0           ; m = 2^53: one more exponent step
        BNE     .rounded
        LSR.L   #1,D0
        ADDQ.L  #1,D2
.rounded:
        ADD.L   #1075,D2                ; biased exponent
        CMP.L   #2047,D2
        BGE     .overflow
        TST.L   D2
        BLE     .zero                   ; below 2^-1022: 0 (no denormals)
        AND.L   #$000FFFFF,D0
        MOVEQ   #20,D5
        LSL.L   D5,D2
        OR.L    D2,D0
        TST.L   D3
        BPL     .pos
        BSET    #31,D0
.pos:   MOVEQ   #0,D2
        MOVEM.L (SP)+,D4-D6
        RTS
.overflow:
        MOVEQ   #1,D2
        BRA     .clear
.zero:  MOVEQ   #0,D2
.clear: MOVEQ   #0,D0
        MOVEQ   #0,D1
        MOVEM.L (SP)+,D4-D6
        RTS
