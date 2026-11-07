;! module    rt_flt_dec
;! provides  rt_flt_dec
;! requires  ix_bn_zero ix_bn_copy ix_bn_mul_pow10 ix_bn_shl ix_bn_shr1 ix_bn_cmp ix_bn_sub
;! group     float
;! clobbers  D0-D3/A0-A1
;! doc       Decimal digits of a float: exact binary value rounded to n significant digits, ties away from 0.

; In:  D0:D1 = x, D2.W = digits (1..13), A0 = buffer for the digits (no terminator)
; Out: D0.L = decimal exponent d of the first digit, D1.L = 1 if x < 0, else 0.
;      For 0: all digits '0', d = 0.
; As runtime/ref/rt_flt_dec.c: q = floor(|x| * 10^s), s = digits-1-d, with 10^(digits-1) <= q < 10^digits.

rt_flt_dec__N   EQU 40                  ; longs per big integer (1280 bits)
rt_flt_dec__NUM EQU -160
rt_flt_dec__DEN EQU -320
rt_flt_dec__DSH EQU -480
rt_flt_dec__Q   EQU -488                ; quotient, 64 bit
rt_flt_dec__PHI EQU -496                ; 10^digits
rt_flt_dec__PLO EQU -504                ; 10^(digits-1)

rt_flt_dec:
        LINK    A6,#-504
        MOVEM.L D2-D7/A2-A5,-(SP)
        MOVE.L  A0,A5                   ; A5 = buffer
        MOVE.W  D2,D7                   ; D7 = digits
        EXT.L   D7
        MOVE.L  D0,D3
        AND.L   #$7FF00000,D3
        BNE     .nonzero
        MOVE.W  D7,D3                   ; 0: all digits '0'
        SUBQ.W  #1,D3
.zfill: MOVE.B  #'0',(A0)+
        DBRA    D3,.zfill
        MOVEQ   #0,D0
        MOVEQ   #0,D1
        BRA     .out
.nonzero:
        SUB.L   A4,A4                   ; A4 = sign
        TST.L   D0
        BPL     .pos
        ADDQ.L  #1,A4
.pos:   MOVE.L  D0,D6                   ; D6 = e = be - 1075
        SWAP    D6
        LSR.W   #4,D6
        AND.L   #$7FF,D6
        SUB.L   #1075,D6
        AND.L   #$000FFFFF,D0           ; A2:A3 = 53-bit significand
        BSET    #20,D0
        MOVE.L  D0,A2
        MOVE.L  D1,A3
        MOVEQ   #0,D0                   ; 10^digits and 10^(digits-1), 64 bit
        MOVEQ   #1,D1                   ; D0:D1 = high:low, starting at 1
        MOVE.W  D7,D3
        SUBQ.W  #1,D3
.pw:    MOVE.L  D0,rt_flt_dec__PLO(A6)  ; high long first
        MOVE.L  D1,rt_flt_dec__PLO+4(A6)
        BSR     rt_flt_dec__times10
        DBRA    D3,.pw
        MOVE.L  D0,rt_flt_dec__PHI(A6)
        MOVE.L  D1,rt_flt_dec__PHI+4(A6)
        MOVE.W  D6,D5                   ; estimate d = floor((e + 52) * log10(2))
        ADD.W   #52,D5
        MULS    #19728,D5               ; 19728 / 65536 ~ log10(2)
        ASR.L   #8,D5
        ASR.L   #8,D5
.again: MOVE.L  D7,D4                   ; s = digits - 1 - d
        SUBQ.L  #1,D4
        SUB.L   D5,D4
        MOVE.W  #rt_flt_dec__N,D2
        LEA     rt_flt_dec__NUM(A6),A0  ; num = m, den = 1
        BSR     ix_bn_zero
        MOVE.L  A2,(rt_flt_dec__N-2)*4(A0)
        MOVE.L  A3,(rt_flt_dec__N-1)*4(A0)
        LEA     rt_flt_dec__DEN(A6),A0
        BSR     ix_bn_zero
        MOVE.L  #1,(rt_flt_dec__N-1)*4(A0)
        TST.L   D4                      ; num *= 10^s or den *= 10^-s
        BMI     .sneg
        LEA     rt_flt_dec__NUM(A6),A0
        MOVE.W  D4,D0
        BRA     .spow
.sneg:  LEA     rt_flt_dec__DEN(A6),A0
        MOVE.W  D4,D0
        NEG.W   D0
.spow:  BSR     ix_bn_mul_pow10
        TST.L   D6                      ; num <<= e or den <<= -e
        BMI     .eneg
        LEA     rt_flt_dec__NUM(A6),A0
        MOVE.W  D6,D0
        BRA     .eshl
.eneg:  LEA     rt_flt_dec__DEN(A6),A0
        MOVE.W  D6,D0
        NEG.W   D0
.eshl:  BSR     ix_bn_shl
        LEA     rt_flt_dec__DSH(A6),A0  ; q = num / den (q < 2^51): dsh = den << 50, then halve
        LEA     rt_flt_dec__DEN(A6),A1
        BSR     ix_bn_copy
        MOVEQ   #50,D0
        BSR     ix_bn_shl
        CLR.L   rt_flt_dec__Q(A6)
        CLR.L   rt_flt_dec__Q+4(A6)
        MOVEQ   #50,D3
