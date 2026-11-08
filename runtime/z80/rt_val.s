;! module    rt_val
;! provides  rt_val rt_val_used
;! requires  ix_bn_zero ix_bn_copy ix_bn_mul10 ix_bn_mul100 ix_bn_addb ix_bn_shl ix_bn_shr ix_bn_shr1 ix_bn_cmp ix_bn_sub ix_bn_iszero ix_bn_bitlen
;! requires  fp_round_pack fp_from_i32 fp_rm fp_re fp_rn
;! group     float
;! clobbers  AF BC DE HL
;! doc       VAL and VAL?: text -> float, correctly rounded; &H $ &O &X % prefixes (D30: 40 digits kept).

; HL = result (binary64), DE = characters, BC = length -> rt_val_used = characters read (VAL?).
; As runtime/ref/rt_val.c, but at most 40 significant digits are kept (further non-zero digits set
; the sticky bit): exact for inputs with up to 40 significant digits (D30).

rt_val__NDIG EQU 40

rt_val:
        LD      (rt_val__dst),HL
        LD      (rt_val__s),DE
        LD      (rt_val__len),BC
        LD      HL,0
        LD      (rt_val__i),HL
        LD      (rt_val_used),HL
        LD      (rt_val__dexp),HL
        LD      (rt_val__ex),HL
        XOR     A
        LD      (rt_val__nd),A
        LD      (rt_val__neg),A
        LD      (rt_val__any),A
        LD      (rt_val__tail),A
rt_val__sp:                             ; spaces
        CALL    rt_val__peek
        JP      C,rt_val__nodig
        CP      ' '
        JP      NZ,rt_val__pre
        CALL    rt_val__next
        JP      rt_val__sp
rt_val__pre:                            ; radix prefixes (no sign before them, probe6 T2)
        LD      C,16
        CP      '$'
        JP      Z,rt_val__r1
        LD      C,2
        CP      '%'
        JP      Z,rt_val__r1
        CP      '&'
        JP      NZ,rt_val__sign
        CALL    rt_val__next
        CALL    rt_val__peek
        JP      C,rt_val__zero
        AND     &DF                     ; upper case
        LD      C,16
        CP      'H'
        JP      Z,rt_val__r1
        LD      C,8
        CP      'O'
        JP      Z,rt_val__r1
        LD      C,2
        CP      'X'
        JP      NZ,rt_val__zero
rt_val__r1:
        CALL    rt_val__next
        LD      A,C
        LD      (rt_val__base),A
        LD      HL,0                    ; v = 0 (32 bit)
        LD      (rt_val__v),HL
        LD      (rt_val__v+2),HL
rt_val__r2:
        CALL    rt_val__peek
        JP      C,rt_val__r3
        CALL    rt_val__digit           ; A = value, CF if no digit of the base
        JP      C,rt_val__r3
        LD      (rt_val__d),A
        LD      A,1
        LD      (rt_val__any),A
        LD      A,(rt_val__base)        ; v = v * base + d: shift by 1, 3 or 4
        LD      B,1
        CP      2
        JP      Z,rt_val__r4
        LD      B,3
        CP      8
        JP      Z,rt_val__r4
        LD      B,4
rt_val__r4:
        LD      HL,rt_val__v
        OR      A
        RL      (HL)
        INC     HL
        RL      (HL)
        INC     HL
        RL      (HL)
        INC     HL
        RL      (HL)
        DJNZ    rt_val__r4
        LD      A,(rt_val__d)
        LD      HL,rt_val__v
        OR      (HL)
        LD      (HL),A
        CALL    rt_val__next
        JP      rt_val__r2
rt_val__r3:
        LD      A,(rt_val__any)
        OR      A
        JP      Z,rt_val__zero0         ; no digit: 0, nothing read
        LD      HL,(rt_val__i)
        LD      (rt_val_used),HL
        LD      HL,(rt_val__dst)
        LD      DE,rt_val__v
        JP      fp_from_i32
rt_val__zero0:
        LD      HL,0
        LD      (rt_val_used),HL
        JP      rt_val__zero
rt_val__nodig:
        JP      rt_val__zero
rt_val__sign:
        CP      '-'
        JP      Z,rt_val__neg1
        CP      '+'
        JP      NZ,rt_val__int
        JP      rt_val__sg2
rt_val__neg1:
        LD      A,1
        LD      (rt_val__neg),A
rt_val__sg2:
        CALL    rt_val__next
