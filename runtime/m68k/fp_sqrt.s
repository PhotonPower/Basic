;! module    fp_sqrt
;! provides  fp_sqrt
;! requires  fp_finish rt_error
;! group     float
;! clobbers  D0-D3/A0-A1
;! doc       Square root, correctly rounded (ties to even); negative argument -> error 5.

; D0:D1 -> D0:D1. Integer square root of R = m * 2^58 by shifting and subtracting (56 result
; bits, 2^55 <= root < 2^56), sticky = remainder; the value is root * 2^((e - 58) / 2).

fp_sqrt:
        MOVEM.L D4-D7,-(SP)
        MOVE.L  D0,D2
        AND.L   #$7FF00000,D2
        BEQ     .zero
        TST.L   D0
        BPL     .pos
        MOVEQ   #5,D0                   ; RT_ERR_SQR_NEG
        JSR     rt_error
        BRA     .zero
.pos:   SWAP    D2                      ; e = be - 1075
        LSR.W   #4,D2
        AND.L   #$7FF,D2
        SUB.L   #1075,D2
        AND.L   #$000FFFFF,D0           ; m = 53-bit significand in D4:D5
        BSET    #20,D0
        MOVE.L  D0,D4
        MOVE.L  D1,D5
        BTST    #0,D2                   ; odd exponent: m <<= 1, e -= 1
        BEQ     .even
        ADD.L   D5,D5
        ADDX.L  D4,D4
        SUBQ.L  #1,D2
.even:  SUB.L   #58,D2                  ; result exponent (e - 58) / 2
        ASR.L   #1,D2
        MOVE.L  D2,-(SP)
        MOVEQ   #10,D6                  ; align: bit 53 of m to bit 63
        MOVE.L  D5,D7
        MOVEQ   #22,D3
        LSR.L   D3,D7
        LSL.L   D6,D4
        OR.L    D7,D4
        LSL.L   D6,D5
        MOVEQ   #0,D0                   ; root
        MOVEQ   #0,D1
        MOVEQ   #0,D6                   ; remainder
        MOVEQ   #0,D7
        MOVE.W  #56,-(SP)               ; 56 pairs of bits
.loop:  ADD.L   D5,D5                   ; rem = (rem << 2) | next two bits
        ADDX.L  D4,D4
        ADDX.L  D7,D7
        ADDX.L  D6,D6
        ADD.L   D5,D5
        ADDX.L  D4,D4
        ADDX.L  D7,D7
        ADDX.L  D6,D6
        ADD.L   D1,D1                   ; root <<= 1
        ADDX.L  D0,D0
        MOVE.L  D1,D3                   ; test = 2 * root + 1
        MOVE.L  D0,D2
        ADD.L   D3,D3
        ADDX.L  D2,D2
        ADDQ.L  #1,D3
        CMP.L   D2,D6                   ; rem >= test ?
        BHI     .ge
        BCS     .next
        CMP.L   D3,D7
        BCS     .next
.ge:    SUB.L   D3,D7
        SUBX.L  D2,D6
        ADDQ.L  #1,D1
.next:  SUBQ.W  #1,(SP)
        BNE     .loop
        ADDQ.L  #2,SP
        MOVE.L  (SP)+,D2
        OR.L    D6,D7                   ; sticky
        BEQ     .exact
        BSET    #0,D1
.exact: MOVEQ   #0,D3                   ; positive
        JSR     fp_finish
        BRA     .done
.zero:  MOVEQ   #0,D0
        MOVEQ   #0,D1
.done:  MOVEM.L (SP)+,D4-D7
        RTS
