;! module    rt_flt_dec
;! provides  rt_flt_dec rt_flt_dec_d
;! requires  ix_bn_zero ix_bn_copy ix_bn_mul10 ix_bn_mul100 ix_bn_shl ix_bn_shr1 ix_bn_cmp ix_bn_sub fp_w_unpack fp_xa zi_mulv
;! group     float
;! clobbers  AF BC DE HL
;! doc       Decimal digits of a float: exact binary value rounded to n significant digits, ties away from 0.

; HL = buffer for the digits (no terminator), DE = binary64, A = digits (1..13)
; -> rt_flt_dec_d = decimal exponent d of the first digit (16 bit), A = 1 if x < 0. For 0: all '0', d = 0.
; As runtime/ref/rt_flt_dec.c: q = floor(|x| * 10^s), s = digits-1-d, 10^(digits-1) <= q < 10^digits.

rt_flt_dec:
        LD      (rt_flt_dec__buf),HL
        LD      (rt_flt_dec__n),A
        EX      DE,HL
        LD      DE,fp_xa
        CALL    fp_w_unpack
        LD      A,(fp_xa+11)
        OR      A
        JR      Z,rt_flt_dec__nz
        LD      HL,(rt_flt_dec__buf)    ; 0: all digits '0'
        LD      A,(rt_flt_dec__n)
        LD      B,A
rt_flt_dec__z:
        LD      (HL),'0'
        INC     HL
        DJNZ    rt_flt_dec__z
        LD      HL,0
        LD      (rt_flt_dec_d),HL
        XOR     A
        RET
rt_flt_dec__nz:
        LD      HL,rt_flt_dec__phi      ; 10^digits and 10^(digits-1)
        CALL    rt_flt_dec__one
        LD      A,(rt_flt_dec__n)
        LD      B,A
rt_flt_dec__p1:
        PUSH    BC
        LD      HL,rt_flt_dec__phi
        LD      DE,rt_flt_dec__plo
        LD      BC,8
        LDIR
        LD      HL,rt_flt_dec__phi
        CALL    rt_flt_dec__mul10
        POP     BC
        DJNZ    rt_flt_dec__p1
        LD      HL,(fp_xa+8)            ; d = floor((e + 52) * 78913 / 2^18)
        LD      DE,52
        ADD     HL,DE
        LD      (rt_flt_dec__t),HL
        LD      A,H
        RLCA
        SBC     A,A
        LD      (rt_flt_dec__t+2),A
        LD      (rt_flt_dec__t+3),A
        LD      HL,rt_flt_dec__t
        LD      DE,rt_flt_dec__t
        LD      BC,rt_flt_dec__k
        CALL    zi_mulv
        LD      HL,(rt_flt_dec__t+2)
        SRA     H
        RR      L
        SRA     H
        RR      L
        LD      (rt_flt_dec_d),HL
rt_flt_dec__loop:
        LD      A,(rt_flt_dec__n)       ; s = digits - 1 - d
        DEC     A
        LD      E,A
        LD      D,0
        LD      HL,(rt_flt_dec_d)
        EX      DE,HL
        OR      A
        SBC     HL,DE
        LD      (rt_flt_dec__s),HL
        LD      HL,rt_flt_dec__num      ; num = m, den = 1
        CALL    ix_bn_zero
        LD      HL,fp_xa
        LD      DE,rt_flt_dec__num
        LD      BC,8
        LDIR
        LD      HL,rt_flt_dec__den
        CALL    ix_bn_zero
        LD      A,1
        LD      (rt_flt_dec__den),A
        LD      HL,(rt_flt_dec__s)      ; s >= 0: num *= 10^s, else den *= 10^-s
        LD      DE,rt_flt_dec__num
        BIT     7,H
        JR      Z,rt_flt_dec__s1
        XOR     A
        SUB     L
        LD      L,A
        SBC     A,A
        SUB     H
        LD      H,A
        LD      DE,rt_flt_dec__den
