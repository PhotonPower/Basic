;! module    fp_div
;! provides  fp_div
;! requires  fp_finish rt_error
;! group     float
;! clobbers  D0-D3/A0-A1
;! doc       Division, correctly rounded (ties to even); division by 0 -> error 0 (runtime/ref/fp_div.c).

; D0:D1 = a, D2:D3 = b -> D0:D1. Restoring division: q = floor(ma / mb * 2^63), sticky = remainder.

fp_div:
        MOVEM.L D4-D7/A2-A6,-(SP)
        MOVE.L  D2,D5
        AND.L   #$7FF00000,D5
        BNE     .bnz
        MOVEQ   #0,D0                   ; RT_ERR_DIV_ZERO
        JSR     rt_error
        BRA     .zero
.bnz:   MOVE.L  D0,D4
        AND.L   #$7FF00000,D4
        BEQ     .zero
        MOVE.L  D0,D6                   ; A5: sign of the result
        EOR.L   D2,D6
        MOVE.L  D6,A5
        SWAP    D4                      ; A6: e = ea - eb - 63
        LSR.W   #4,D4
        AND.L   #$7FF,D4
        SWAP    D5
        LSR.W   #4,D5
        AND.L   #$7FF,D5
        SUB.L   D5,D4
        SUB.L   #63,D4
        MOVE.L  D4,A6
        AND.L   #$000FFFFF,D0           ; r = ma in D4:D5
        BSET    #20,D0
        MOVE.L  D0,D4
        MOVE.L  D1,D5
        AND.L   #$000FFFFF,D2           ; d = mb in D2:D3
        BSET    #20,D2
        MOVEQ   #0,D0                   ; q in D0:D1
        MOVEQ   #0,D1
        MOVEQ   #63,D6
.loop:  ADD.L   D1,D1                   ; q <<= 1
        ADDX.L  D0,D0
        CMP.L   D2,D4                   ; r >= d ?
        BHI     .sub
        BCS     .next
        CMP.L   D3,D5
        BCS     .next
.sub:   SUB.L   D3,D5
        SUBX.L  D2,D4
        BSET    #0,D1
.next:  ADD.L   D5,D5                   ; r <<= 1
        ADDX.L  D4,D4
        DBRA    D6,.loop
        OR.L    D4,D5                   ; sticky: remainder not 0
        BEQ     .exact
        BSET    #0,D1
.exact: MOVE.L  A6,D2
        MOVE.L  A5,D3
        JSR     fp_finish
        BRA     .done
.zero:  MOVEQ   #0,D0
        MOVEQ   #0,D1
.done:  MOVEM.L (SP)+,D4-D7/A2-A6
        RTS
