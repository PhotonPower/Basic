;! module    rt_val
;! provides  rt_val
;! requires  ix_bn_zero ix_bn_mul_add ix_bn_mul_pow10 ix_bn_shl ix_bn_shr1 ix_bn_cmp ix_bn_sub ix_bn_is_zero fp_round_pack fp_from_i32
;! group     float
;! clobbers  D0-D3/A0-A1
;! doc       VAL and VAL?: text -> float, correctly rounded; &H $ &O &X % prefixes (runtime/ref/rt_val.c).

; In:  A0 = characters, D0.L = length
; Out: D0:D1 = value, D2.L = number of characters used (VAL?)
; Up to 800 significant digits are kept, further non-zero digits only set the sticky flag.
; The big integers have as many longs as the digits and the exponent need (at most ~210).

rt_val__DIGITS EQU 800
rt_val__MAXN   EQU 216

rt_val:
        MOVEM.L D4-D7/A2-A3,-(SP)
        MOVE.L  A0,A2                   ; A2 = text, D7 = length, D6 = position
        MOVE.L  D0,D7
        MOVEQ   #0,D6
.sp:    CMP.L   D7,D6
        BCC     .zero
        CMP.B   #' ',0(A2,D6.L)
        BNE     .nsp
        ADDQ.L  #1,D6
        BRA     .sp
.nsp:   MOVE.B  0(A2,D6.L),D0
        CMP.B   #'$',D0
        BEQ     .hex1
        CMP.B   #'%',D0
        BEQ     .bin1
        CMP.B   #'&',D0
        BNE     .dec
        MOVE.L  D6,D1                   ; &H &O &X
        ADDQ.L  #1,D1
        CMP.L   D7,D1
        BCC     .zero
        MOVE.B  0(A2,D1.L),D0
        AND.B   #$DF,D0
        MOVEQ   #4,D3
        CMP.B   #'H',D0
        BEQ     .pfx2
        MOVEQ   #3,D3
        CMP.B   #'O',D0
        BEQ     .pfx2
        MOVEQ   #1,D3
        CMP.B   #'X',D0
        BEQ     .pfx2
        BRA     .zero
.pfx2:  ADDQ.L  #2,D6
        BRA     .radix
.hex1:  MOVEQ   #4,D3
        ADDQ.L  #1,D6
        BRA     .radix
.bin1:  MOVEQ   #1,D3
        ADDQ.L  #1,D6
; D3 = bits per digit: 32-bit value, wrapping, read as a signed long (probe6 T1)
.radix: MOVEQ   #0,D4                   ; D4 = value
        MOVEQ   #0,D5                   ; D5 = any digit
        MOVEQ   #1,D2                   ; D2 = base
        LSL.L   D3,D2
.rl:    CMP.L   D7,D6
        BCC     .rend
        MOVEQ   #0,D0
        MOVE.B  0(A2,D6.L),D0
        BSR     rt_val__digit
        CMP.L   D2,D0
        BCC     .rend                   ; also -1 (unsigned large)
        LSL.L   D3,D4
        OR.L    D0,D4
        MOVEQ   #1,D5
        ADDQ.L  #1,D6
        BRA     .rl
.rend:  TST.L   D5
        BEQ     .zero
        MOVE.L  D4,D0
        JSR     fp_from_i32
        MOVE.L  D6,D2
        BRA     .out

.zero:  MOVEQ   #0,D6                   ; nothing read: 0, no characters used
.zerou: MOVEQ   #0,D0                   ; 0 with D6 characters used
        MOVEQ   #0,D1
        MOVE.L  D6,D2
.out:   MOVEM.L (SP)+,D4-D7/A2-A3
        RTS

; decimal: [sign] digits [. digits] [E [sign] digits]
.dec:   CLR.W   rt_val__neg
        CMP.B   #'-',D0
        BNE     .np
        MOVE.W  #1,rt_val__neg
        BRA     .sgn