.div:   MOVE.L  rt_flt_dec__Q(A6),D0    ; q <<= 1
        MOVE.L  rt_flt_dec__Q+4(A6),D1
        ADD.L   D1,D1
        ADDX.L  D0,D0
        MOVE.L  D0,rt_flt_dec__Q(A6)
        MOVE.L  D1,rt_flt_dec__Q+4(A6)
        LEA     rt_flt_dec__NUM(A6),A0
        LEA     rt_flt_dec__DSH(A6),A1
        BSR     ix_bn_cmp
        TST.L   D0
        BMI     .nobit
        BSR     ix_bn_sub
        BSET    #0,rt_flt_dec__Q+7(A6)
.nobit: LEA     rt_flt_dec__DSH(A6),A0
        BSR     ix_bn_shr1
        DBRA    D3,.div
        MOVE.L  rt_flt_dec__Q(A6),D0    ; q >= 10^digits: d + 1
        MOVE.L  rt_flt_dec__Q+4(A6),D1
        MOVE.L  rt_flt_dec__PHI+4(A6),D3
        CMP.L   rt_flt_dec__PHI(A6),D0
        BHI     .dinc
        BCS     .chklo
        CMP.L   D3,D1
        BCC     .dinc
.chklo: MOVE.L  rt_flt_dec__PLO+4(A6),D3  ; q < 10^(digits-1): d - 1
        CMP.L   rt_flt_dec__PLO(A6),D0
        BHI     .found
        BCS     .ddec
        CMP.L   D3,D1
        BCC     .found
.ddec:  SUBQ.L  #1,D5
        BRA     .again
.dinc:  ADDQ.L  #1,D5
        BRA     .again
.found: LEA     rt_flt_dec__NUM(A6),A0  ; remainder * 2 >= den: round half away from 0
        MOVEQ   #1,D0
        MOVE.W  #rt_flt_dec__N,D2
        BSR     ix_bn_shl
        LEA     rt_flt_dec__DEN(A6),A1
        BSR     ix_bn_cmp
        MOVE.L  rt_flt_dec__Q(A6),D2
        MOVE.L  rt_flt_dec__Q+4(A6),D3
        TST.L   D0
        BMI     .digits
        ADDQ.L  #1,D3
        BCC     .nc
        ADDQ.L  #1,D2
.nc:    CMP.L   rt_flt_dec__PHI(A6),D2  ; q = 10^digits: q = 10^(digits-1), d + 1
        BNE     .digits
        CMP.L   rt_flt_dec__PHI+4(A6),D3
        BNE     .digits
        MOVE.L  rt_flt_dec__PLO(A6),D2
        MOVE.L  rt_flt_dec__PLO+4(A6),D3
        ADDQ.L  #1,D5
.digits:
        MOVE.L  A5,A0                   ; digits from the end: q mod 10, q / 10
        ADD.L   D7,A0
        MOVE.W  D7,D4
        SUBQ.W  #1,D4
.dl:    MOVE.L  D2,D1                   ; D2:D3 / 10 word by word (w3 w2 w1 w0)
        SWAP    D1
        MOVEQ   #0,D0
        MOVE.W  D1,D0                   ; 0:w3
        DIVU    #10,D0                  ; remainder:quotient
        MOVE.W  D0,D6
        SWAP    D6                      ; q3 in the high word
        MOVE.W  D2,D0                   ; remainder:w2
        DIVU    #10,D0
        MOVE.W  D0,D6                   ; D6 = q3:q2
        MOVE.L  D3,D1
        SWAP    D1                      ; D1 = w0:w1
        MOVE.W  D1,D0                   ; remainder:w1
        DIVU    #10,D0
        MOVE.W  D0,D1
        SWAP    D1                      ; D1 = q1:w0
        MOVE.W  D1,D0                   ; remainder:w0
        DIVU    #10,D0
        MOVE.W  D0,D1                   ; D1 = q1:q0
        MOVE.L  D1,D3
        MOVE.L  D6,D2
        SWAP    D0                      ; remainder = digit
        ADD.B   #'0',D0
        MOVE.B  D0,-(A0)
        DBRA    D4,.dl
        MOVE.L  D5,D0
        MOVE.L  A4,D1
.out:   MOVEM.L (SP)+,D2-D7/A2-A5
        UNLK    A6
        RTS

; D0:D1 (high:low) *= 10
rt_flt_dec__times10:
        MOVEM.L D2-D3,-(SP)
        ADD.L   D1,D1                   ; x * 2
        ADDX.L  D0,D0
        MOVE.L  D0,D2
        MOVE.L  D1,D3
        ADD.L   D1,D1                   ; x * 8
        ADDX.L  D0,D0
        ADD.L   D1,D1
        ADDX.L  D0,D0
        ADD.L   D3,D1                   ; x * 8 + x * 2
        ADDX.L  D2,D0
        MOVEM.L (SP)+,D2-D3
        RTS
