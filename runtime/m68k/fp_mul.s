;! module    fp_mul
;! provides  fp_mul
;! requires  fp_finish
;! group     float
;! clobbers  D0-D3/A0-A1
;! doc       Multiplication, correctly rounded (ties to even): exact 106-bit product, then fp_finish.

; D0:D1 = a, D2:D3 = b -> D0:D1

fp_mul:
        MOVEM.L D4-D7/A2-A6,-(SP)
        MOVE.L  D0,D4
        AND.L   #$7FF00000,D4
        BEQ     .zero
        MOVE.L  D2,D5
        AND.L   #$7FF00000,D5
        BEQ     .zero
        MOVE.L  D0,D6                   ; A5: sign of the result (bit 31)
        EOR.L   D2,D6
        MOVE.L  D6,A5
        SWAP    D4                      ; A6: e = ea + eb - 2150 + 42
        LSR.W   #4,D4
        AND.L   #$7FF,D4
        SWAP    D5
        LSR.W   #4,D5
        AND.L   #$7FF,D5
        ADD.L   D5,D4
        SUB.L   #2108,D4
        MOVE.L  D4,A6
        AND.L   #$000FFFFF,D0           ; significands: A2:A3 = a, A4:D3 = b
        BSET    #20,D0
        MOVE.L  D0,A2
        MOVE.L  D1,A3
        AND.L   #$000FFFFF,D2
        BSET    #20,D2
        MOVE.L  D2,A4
        MOVE.L  A3,D4                   ; al * bl -> r1:r0 in D0:D1
        MOVE.L  D3,D5
        BSR     fp_mul__mul32
        MOVE.L  D4,D0
        MOVE.L  D5,D1
        MOVE.L  A2,D4                   ; ah * bl
        MOVE.L  D3,D5
        BSR     fp_mul__mul32
        MOVE.L  D4,-(SP)
        MOVE.L  D5,-(SP)
        MOVE.L  A3,D4                   ; al * bh, plus ah * bl
        MOVE.L  A4,D5
        BSR     fp_mul__mul32
        MOVE.L  (SP)+,D6
        MOVE.L  (SP)+,D7
        ADD.L   D6,D5
        ADDX.L  D7,D4                   ; D4:D5 = cross terms (< 2^54)
        ADD.L   D5,D0                   ; r1 += low part
        MOVEQ   #0,D3
        ADDX.L  D3,D4
        MOVE.L  D4,D2                   ; r2 = high part + carry
        MOVE.L  A2,D4                   ; ah * bh, added at bit 64
        MOVE.L  A4,D5
        BSR     fp_mul__mul32
        ADD.L   D5,D2
        MOVEQ   #0,D3
        ADDX.L  D3,D4                   ; r3 = D4; product = D4:D2:D0:D1
        MOVE.L  D0,D5                   ; sticky: bits 41..0
        AND.L   #$3FF,D5
        OR.L    D1,D5
        MOVEQ   #10,D7                  ; m = product >> 42
        LSR.L   D7,D0
        MOVE.L  D2,D6
        MOVEQ   #22,D7
        LSL.L   D7,D6
        OR.L    D6,D0
        MOVEQ   #10,D7
        LSR.L   D7,D2
        MOVEQ   #22,D7
        LSL.L   D7,D4
        OR.L    D2,D4
        MOVE.L  D0,D1
        MOVE.L  D4,D0
        TST.L   D5
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

; D4 * D5 (unsigned 32 x 32) -> D4:D5 (64 bit); clobbers D6, D7
fp_mul__mul32:
        MOVEM.L D2-D3,-(SP)
        MOVE.L  D4,D2
        MULU    D5,D2                   ; xl * yl
        MOVE.L  D4,D3
        SWAP    D3
        MULU    D5,D3                   ; xh * yl
        MOVE.L  D5,D6
        SWAP    D6
        MOVE.L  D6,D7
        MULU    D4,D6                   ; yh * xl
        SWAP    D4
        MULU    D4,D7                   ; yh * xh
        ADD.L   D3,D6                   ; middle = xh*yl + yh*xl (33 bits)
        MOVEQ   #0,D3
        ADDX.L  D3,D3
        SWAP    D3                      ; carry of the middle sum at bit 16 of the high part
        MOVE.L  D6,D4
        CLR.W   D4
        SWAP    D4                      ; middle >> 16
        ADD.L   D3,D4
        SWAP    D6
        CLR.W   D6                      ; middle << 16
        ADD.L   D6,D2                   ; low = xl*yl + (middle << 16)
        MOVEQ   #0,D3
        ADDX.L  D3,D7
        ADD.L   D4,D7                   ; high = xh*yh + (middle >> 16) + carries
        MOVE.L  D7,D4
        MOVE.L  D2,D5
        MOVEM.L (SP)+,D2-D3
        RTS