rt_val__int:                            ; digits before the point
        CALL    rt_val__peek
        JP      C,rt_val__point
        CP      '0'
        JP      C,rt_val__point
        CP      '9'+1
        JP      NC,rt_val__point
        CALL    rt_val__keep            ; leading zeros skipped, beyond 40 digits: dexp + 1
        JP      NC,rt_val__i2
        LD      HL,(rt_val__dexp)
        INC     HL
        LD      (rt_val__dexp),HL
rt_val__i2:
        CALL    rt_val__next
        JP      rt_val__int
rt_val__point:
        CALL    rt_val__peek
        JP      C,rt_val__exp
        CP      '.'
        JP      NZ,rt_val__exp
        CALL    rt_val__next
rt_val__frac:
        CALL    rt_val__peek
        JP      C,rt_val__exp
        CP      '0'
        JP      C,rt_val__exp
        CP      '9'+1
        JP      NC,rt_val__exp
        CALL    rt_val__keep            ; kept or a leading zero: dexp - 1
        JP      C,rt_val__f2
        LD      HL,(rt_val__dexp)
        DEC     HL
        LD      (rt_val__dexp),HL
rt_val__f2:
        CALL    rt_val__next
        JP      rt_val__frac
rt_val__exp:
        LD      A,(rt_val__any)
        OR      A
        JP      Z,rt_val__zero
        CALL    rt_val__peek
        JP      C,rt_val__done
        AND     &DF
        CP      'E'
        JP      NZ,rt_val__done
        CALL    rt_val__next            ; consumed even without digits: VAL?("1E") = 2
        XOR     A
        LD      (rt_val__eneg),A
        CALL    rt_val__peek
        JP      C,rt_val__ed
        CP      '-'
        JP      Z,rt_val__es
        CP      '+'
        JP      NZ,rt_val__ed
rt_val__es:                             ; a sign only before a digit
        LD      C,A
        LD      HL,(rt_val__i)
        INC     HL
        LD      DE,(rt_val__len)
        OR      A
        SBC     HL,DE
        JP      NC,rt_val__ed
        LD      HL,(rt_val__s)
        LD      DE,(rt_val__i)
        ADD     HL,DE
        INC     HL
        LD      A,(HL)
        CP      '0'
        JP      C,rt_val__ed
        CP      '9'+1
        JP      NC,rt_val__ed
        LD      A,C
        CP      '-'
        JP      NZ,rt_val__ep
        LD      A,1
        LD      (rt_val__eneg),A
rt_val__ep:
        CALL    rt_val__next
rt_val__ed:                             ; the exponent digits (from 3000 on it no longer grows)
        CALL    rt_val__peek
        JP      C,rt_val__ee
        SUB     '0'
        JP      C,rt_val__ee
        CP      10
        JP      NC,rt_val__ee
        LD      E,A
        LD      D,0
        LD      HL,(rt_val__ex)
        LD      BC,3000
        PUSH    HL
        OR      A
        SBC     HL,BC
        POP     HL
        JP      NC,rt_val__en
        ADD     HL,HL                   ; ex * 10 + d
        LD      B,H
        LD      C,L
        ADD     HL,HL
        ADD     HL,HL
        ADD     HL,BC
        ADD     HL,DE
        LD      (rt_val__ex),HL
rt_val__en:
        CALL    rt_val__next
        JP      rt_val__ed
rt_val__ee:
        LD      HL,(rt_val__ex)
        LD      A,(rt_val__eneg)
        OR      A
        JP      Z,rt_val__ea
        XOR     A
        SUB     L
        LD      L,A
        SBC     A,A
        SUB     H
        LD      H,A
rt_val__ea:
        LD      DE,(rt_val__dexp)
        ADD     HL,DE
        LD      (rt_val__dexp),HL
rt_val__done:
        LD      HL,(rt_val__i)
        LD      (rt_val_used),HL
rt_val__tz:                             ; trailing zeros into the exponent
        LD      A,(rt_val__nd)
        OR      A
        JP      Z,rt_val__zero
        LD      E,A
        LD      D,0
        LD      HL,rt_val__dig-1
        ADD     HL,DE
        LD      A,(HL)
        OR      A
        JP      NZ,rt_val__top
        LD      HL,rt_val__nd
        DEC     (HL)
        LD      HL,(rt_val__dexp)
        INC     HL
        LD      (rt_val__dexp),HL
        JP      rt_val__tz
rt_val__top:                            ; top = nd + dexp - 1 in -330 .. 309, else 0 (probe3 T20)
        LD      A,(rt_val__nd)
        LD      E,A
        LD      D,0
        LD      HL,(rt_val__dexp)
        ADD     HL,DE
        DEC     HL
        LD      DE,330
        ADD     HL,DE                   ; top + 330 in 0 .. 639
        BIT     7,H
        JP      NZ,rt_val__zero
        LD      DE,640
        OR      A
        SBC     HL,DE
        JP      NC,rt_val__zero
        LD      HL,rt_val__num          ; num = the digits
        CALL    ix_bn_zero
        LD      A,(rt_val__nd)
        LD      B,A
        LD      DE,rt_val__dig