.np:    CMP.B   #'+',D0
        BNE     .ns
.sgn:   ADDQ.L  #1,D6
.ns:    LEA     rt_val__dig,A3          ; A3 = digits, D5 = count, D4 = decimal exponent
        MOVEQ   #0,D5
        MOVEQ   #0,D4
        CLR.W   rt_val__any
        CLR.W   rt_val__tail
.int:   BSR     rt_val__dec             ; D0 = digit or -1
        BMI     .frac
        MOVE.W  #1,rt_val__any
        ADDQ.L  #1,D6
        TST.L   D5
        BNE     .i1
        TST.B   D0
        BEQ     .int                    ; leading zero
.i1:    CMP.L   #rt_val__DIGITS,D5
        BCC     .i2
        MOVE.B  D0,0(A3,D5.L)
        ADDQ.L  #1,D5
        BRA     .int
.i2:    ADDQ.L  #1,D4
        TST.B   D0
        BEQ     .int
        MOVE.W  #1,rt_val__tail
        BRA     .int
.frac:  CMP.L   D7,D6
        BCC     .nd
        CMP.B   #'.',0(A2,D6.L)
        BNE     .nd
        ADDQ.L  #1,D6
.fl:    BSR     rt_val__dec
        BMI     .nd
        MOVE.W  #1,rt_val__any
        ADDQ.L  #1,D6
        TST.L   D5
        BNE     .f1
        TST.B   D0
        BNE     .f1
        SUBQ.L  #1,D4                   ; leading zero after the point
        BRA     .fl
.f1:    CMP.L   #rt_val__DIGITS,D5
        BCC     .f2
        MOVE.B  D0,0(A3,D5.L)
        ADDQ.L  #1,D5
        SUBQ.L  #1,D4
        BRA     .fl
.f2:    TST.B   D0
        BEQ     .fl
        MOVE.W  #1,rt_val__tail
        BRA     .fl
.nd:    TST.W   rt_val__any
        BEQ     .zero
        CMP.L   D7,D6                   ; exponent
        BCC     .used
        MOVE.B  0(A2,D6.L),D0
        AND.B   #$DF,D0
        CMP.B   #'E',D0
        BNE     .used
        ADDQ.L  #1,D6                   ; consumed even without digits: VAL?("1E") = 2
        MOVEQ   #0,D3                   ; D3 = exponent negative
        CMP.L   D7,D6
        BCC     .ex
        MOVE.B  0(A2,D6.L),D0
        CMP.B   #'-',D0
        BEQ     .es
        CMP.B   #'+',D0
        BNE     .ex
.es:    MOVE.L  D6,D1                   ; a sign counts only before a digit
        ADDQ.L  #1,D1
        CMP.L   D7,D1
        BCC     .ex
        MOVE.B  0(A2,D1.L),D2
        CMP.B   #'0',D2
        BCS     .ex
        CMP.B   #'9',D2
        BHI     .ex
        CMP.B   #'-',D0
        SEQ     D3
        MOVE.L  D1,D6
.ex:    MOVEQ   #0,D2                   ; D2 = exponent value (stops growing at 100000)
.el:    BSR     rt_val__dec
        BMI     .ee
        ADDQ.L  #1,D6
        CMP.L   #100000,D2
        BGE     .el
        MOVE.L  D2,D1                   ; D2 = D2 * 10 + digit
        LSL.L   #3,D2
        ADD.L   D1,D2
        ADD.L   D1,D2
        AND.L   #$FF,D0
        ADD.L   D0,D2
        BRA     .el
.ee:    TST.B   D3
        BEQ     .ep
        NEG.L   D2
.ep:    ADD.L   D2,D4
.used:  MOVE.L  D6,rt_val__used
.tz:    TST.L   D5                      ; trailing zeros into the exponent
        BEQ     .zu
        TST.B   -1(A3,D5.L)
        BNE     .top
        SUBQ.L  #1,D5
        ADDQ.L  #1,D4
        BRA     .tz
