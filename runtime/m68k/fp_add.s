;! module    fp_add
;! provides  fp_add fp_sub fp_neg
;! requires  fp_finish
;! group     float
;! clobbers  D0-D3/A0-A1
;! doc       Addition and subtraction, correctly rounded (ties to even); negation (runtime/ref/fp_add.c).

; fp_add, fp_sub: D0:D1 = a, D2:D3 = b -> D0:D1.  fp_neg: D0:D1 -> D0:D1.

fp_neg:
        MOVE.L  D0,-(SP)
        AND.L   #$7FF00000,D0
        BEQ     .zero                   ; 0 stays +0
        MOVE.L  (SP)+,D0
        BCHG    #31,D0
        RTS
.zero:  ADDQ.L  #4,SP
        MOVEQ   #0,D1
        RTS

fp_sub:
        BCHG    #31,D2                  ; a - b = a + (-b); a zero b stays zero for fp_add

fp_add:
        MOVEM.L D4-D7,-(SP)
        MOVE.L  D2,D4
        AND.L   #$7FF00000,D4
        BNE     .bnz
        MOVE.L  D0,D4                   ; b = 0: the result is a, or +0
        AND.L   #$7FF00000,D4
        BNE     .done
        MOVEQ   #0,D0
        MOVEQ   #0,D1
        BRA     .done
.bnz:   MOVE.L  D0,D4
        AND.L   #$7FF00000,D4
        BNE     .anz
        MOVE.L  D2,D0                   ; a = 0: the result is b
        MOVE.L  D3,D1
        BRA     .done
.anz:   MOVE.L  D0,D4                   ; remember whether the signs differ
        EOR.L   D2,D4
        MOVE.L  D4,-(SP)
        MOVE.L  D0,D4                   ; order: |x| >= |y|
        BCLR    #31,D4
        MOVE.L  D2,D5
        BCLR    #31,D5
        CMP.L   D5,D4
        BHI     .ordered
        BCS     .swap
        CMP.L   D3,D1
        BCC     .ordered
.swap:  EXG     D0,D2
        EXG     D1,D3
.ordered:
        MOVE.L  D0,A1                   ; A1: sign of the result (bit 31)
        MOVE.L  D0,D6                   ; D6 = biased exponent of x
        SWAP    D6
        LSR.W   #4,D6
        AND.L   #$7FF,D6
        MOVE.L  D2,D7                   ; D7 = biased exponent of y
        SWAP    D7
        LSR.W   #4,D7
        AND.L   #$7FF,D7
        MOVE.L  D6,D4                   ; A0 = e = ex - 1075 - 10
        SUB.L   #1085,D4
        MOVE.L  D4,A0
        AND.L   #$000FFFFF,D0           ; significands with the leading bit at 62
        BSET    #20,D0
        MOVE.L  D1,D4
        MOVEQ   #22,D5
        LSR.L   D5,D4
        MOVEQ   #10,D5
        LSL.L   D5,D0
        OR.L    D4,D0
        LSL.L   D5,D1
        AND.L   #$000FFFFF,D2
        BSET    #20,D2
        MOVE.L  D3,D4
        MOVEQ   #22,D5
        LSR.L   D5,D4
        MOVEQ   #10,D5
        LSL.L   D5,D2
        OR.L    D4,D2
        LSL.L   D5,D3
        MOVE.L  D6,D4                   ; D4 = d = ex - ey
        SUB.L   D7,D4
        BEQ     .aligned
        CMP.L   #63,D4
        BLT     .shift
        MOVEQ   #0,D2                   ; only the sticky bit is left
        MOVEQ   #1,D3
        BRA     .aligned
.shift: MOVEQ   #0,D7                   ; D7: bits shifted out
        CMP.L   #32,D4
        BLT     .lt32
        MOVE.L  D3,D7
        MOVE.L  D2,D3
        MOVEQ   #0,D2
        SUB.L   #32,D4
        BEQ     .sticky
.lt32:  MOVEQ   #1,D5                   ; lost |= D3 & ((1 << d) - 1)
        LSL.L   D4,D5
        SUBQ.L  #1,D5
        AND.L   D3,D5
        OR.L    D5,D7
        LSR.L   D4,D3
        MOVE.L  D2,D5
        MOVEQ   #32,D6
        SUB.L   D4,D6
        LSL.L   D6,D5
        OR.L    D5,D3
        LSR.L   D4,D2
.sticky:
        TST.L   D7
        BEQ     .aligned
        BSET    #0,D3
.aligned:
        TST.L   (SP)+
        BMI     .subtract
        ADD.L   D3,D1
        ADDX.L  D2,D0
        BRA     .pack
.subtract:
        SUB.L   D3,D1
        SUBX.L  D2,D0
        MOVE.L  D0,D4
        OR.L    D1,D4
        BNE     .pack
        MOVEQ   #0,D0                   ; x - x = +0
        MOVEQ   #0,D1
        BRA     .done
.pack:  MOVE.L  A0,D2
        MOVE.L  A1,D3
        JSR     fp_finish
.done:  MOVEM.L (SP)+,D4-D7
        RTS
