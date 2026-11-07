;! module    fp_to_int
;! provides  fp_to_i32 fp_to_i16 fp_to_u8
;! requires  rt_error
;! group     core
;! clobbers  D0-D3/A0-A1
;! doc       Assignment of a float (D0:D1) to %, & or |: truncation towards 0, range error 2, 4 or 3.

; Out: D0.L = value (0 after an error)

fp_to_i32:
        BSR     fp_to_int__trunc
        TST.L   D1
        BNE     fp_to_int__err2
        RTS

fp_to_i16:
        BSR     fp_to_int__trunc
        TST.L   D1
        BNE     .err
        CMP.L   #-32768,D0
        BLT     .err
        CMP.L   #32767,D0
        BGT     .err
        RTS
.err:   MOVEQ   #4,D0                   ; RT_ERR_NO_WORD
        BRA     fp_to_int__error

fp_to_u8:
        BSR     fp_to_int__trunc
        TST.L   D1
        BNE     .err
        CMP.L   #255,D0
        BHI     .err                    ; unsigned: negative values are also too large
        RTS
.err:   MOVEQ   #3,D0                   ; RT_ERR_NO_BYTE
        BRA     fp_to_int__error

fp_to_int__err2:
        MOVEQ   #2,D0                   ; RT_ERR_NO_INT
fp_to_int__error:
        JSR     rt_error
        MOVEQ   #0,D0
        RTS

; D0:D1 -> D0 = truncated value, D1 = 0, or D1 = 1 if it is outside -2^31 .. 2^31-1
fp_to_int__trunc:
        MOVEM.L D4-D5,-(SP)
        MOVE.L  D0,D2                   ; D2 = biased exponent
        SWAP    D2
        LSR.W   #4,D2
        AND.L   #$7FF,D2
        CMP.L   #1023,D2
        BLT     .small                  ; |x| < 1 (also 0)
        CMP.L   #1054,D2
        BLT     .fits                   ; |x| < 2^31
        CMP.L   #$C1E00000,D0           ; truncates to -2^31 ? (x in ]-2^31-1, -2^31])
        BNE     .range
        CMP.L   #$00200000,D1           ; the 21 bits below the integer part may be set
        BCC     .range
        MOVE.L  #$80000000,D0
        BRA     .ok
.fits:  MOVE.L  D0,D3                   ; D3 = sign
        AND.L   #$000FFFFF,D0
        BSET    #20,D0                  ; 53-bit significand in D0:D1
        MOVE.L  #1075,D4                ; shift = 1075 - be (22..52)
        SUB.L   D2,D4
        CMP.L   #32,D4
        BLT     .lt32
        SUB.L   #32,D4
        LSR.L   D4,D0                   ; value = hi >> (shift - 32)
        BRA     .sign
.lt32:  LSR.L   D4,D1                   ; value = (lo >> shift) | (hi << (32 - shift))
        MOVEQ   #32,D5
        SUB.L   D4,D5
        LSL.L   D5,D0
        OR.L    D1,D0
.sign:  TST.L   D3
        BPL     .ok
        NEG.L   D0
        BRA     .ok
.small: MOVEQ   #0,D0
.ok:    MOVEQ   #0,D1
        MOVEM.L (SP)+,D4-D5
        RTS
.range: MOVEQ   #0,D0
        MOVEQ   #1,D1
        MOVEM.L (SP)+,D4-D5
        RTS