.zu:    MOVE.L  rt_val__used,D6
        BRA     .zerou
.top:   MOVE.L  D5,D0                   ; out of range -> 0 without error (probe3 T20)
        ADD.L   D4,D0
        SUBQ.L  #1,D0
        CMP.L   #309,D0
        BGT     .zu
        CMP.L   #-330,D0
        BLT     .zu
        BSR     rt_val__convert
        MOVE.L  rt_val__used,D2
        BRA     .out

; D5 = number of digits (A3), D4 = decimal exponent -> D0:D1 (0 on overflow)
rt_val__convert:
        MOVE.L  D4,D0                   ; longs: (digits + |exp|) * 27 / 256 + 6
        BPL     .a
        NEG.L   D0
.a:     ADD.L   D5,D0
        MULU    #27,D0
        LSR.L   #8,D0
        ADDQ.L  #6,D0
        MOVE.L  D0,D2                   ; D2.W = N for all ix_bn routines
        LEA     rt_val__num,A0
        JSR     ix_bn_zero
        MOVEQ   #0,D6                   ; digits in groups of up to four: num = num * 10^c + group
.grp:   CMP.L   D5,D6
        BCC     .grpd
        MOVEQ   #0,D1
        MOVEQ   #1,D0
        MOVEQ   #3,D3
.g4:    MULU    #10,D0
        MULU    #10,D1
        MOVEQ   #0,D7
        MOVE.B  0(A3,D6.L),D7
        ADD.L   D7,D1
        ADDQ.L  #1,D6
        CMP.L   D5,D6
        DBCC    D3,.g4
        LEA     rt_val__num,A0
        JSR     ix_bn_mul_add
        BRA     .grp
.grpd:  TST.L   D4
        BMI     .neg
        LEA     rt_val__num,A0          ; num * 10^exp
        MOVE.L  D4,D0
        JSR     ix_bn_mul_pow10
        BSR     rt_val__bitlen
        MOVEQ   #0,D3                   ; D3 = binary exponent
        CMP.L   #64,D0
        BCS     .small
        MOVE.L  D0,D3
        SUB.L   #64,D3
        MOVE.L  D2,D1                   ; top bit to the top of the number
        LSL.L   #5,D1
        SUB.L   D0,D1
        MOVE.L  D1,D0
        LEA     rt_val__num,A0
        JSR     ix_bn_shl
        MOVE.L  (A0),D0                 ; q = first two longs, sticky = any other bit
        MOVE.L  4(A0),D1
        MOVE.L  D2,D7
        SUBQ.W  #3,D7
        BMI     .pack
        ADDQ.L  #8,A0
.st:    TST.L   (A0)+
        DBNE    D7,.st
        BEQ     .pack
        OR.L    #1,D1
        BRA     .pack
.small: MOVE.L  D2,D0                   ; fewer than 64 bits: the last two longs
        LSL.L   #2,D0
        LEA     rt_val__num,A0
        ADD.L   D0,A0
        MOVE.L  -8(A0),D0
        MOVE.L  -4(A0),D1
        BRA     .pack
.neg:   LEA     rt_val__den,A0          ; den = 10^-exp
        JSR     ix_bn_zero
        MOVE.L  D2,D0
        LSL.L   #2,D0
        MOVE.L  #1,-4(A0,D0.L)
        MOVE.L  D4,D0
        NEG.L   D0
        JSR     ix_bn_mul_pow10
        BSR     rt_val__bitlen          ; k = 63 + bitlen(den) - bitlen(num)
        MOVE.L  D0,D3
        ADD.L   #63,D3
        LEA     rt_val__num,A0
        BSR     rt_val__bitlen
        SUB.L   D0,D3
        BMI     .kn
        MOVE.L  D3,D0                   ; num <<= k
        JSR     ix_bn_shl
        BRA     .div