rt_flt_dec__s1:
        CALL    rt_flt_dec__pow10
        LD      HL,(fp_xa+8)            ; e >= 0: num <<= e, else den <<= -e
        LD      DE,rt_flt_dec__num
        BIT     7,H
        JR      Z,rt_flt_dec__e1
        XOR     A
        SUB     L
        LD      L,A
        SBC     A,A
        SUB     H
        LD      H,A
        LD      DE,rt_flt_dec__den
rt_flt_dec__e1:
        LD      B,H
        LD      C,L
        EX      DE,HL
        CALL    ix_bn_shl
        CALL    rt_flt_dec__divq        ; q = floor(num / den), num = the rest
        LD      HL,rt_flt_dec__q        ; q >= 10^digits: d + 1
        LD      DE,rt_flt_dec__phi
        CALL    rt_flt_dec__cmp8
        JR      C,rt_flt_dec__lo
        LD      HL,(rt_flt_dec_d)
        INC     HL
        LD      (rt_flt_dec_d),HL
        JP      rt_flt_dec__loop
rt_flt_dec__lo:
        LD      HL,rt_flt_dec__q        ; q < 10^(digits-1): d - 1
        LD      DE,rt_flt_dec__plo
        CALL    rt_flt_dec__cmp8
        JR      NC,rt_flt_dec__round
        LD      HL,(rt_flt_dec_d)
        DEC     HL
        LD      (rt_flt_dec_d),HL
        JP      rt_flt_dec__loop
rt_flt_dec__round:                      ; 2 * rest >= den: one more, ties away from 0
        LD      HL,rt_flt_dec__num
        LD      BC,1
        CALL    ix_bn_shl
        LD      HL,rt_flt_dec__num
        LD      DE,rt_flt_dec__den
        CALL    ix_bn_cmp
        JR      C,rt_flt_dec__dig
        LD      HL,rt_flt_dec__q
        LD      B,8
rt_flt_dec__inc:
        INC     (HL)
        JR      NZ,rt_flt_dec__i2
        INC     HL
        DJNZ    rt_flt_dec__inc
rt_flt_dec__i2:
        LD      HL,rt_flt_dec__q        ; 10^digits: 10^(digits-1), d + 1
        LD      DE,rt_flt_dec__phi
        CALL    rt_flt_dec__cmp8
        JR      NZ,rt_flt_dec__dig
        LD      HL,rt_flt_dec__plo
        LD      DE,rt_flt_dec__q
        LD      BC,8
        LDIR
        LD      HL,(rt_flt_dec_d)
        INC     HL
        LD      (rt_flt_dec_d),HL
rt_flt_dec__dig:                        ; the digits of q from the right
        LD      A,(rt_flt_dec__n)
        LD      E,A
        LD      D,0
        LD      HL,(rt_flt_dec__buf)
        ADD     HL,DE
        LD      B,A
rt_flt_dec__d1:
        DEC     HL
        PUSH    HL
        PUSH    BC
        CALL    rt_flt_dec__div10       ; A = q mod 10, q /= 10
        POP     BC
        POP     HL
        ADD     A,'0'
        LD      (HL),A
        DJNZ    rt_flt_dec__d1
        LD      A,(fp_xa+10)
        RET

; HL = 8 bytes: 1
rt_flt_dec__one:
        LD      (HL),1
        LD      B,7
rt_flt_dec__o1:
        INC     HL
        LD      (HL),0
        DJNZ    rt_flt_dec__o1
        RET

; HL = 8 bytes: *= 10
rt_flt_dec__mul10:
        LD      B,8
        LD      C,0
rt_flt_dec__m1:
        PUSH    HL
        LD      L,(HL)
        LD      H,0
        ADD     HL,HL
        LD      D,H
        LD      E,L
        ADD     HL,HL
        ADD     HL,HL
        ADD     HL,DE
        LD      E,C
        LD      D,0
        ADD     HL,DE
        LD      C,H
        LD      A,L
        POP     HL
        LD      (HL),A
        INC     HL
        DJNZ    rt_flt_dec__m1
        RET