rt_val__nm:
        PUSH    BC
        PUSH    DE
        LD      HL,rt_val__num
        CALL    ix_bn_mul10
        POP     DE
        LD      A,(DE)
        INC     DE
        PUSH    DE
        LD      HL,rt_val__num
        CALL    ix_bn_addb
        POP     DE
        POP     BC
        DJNZ    rt_val__nm
        LD      HL,(rt_val__dexp)
        BIT     7,H
        JP      NZ,rt_val__small
        LD      DE,rt_val__num          ; num *= 10^dexp
        CALL    rt_val__pow10
        LD      HL,rt_val__num
        CALL    ix_bn_bitlen            ; bits >= 64: the top 64 bits, e = bits - 64
        LD      DE,64
        OR      A
        SBC     HL,DE
        JP      C,rt_val__fits
        LD      (fp_re),HL
        LD      B,H
        LD      C,L
        LD      HL,rt_val__num
        CALL    ix_bn_shr
        JP      rt_val__pack
rt_val__fits:
        LD      HL,0
        LD      (fp_re),HL
        XOR     A
rt_val__pack:                           ; A = sticky; m = num bytes 0..7
        LD      C,A
        LD      A,(rt_val__tail)
        OR      C
        LD      (rt_val__st),A
        LD      HL,rt_val__num
        LD      DE,fp_rm
        LD      BC,8
        LDIR
rt_val__fin:
        LD      A,(rt_val__st)
        OR      A
        JP      Z,rt_val__f1
        LD      HL,fp_rm
        SET     0,(HL)
rt_val__f1:
        LD      A,(rt_val__neg)
        LD      (fp_rn),A
        LD      HL,(rt_val__dst)
        CALL    fp_round_pack
        RET     NC                      ; overflow: 0
rt_val__zero:
        LD      HL,(rt_val__dst)
        LD      B,8
rt_val__z:
        LD      (HL),0
        INC     HL
        DJNZ    rt_val__z
        RET
rt_val__small:                          ; q = floor(num * 2^k / 10^-dexp), 2^62 <= q < 2^64
        LD      HL,rt_val__den
        CALL    ix_bn_zero
        LD      A,1
        LD      (rt_val__den),A
        LD      HL,(rt_val__dexp)
        XOR     A
        SUB     L
        LD      L,A
        SBC     A,A
        SUB     H
        LD      H,A
        LD      DE,rt_val__den
        CALL    rt_val__pow10
        LD      HL,rt_val__den          ; k = 63 + bitlen(den) - bitlen(num)
        CALL    ix_bn_bitlen
        PUSH    HL
        LD      HL,rt_val__num
        CALL    ix_bn_bitlen
        EX      DE,HL
        POP     HL
        OR      A
        SBC     HL,DE
        LD      DE,63
        ADD     HL,DE
        LD      (rt_val__k),HL
        BIT     7,H
        JP      NZ,rt_val__kn
        LD      B,H
        LD      C,L
        LD      HL,rt_val__num
        CALL    ix_bn_shl
        JP      rt_val__div
rt_val__kn:
        XOR     A
        SUB     L
        LD      C,A
        SBC     A,A
        SUB     H
        LD      B,A
        LD      HL,rt_val__den
        CALL    ix_bn_shl
rt_val__div:                            ; q (64 bits) = num / den, t = den << 63
        LD      HL,rt_val__den
        LD      DE,rt_val__t
        CALL    ix_bn_copy
        LD      HL,rt_val__t
        LD      BC,63
        CALL    ix_bn_shl
        LD      HL,fp_rm
        LD      B,8
rt_val__q0:
        LD      (HL),0
        INC     HL
        DJNZ    rt_val__q0
        LD      A,64
rt_val__q1:
        PUSH    AF
        LD      HL,fp_rm
        OR      A
        LD      B,8
rt_val__q2:
        RL      (HL)
        INC     HL
        DJNZ    rt_val__q2
        LD      HL,rt_val__num
        LD      DE,rt_val__t
        CALL    ix_bn_cmp
        JP      C,rt_val__q3
        LD      HL,rt_val__num
        LD      DE,rt_val__t
        CALL    ix_bn_sub
        LD      HL,fp_rm
        SET     0,(HL)
