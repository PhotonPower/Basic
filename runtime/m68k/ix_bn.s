;! module    ix_bn
;! provides  ix_bn_zero ix_bn_copy ix_bn_mul_add ix_bn_mul_pow10 ix_bn_shl ix_bn_shr1 ix_bn_cmp ix_bn_sub ix_bn_is_zero
;! group     float
;! clobbers  D0-D1/A0-A1
;! doc       Unsigned big integers of D2.W longs (most significant first) for the exact conversions.

; Common: A0 = number (first long = most significant), D2.W = number of longs (n >= 1).
; All routines preserve D2-D7 and A2-A6.

; A0 = 0
ix_bn_zero:
        MOVE.L  A0,-(SP)
        MOVE.W  D2,D0
        SUBQ.W  #1,D0
.loop:  CLR.L   (A0)+
        DBRA    D0,.loop
        MOVE.L  (SP)+,A0
        RTS

; A0 = A1
ix_bn_copy:
        MOVEM.L A0-A1,-(SP)
        MOVE.W  D2,D0
        SUBQ.W  #1,D0
.loop:  MOVE.L  (A1)+,(A0)+
        DBRA    D0,.loop
        MOVEM.L (SP)+,A0-A1
        RTS

; A0 = A0 * D0.W + D1.W (both 0..65535), word by word from the end
ix_bn_mul_add:
        MOVEM.L D3-D4/A0,-(SP)
        MOVE.W  D2,D3
        ADD.W   D3,D3                   ; number of words
        LEA     0(A0,D3.W),A0
        ADD.W   D3,A0                   ; A0 = end of the number (2 bytes per word)
        AND.L   #$FFFF,D1               ; carry
        SUBQ.W  #1,D3
.loop:  MOVEQ   #0,D4
        MOVE.W  -(A0),D4
        MULU    D0,D4                   ; word * m (< 2^32 together with the carry)
        ADD.L   D1,D4
        MOVE.W  D4,(A0)
        CLR.W   D4
        SWAP    D4
        MOVE.L  D4,D1
        DBRA    D3,.loop
        MOVEM.L (SP)+,D3-D4/A0
        RTS

; A0 = A0 * 10^D0.W (D0 >= 0)
ix_bn_mul_pow10:
        MOVEM.L D3-D4,-(SP)
        MOVE.W  D0,D3
.k4:    CMP.W   #4,D3
        BLT     .rest
        MOVE.W  #10000,D0
        MOVEQ   #0,D1
        BSR     ix_bn_mul_add
        SUBQ.W  #4,D3
        BRA     .k4
.rest:  TST.W   D3
        BEQ     .done
        MOVEQ   #1,D0                   ; 10^(k mod 4)
.p:     MULU    #10,D0
        SUBQ.W  #1,D3
        BNE     .p
        MOVEQ   #0,D1
        BSR     ix_bn_mul_add
.done:  MOVEM.L (SP)+,D3-D4
        RTS

; A0 <<= D0.W bits (bits shifted out at the top are lost)
ix_bn_shl:
        MOVEM.L D3-D7/A0-A1,-(SP)
        MOVE.W  D0,D3
        LSR.W   #5,D3                   ; D3 = whole longs
        MOVE.W  D0,D4
        AND.W   #31,D4                  ; D4 = remaining bits
        TST.W   D3
        BEQ     .bits
        MOVE.W  D2,D5                   ; move longs up by D3: a[i] = a[i + D3]
        SUB.W   D3,D5                   ; count of longs kept
        BLE     .allzero
        MOVE.W  D3,D6
        LSL.W   #2,D6
        LEA     0(A0,D6.W),A1
        SUBQ.W  #1,D5
        MOVE.L  A0,-(SP)
.mv:    MOVE.L  (A1)+,(A0)+
        DBRA    D5,.mv
        MOVE.W  D3,D5                   ; clear the lowest D3 longs
        SUBQ.W  #1,D5
.cl:    CLR.L   (A0)+
        DBRA    D5,.cl
        MOVE.L  (SP)+,A0
.bits:  TST.W   D4
        BEQ     .done
        MOVEQ   #32,D5
        SUB.W   D4,D5                   ; D5 = 32 - s
        MOVE.W  D2,D6
        SUBQ.W  #2,D6
        BLT     .last
.shift: MOVE.L  (A0),D7                 ; a[i] = (a[i] << s) | (a[i+1] >> (32 - s))
        LSL.L   D4,D7
        MOVE.L  4(A0),D3
        LSR.L   D5,D3
        OR.L    D3,D7
        MOVE.L  D7,(A0)+
        DBRA    D6,.shift
.last:  MOVE.L  (A0),D7
        LSL.L   D4,D7
        MOVE.L  D7,(A0)
        BRA     .done
.allzero:
        BSR     ix_bn_zero
.done:  MOVEM.L (SP)+,D3-D7/A0-A1
        RTS

; A0 >>= 1
ix_bn_shr1:
        MOVEM.L D3/A0,-(SP)
        MOVE.W  D2,D3
        ADD.W   D3,D3
        SUBQ.W  #1,D3
        ANDI    #$EF,CCR                ; X = 0
.loop:  ROXR.W  (A0)+
        DBRA    D3,.loop
        MOVEM.L (SP)+,D3/A0
        RTS

; D0.L = -1, 0 or 1 for A0 <, =, > A1
ix_bn_cmp:
        MOVEM.L D3/A0-A1,-(SP)
        MOVE.W  D2,D3
        SUBQ.W  #1,D3
.loop:  CMPM.L  (A1)+,(A0)+
        BHI     .gt
        BCS     .lt
        DBRA    D3,.loop
        MOVEQ   #0,D0
        BRA     .out
.gt:    MOVEQ   #1,D0
        BRA     .out
.lt:    MOVEQ   #-1,D0
.out:   MOVEM.L (SP)+,D3/A0-A1
        RTS

; A0 -= A1 (requires A0 >= A1)
ix_bn_sub:
        MOVEM.L D3/A0-A1,-(SP)
        MOVE.W  D2,D3
        LSL.W   #2,D3
        ADD.W   D3,A0
        ADD.W   D3,A1
        LSR.W   #2,D3
        SUBQ.W  #1,D3
        ANDI    #$EF,CCR                ; X = 0
.loop:  SUBX.L  -(A1),-(A0)
        DBRA    D3,.loop
        MOVEM.L (SP)+,D3/A0-A1
        RTS

; D0.L = 1 if A0 = 0, else 0
ix_bn_is_zero:
        MOVEM.L D3/A0,-(SP)
        MOVE.W  D2,D3
        SUBQ.W  #1,D3
        MOVEQ   #0,D0
.loop:  OR.L    (A0)+,D0
        DBRA    D3,.loop
        TST.L   D0
        SEQ     D0
        AND.L   #1,D0
        MOVEM.L (SP)+,D3/A0
        RTS