.kn:    MOVE.L  D3,D0                   ; den <<= -k
        NEG.L   D0
        LEA     rt_val__den,A0
        JSR     ix_bn_shl
.div:   NEG.L   D3                      ; binary exponent -k
        LEA     rt_val__den,A0          ; q = floor(num / den) < 2^64: den <<= 63, 64 steps
        MOVEQ   #63,D0
        JSR     ix_bn_shl
        MOVEQ   #0,D4                   ; D4:D5 = q
        MOVEQ   #0,D5
        MOVEQ   #63,D6
.dl:    ADD.L   D5,D5
        ADDX.L  D4,D4
        LEA     rt_val__num,A0
        LEA     rt_val__den,A1
        JSR     ix_bn_cmp
        TST.L   D0
        BMI     .dn
        JSR     ix_bn_sub
        ADDQ.L  #1,D5
.dn:    LEA     rt_val__den,A0
        JSR     ix_bn_shr1
        DBRA    D6,.dl
        MOVE.L  D4,D0
        MOVE.L  D5,D1
        LEA     rt_val__num,A0          ; sticky: remainder
        MOVEM.L D0-D1,-(SP)
        JSR     ix_bn_is_zero
        MOVE.L  D0,D7
        MOVEM.L (SP)+,D0-D1
        TST.L   D7
        BNE     .pack
        OR.L    #1,D1
.pack:  TST.W   rt_val__tail
        BEQ     .p2
        OR.L    #1,D1
.p2:    MOVE.L  D3,D2
        MOVEQ   #0,D3
        TST.W   rt_val__neg
        BEQ     .p3
        BSET    #31,D3
.p3:    JSR     fp_round_pack
        TST.L   D2
        BEQ     .ok
        MOVEQ   #0,D0                   ; overflow: 0 without error
        MOVEQ   #0,D1
.ok:    RTS

; A0 = number (D2.W longs) -> D0.L = bit length
rt_val__bitlen:
        MOVEM.L D3/A0,-(SP)
        MOVE.L  D2,D3
        AND.L   #$FFFF,D3
.f:     MOVE.L  (A0)+,D0
        BNE     .found
        SUBQ.L  #1,D3
        BNE     .f
        MOVEQ   #0,D0
        BRA     .out
.found: LSL.L   #5,D3                   ; bits = 32 * remaining longs - leading zeros
.c:     TST.L   D0
        BMI     .done
        ADD.L   D0,D0
        SUBQ.L  #1,D3
        BRA     .c
.done:  MOVE.L  D3,D0
.out:   MOVEM.L (SP)+,D3/A0
        RTS

; D0 = character -> D0 = value as a digit of base 16 (0-15), else -1
rt_val__digit:
        CMP.B   #'0',D0
        BCS     .no
        CMP.B   #'9',D0
        BLS     .num
        AND.B   #$DF,D0
        CMP.B   #'A',D0
        BCS     .no
        CMP.B   #'F',D0
        BHI     .no
        SUB.B   #'A'-10,D0
        RTS
.num:   SUB.B   #'0',D0
        RTS
.no:    MOVEQ   #-1,D0
        RTS

; decimal digit at the position D6 -> D0.L = 0-9 (flags: PL), or -1 (MI) at the end or another character
rt_val__dec:
        CMP.L   D7,D6
        BCC     .no
        MOVEQ   #0,D0
        MOVE.B  0(A2,D6.L),D0
        SUB.B   #'0',D0
        CMP.B   #9,D0
        BHI     .no
        TST.L   D0
        RTS
.no:    MOVEQ   #-1,D0
        RTS

;! section bss
rt_val__neg:
        DS.W    1
rt_val__any:
        DS.W    1
rt_val__tail:
        DS.W    1
rt_val__pad:
        DS.W    1
rt_val__used:
        DS.L    1
rt_val__num:
        DS.L    rt_val__MAXN
rt_val__den:
        DS.L    rt_val__MAXN
rt_val__dig:
        DS.B    rt_val__DIGITS
