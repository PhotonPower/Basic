;! module    fp_round
;! provides  fp_round
;! requires  fp_mul fp_div fp_add fp_cmp fp_int
;! group     float
;! clobbers  D0-D3/A0-A1
;! doc       ROUND(x,n) = INT(x*10^n+0.5)/10^n; x unchanged if |x*10^n| >= 2^52 (runtime/ref/fp_round.c).

; D0:D1 = x, D2.L = n -> D0:D1

fp_round:
        MOVEM.L D4-D7/A2-A3,-(SP)
        MOVE.L  D0,D3
        AND.L   #$7FF00000,D3
        BEQ     .zero
        MOVE.L  D0,D4                   ; D4:D5 = x
        MOVE.L  D1,D5
        MOVE.L  D2,A2                   ; A2 = n
        MOVE.L  D2,D3                   ; D3 = k = |n|
        BPL     .kpos
        NEG.L   D3
.kpos:  CMP.L   #308,D3
        BLE     .pow
        TST.L   D2                      ; k > 308: n > 0 -> x, n < 0 -> 0
        BGT     .keep
        BRA     .zero
.pow:   MOVE.L  #$3FF00000,D6           ; D6:D7 = p = 10^k by repeated multiplication
        MOVEQ   #0,D7
        MOVE.L  D3,A3
        BRA     .pcond
.ploop: MOVE.L  D6,D0
        MOVE.L  D7,D1
        MOVE.L  #$40240000,D2           ; 10
        MOVEQ   #0,D3
        JSR     fp_mul
        MOVE.L  D0,D6
        MOVE.L  D1,D7
        SUBQ.L  #1,A3
.pcond: MOVE.L  A3,D0
        TST.L   D0
        BNE     .ploop
        MOVE.L  A2,D0
        TST.L   D0
        BMI     .neg
        MOVE.L  #$43300000,D0           ; n >= 0: |x| >= 2^52 / p -> x
        MOVEQ   #0,D1
        MOVE.L  D6,D2
        MOVE.L  D7,D3
        JSR     fp_div
        MOVE.L  D0,D2
        MOVE.L  D1,D3
        MOVE.L  D4,D0
        BCLR    #31,D0
        MOVE.L  D5,D1
        JSR     fp_cmp
        TST.L   D0
        BGE     .keep
        MOVE.L  D4,D0                   ; y = x * p
        MOVE.L  D5,D1
        MOVE.L  D6,D2
        MOVE.L  D7,D3
        JSR     fp_mul
        BRA     .round
.neg:   MOVE.L  D4,D0                   ; n < 0: y = x / p; |y| >= 2^52 -> x
        MOVE.L  D5,D1
        MOVE.L  D6,D2
        MOVE.L  D7,D3
        JSR     fp_div
        MOVE.L  D0,-(SP)
        MOVE.L  D1,-(SP)
        BCLR    #31,D0
        MOVE.L  #$43300000,D2
        MOVEQ   #0,D3
        JSR     fp_cmp
        MOVE.L  D0,D2
        MOVE.L  (SP)+,D1
        MOVE.L  (SP)+,D0
        TST.L   D2
        BGE     .keep
.round: MOVE.L  #$3FE00000,D2           ; y = INT(y + 0.5)
        MOVEQ   #0,D3
        JSR     fp_add
        JSR     fp_int
        MOVE.L  D6,D2                   ; n >= 0: y / p, else y * p
        MOVE.L  D7,D3
        MOVE.L  A2,A3
        MOVE.L  A3,D4
        TST.L   D4
        BMI     .mul
        JSR     fp_div
        BRA     .done
.mul:   JSR     fp_mul
        BRA     .done
.keep:  MOVE.L  D4,D0
        MOVE.L  D5,D1
        BRA     .done
.zero:  MOVEQ   #0,D0
        MOVEQ   #0,D1
.done:  MOVEM.L (SP)+,D4-D7/A2-A3
        RTS
