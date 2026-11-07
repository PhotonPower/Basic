;! module    fp_round_int
;! provides  fp_trunc fp_int fp_frac fp_cint
;! requires  fp_add fp_round_pack
;! group     float
;! clobbers  D0-D3/A0-A1
;! doc       TRUNC/FIX (towards 0), INT (floor), FRAC (x - TRUNC x), CINT (nearest, ties away from 0).

; All: D0:D1 -> D0:D1

fp_trunc:
        MOVEM.L D4-D5,-(SP)
        MOVE.L  D0,D2                   ; D2 = biased exponent
        SWAP    D2
        LSR.W   #4,D2
        AND.L   #$7FF,D2
        BEQ     .zero
        CMP.L   #1075,D2
        BGE     .done                   ; already an integer
        CMP.L   #1023,D2
        BLT     .zero                   ; |x| < 1
        MOVE.L  #1075,D4                ; clear the lowest (1075 - be) bits of the fraction (1..52)
        SUB.L   D2,D4
        CMP.L   #32,D4
        BLT     .lt32
        MOVEQ   #0,D1
        SUB.L   #32,D4
        BEQ     .done
        MOVEQ   #-1,D5
        LSL.L   D4,D5
        AND.L   D5,D0
        BRA     .done
.lt32:  MOVEQ   #-1,D5
        LSL.L   D4,D5
        AND.L   D5,D1
        BRA     .done
.zero:  MOVEQ   #0,D0
        MOVEQ   #0,D1
.done:  MOVEM.L (SP)+,D4-D5
        RTS

fp_int:
        MOVEM.L D4-D5,-(SP)
        MOVE.L  D0,D4                   ; x
        MOVE.L  D1,D5
        BSR     fp_trunc
        MOVE.L  D4,D2                   ; x = 0 (also -0): INT is 0
        AND.L   #$7FF00000,D2
        BEQ     .done
        TST.L   D4                      ; x < 0 and not an integer: trunc - 1
        BPL     .done
        CMP.L   D0,D4
        BNE     .minus
        CMP.L   D1,D5
        BEQ     .done
.minus: MOVE.L  #$3FF00000,D2
        MOVEQ   #0,D3
        JSR     fp_sub
.done:  MOVEM.L (SP)+,D4-D5
        RTS

fp_frac:
        MOVEM.L D4-D5,-(SP)
        MOVE.L  D0,D4
        MOVE.L  D1,D5
        BSR     fp_trunc
        MOVE.L  D0,D2                   ; x - trunc(x)
        MOVE.L  D1,D3
        MOVE.L  D4,D0
        MOVE.L  D5,D1
        JSR     fp_sub
        MOVEM.L (SP)+,D4-D5
        RTS

fp_cint:
        MOVEM.L D4-D5,-(SP)
        MOVE.L  D0,D2                   ; D2 = biased exponent
        SWAP    D2
        LSR.W   #4,D2
        AND.L   #$7FF,D2
        BEQ     .zero
        CMP.L   #1075,D2
        BGE     .done                   ; already an integer
        CMP.L   #1022,D2
        BLT     .zero                   ; |x| < 0.5
        MOVE.L  D0,D3                   ; D3 = sign
        AND.L   #$000FFFFF,D0           ; 53-bit significand
        BSET    #20,D0
        MOVE.L  #1075,D4                ; sh = 1075 - be (1..53)
        SUB.L   D2,D4
        SUB.L   #1075,D2                ; e = be - 1075
        MOVE.L  D4,D5                   ; m += 1 << (sh - 1)
        SUBQ.L  #1,D5
        CMP.L   #32,D5
        BLT     .addlo
        SUB.L   #32,D5
        MOVEQ   #1,D4
        LSL.L   D5,D4
        ADD.L   D4,D0
        ADD.L   #32,D5                  ; restore sh - 1
        BRA     .clear
.addlo: MOVEQ   #1,D4
        LSL.L   D5,D4
        ADD.L   D4,D1
        MOVEQ   #0,D4
        ADDX.L  D4,D0
.clear: ADDQ.L  #1,D5                   ; clear the lowest sh bits
        CMP.L   #32,D5
        BLT     .cllo
        MOVEQ   #0,D1
        SUB.L   #32,D5
        MOVEQ   #-1,D4
        LSL.L   D5,D4
        AND.L   D4,D0
        BRA     .pack
.cllo:  MOVEQ   #-1,D4
        LSL.L   D5,D4
        AND.L   D4,D1
.pack:  JSR     fp_round_pack           ; exact
        BRA     .done
.zero:  MOVEQ   #0,D0
        MOVEQ   #0,D1
.done:  MOVEM.L (SP)+,D4-D5
        RTS