rt_val__q3:
        LD      HL,rt_val__t
        CALL    ix_bn_shr1
        POP     AF
        DEC     A
        JP      NZ,rt_val__q1
        LD      HL,rt_val__num          ; sticky: a rest or the tail
        CALL    ix_bn_iszero
        LD      A,0
        JP      Z,rt_val__s1
        INC     A
rt_val__s1:
        LD      C,A
        LD      A,(rt_val__tail)
        OR      C
        LD      (rt_val__st),A
        LD      HL,(rt_val__k)          ; e = -k
        XOR     A
        SUB     L
        LD      L,A
        SBC     A,A
        SUB     H
        LD      H,A
        LD      (fp_re),HL
        JP      rt_val__fin

; DE = big number, HL = k >= 0: *= 10^k
rt_val__pow10:
        LD      (rt_val__pk),HL
        LD      (rt_val__pp),DE
rt_val__pw:
        LD      HL,(rt_val__pk)
        LD      A,H
        OR      A
        JP      NZ,rt_val__p100
        LD      A,L
        CP      2
        JP      C,rt_val__plast
rt_val__p100:
        DEC     HL
        DEC     HL
        LD      (rt_val__pk),HL
        LD      HL,(rt_val__pp)
        CALL    ix_bn_mul100
        JP      rt_val__pw
rt_val__plast:
        OR      A
        RET     Z
        LD      HL,(rt_val__pp)
        JP      ix_bn_mul10

; A = digit character: keep it (NC: kept or skipped as a leading zero in the integer part ... see below)
; Integer part: CF set if the digit was beyond the 40 kept ones (dexp + 1 by the caller).
; Fraction: CF set if it was beyond them (no dexp change), NC if kept or a leading zero (dexp - 1).
rt_val__keep:
        SUB     '0'
        LD      C,A
        LD      A,1
        LD      (rt_val__any),A
        LD      A,(rt_val__nd)
        OR      A
        JP      NZ,rt_val__k1
        LD      A,C                     ; no digit yet and a zero: skipped
        OR      A
        RET     Z                       ; NC
rt_val__k1:
        LD      A,(rt_val__nd)
        CP      rt_val__NDIG
        JP      NC,rt_val__k2
        LD      E,A
        LD      D,0
        LD      HL,rt_val__dig
        ADD     HL,DE
        LD      (HL),C
        INC     A
        LD      (rt_val__nd),A
        OR      A                       ; NC
        RET
rt_val__k2:
        LD      A,C                     ; beyond: the tail
        OR      A
        JP      Z,rt_val__k3
        LD      A,1
        LD      (rt_val__tail),A
rt_val__k3:
        SCF
        RET

; -> A = the character at i, CF if at the end
rt_val__peek:
        LD      HL,(rt_val__i)
        LD      DE,(rt_val__len)
        OR      A
        SBC     HL,DE
        CCF
        RET     C
        LD      HL,(rt_val__s)
        LD      DE,(rt_val__i)
        ADD     HL,DE
        LD      A,(HL)
        OR      A                       ; NC
        RET

rt_val__next:
        LD      HL,(rt_val__i)
        INC     HL
        LD      (rt_val__i),HL
        RET

; A = character -> A = its value, CF if it is no digit of rt_val__base
rt_val__digit:
        CP      '0'
        JP      C,rt_val__nd0
        CP      '9'+1
        JP      C,rt_val__dd
        AND     &DF
        CP      'A'
        JP      C,rt_val__nd0
        CP      'F'+1
        JP      NC,rt_val__nd0
        SUB     'A'-10-'0'
rt_val__dd:
        SUB     '0'
        LD      B,A
        LD      A,(rt_val__base)
        DEC     A
        CP      B
        LD      A,B
        RET                             ; CF if base - 1 < value
rt_val__nd0:
        SCF
        RET

;! section bss
rt_val_used:
        DS      2
rt_val__dst:
        DS      2
rt_val__s:
        DS      2
rt_val__len:
        DS      2
rt_val__i:
        DS      2
rt_val__dexp:
        DS      2
rt_val__ex:
        DS      2
rt_val__k:
        DS      2
rt_val__pk:
        DS      2
rt_val__pp:
        DS      2
rt_val__nd:
        DS      1
rt_val__neg:
        DS      1
rt_val__eneg:
        DS      1
rt_val__any:
        DS      1
rt_val__tail:
        DS      1
rt_val__st:
        DS      1
rt_val__base:
        DS      1
rt_val__d:
        DS      1
rt_val__v:
        DS      4
rt_val__dig:
        DS      rt_val__NDIG
rt_val__num:
        DS      176
rt_val__den:
        DS      176
rt_val__t:
        DS      176
