;! module    rt_power
;! provides  rt_power
;! requires  fp_trunc fp_cmp fp_mul fp_div fp_to_i32 rt_error rt_powf
;! group     float
;! clobbers  D0-D3/A0-A1
;! doc       x^y: integer y by repeated multiplication, otherwise rt_powf (as platform/pc power).

; D0:D1 = x, D2:D3 = y -> D0:D1

rt_power:
        MOVEM.L D4-D7/A2-A3,-(SP)
        MOVE.L  D0,A2                   ; A2:A3 = x
        MOVE.L  D1,A3
        MOVE.L  D2,D4                   ; D4:D5 = y
        MOVE.L  D3,D5
        MOVE.L  D2,D0                   ; integer exponent?
        MOVE.L  D3,D1
        JSR     fp_trunc
        MOVE.L  D4,D2
        MOVE.L  D5,D3
        JSR     fp_cmp
        TST.L   D0
        BNE     .frac
        MOVE.L  D4,D0                   ; |y| <= 2^31 - 1
        BCLR    #31,D0
        MOVE.L  D5,D1
        MOVE.L  #$41DFFFFF,D2
        MOVE.L  #$FFC00000,D3
        JSR     fp_cmp
        TST.L   D0
        BGT     .err
        MOVE.L  D4,D0
        MOVE.L  D5,D1
        JSR     fp_to_i32
        MOVE.L  D0,D7                   ; D7 = n, D6 = negative
        SMI     D6
        BPL     .npos
        NEG.L   D7
.npos:  MOVE.L  #$3FF00000,D4           ; r = 1 in D4:D5, b = x in A2:A3
        MOVEQ   #0,D5
.loop:  TST.L   D7
        BEQ     .fin
        BTST    #0,D7
        BEQ     .sq
        MOVE.L  D4,D0                   ; r *= b
        MOVE.L  D5,D1
        MOVE.L  A2,D2
        MOVE.L  A3,D3
        JSR     fp_mul
        MOVE.L  D0,D4
        MOVE.L  D1,D5
.sq:    LSR.L   #1,D7
        BEQ     .fin
        MOVE.L  A2,D0                   ; b *= b
        MOVE.L  A3,D1
        MOVE.L  D0,D2
        MOVE.L  D1,D3
        JSR     fp_mul
        MOVE.L  D0,A2
        MOVE.L  D1,A3
        BRA     .loop
.fin:   MOVE.L  D4,D0
        MOVE.L  D5,D1
        TST.B   D6
        BEQ     .done
        MOVE.L  D0,D2                   ; negative exponent: 1 / r
        MOVE.L  D1,D3
        MOVE.L  #$3FF00000,D0
        MOVEQ   #0,D1
        JSR     fp_div
        BRA     .done
.frac:  MOVE.L  A2,D0                   ; not an integer: EXP(y*LOG(x))
        MOVE.L  A3,D1
        MOVE.L  D4,D2
        MOVE.L  D5,D3
        JSR     rt_powf
        BRA     .done
.err:   MOVEQ   #1,D0                   ; RT_ERR_OVERFLOW (|y| >= 2^31)
        JSR     rt_error
        MOVEQ   #0,D0
        MOVEQ   #0,D1
.done:  MOVEM.L (SP)+,D4-D7/A2-A3
        RTS