; HL = a, DE = b (8 bytes): CF if a < b, Z if equal
rt_flt_dec__cmp8:
        LD      BC,7
        ADD     HL,BC
        EX      DE,HL
        ADD     HL,BC
        EX      DE,HL
        LD      B,8
rt_flt_dec__c1:
        LD      A,(DE)
        LD      C,A
        LD      A,(HL)
        CP      C
        RET     NZ
        DEC     HL
        DEC     DE
        DJNZ    rt_flt_dec__c1
        RET

; DE = big number, HL = k >= 0: *= 10^k
rt_flt_dec__pow10:
        LD      (rt_flt_dec__pk),HL
        LD      (rt_flt_dec__pp),DE
rt_flt_dec__pw:
        LD      HL,(rt_flt_dec__pk)
        LD      A,H
        OR      A
        JR      NZ,rt_flt_dec__p100
        LD      A,L
        CP      2
        JR      C,rt_flt_dec__plast
rt_flt_dec__p100:
        DEC     HL
        DEC     HL
        LD      (rt_flt_dec__pk),HL
        LD      HL,(rt_flt_dec__pp)
        CALL    ix_bn_mul100
        JR      rt_flt_dec__pw
rt_flt_dec__plast:
        OR      A
        RET     Z
        LD      HL,(rt_flt_dec__pp)
        JP      ix_bn_mul10

; q = floor(num / den) (< 2^51), num = the rest: t = den << 50, then 51 steps with t >>= 1
rt_flt_dec__divq:
        LD      HL,rt_flt_dec__den
        LD      DE,rt_flt_dec__t
        CALL    ix_bn_copy
        LD      HL,rt_flt_dec__t
        LD      BC,50
        CALL    ix_bn_shl
        LD      HL,rt_flt_dec__q
        LD      B,8
rt_flt_dec__q0:
        LD      (HL),0
        INC     HL
        DJNZ    rt_flt_dec__q0
        LD      A,51
rt_flt_dec__q1:
        PUSH    AF
        LD      HL,rt_flt_dec__q        ; q <<= 1
        OR      A
        LD      B,8
rt_flt_dec__q2:
        RL      (HL)
        INC     HL
        DJNZ    rt_flt_dec__q2
        LD      HL,rt_flt_dec__num
        LD      DE,rt_flt_dec__t
        CALL    ix_bn_cmp
        JR      C,rt_flt_dec__q3
        LD      HL,rt_flt_dec__num
        LD      DE,rt_flt_dec__t
        CALL    ix_bn_sub
        LD      HL,rt_flt_dec__q
        SET     0,(HL)
rt_flt_dec__q3:
        LD      HL,rt_flt_dec__t
        CALL    ix_bn_shr1
        POP     AF
        DEC     A
        JR      NZ,rt_flt_dec__q1
        RET

; q /= 10, A = the remainder
rt_flt_dec__div10:
        LD      HL,rt_flt_dec__q+7
        LD      B,8
        XOR     A
rt_flt_dec__v1:
        LD      C,(HL)
        PUSH    BC
        LD      B,8
        LD      E,C
rt_flt_dec__v2:
        SLA     E
        RLA
        CP      10
        JR      C,rt_flt_dec__v3
        SUB     10
        INC     E
rt_flt_dec__v3:
        DJNZ    rt_flt_dec__v2
        LD      (HL),E
        POP     BC
        DEC     HL
        DJNZ    rt_flt_dec__v1
        RET

;! section data
rt_flt_dec__k:
        DB      &41,&34,&01,&00         ; 78913

;! section bss
rt_flt_dec_d:
        DS      2
rt_flt_dec__buf:
        DS      2
rt_flt_dec__n:
        DS      1
rt_flt_dec__s:
        DS      2
rt_flt_dec__pk:
        DS      2
rt_flt_dec__pp:
        DS      2
rt_flt_dec__phi:
        DS      8
rt_flt_dec__plo:
        DS      8
rt_flt_dec__q:
        DS      8
rt_flt_dec__t:
        DS      176
rt_flt_dec__num:
        DS      176
rt_flt_dec__den:
        DS      176
