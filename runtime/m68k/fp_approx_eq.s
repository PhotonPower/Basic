;! module    fp_approx_eq
;! provides  fp_approx_eq
;! group     float
;! clobbers  D0-D3/A0-A1
;! doc       GFA "==": both rounded to 28 significant bits (ties away from 0), then compared; D0.L = 1 or 0.

fp_approx_eq:
        MOVEM.L D4-D5,-(SP)
        MOVE.L  D0,D4
        AND.L   #$7FF00000,D4
        MOVE.L  D2,D5
        AND.L   #$7FF00000,D5
        TST.L   D4                      ; 0 only equals 0
        BEQ     .azero
        TST.L   D5
        BEQ     .no
        MOVE.L  D0,D4                   ; signs must be equal
        EOR.L   D2,D4
        BMI     .no
        BSR     fp_approx_eq__key       ; D0:D1 -> D0 = exponent, D1 = key
        EXG     D0,D2
        EXG     D1,D3
        BSR     fp_approx_eq__key
        CMP.L   D0,D2
        BNE     .no
        CMP.L   D1,D3
        BNE     .no
.yes:   MOVEQ   #1,D0
        BRA     .out
.azero: TST.L   D5
        BEQ     .yes
.no:    MOVEQ   #0,D0
.out:   MOVEM.L (SP)+,D4-D5
        RTS

; |x| rounded to 28 significant bits: D0 = biased exponent, D1 = 28-bit key
fp_approx_eq__key:
        MOVE.L  D0,D4                   ; D4 = biased exponent
        SWAP    D4
        LSR.W   #4,D4
        AND.L   #$7FF,D4
        AND.L   #$000FFFFF,D0           ; 53-bit significand + 2^24
        BSET    #20,D0
        ADD.L   #$01000000,D1
        MOVEQ   #0,D5
        ADDX.L  D5,D0
        MOVEQ   #25,D5                  ; key = m >> 25 = (hi << 7) | (lo >> 25)
        LSR.L   D5,D1
        LSL.L   #7,D0
        OR.L    D0,D1
        CMP.L   #$10000000,D1           ; carried into bit 28: one more exponent step
        BNE     .ok
        LSR.L   #1,D1
        ADDQ.L  #1,D4
.ok:    MOVE.L  D4,D0
        RTS
