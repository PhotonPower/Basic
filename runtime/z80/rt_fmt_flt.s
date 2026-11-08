;! module    rt_fmt_flt
;! provides  rt_fmt_flt
;! requires  rt_flt_dec rt_flt_dec_d
;! group     float
;! clobbers  AF BC DE HL
;! doc       Float -> text as PRINT/STR$ (13 significant digits or DEFNUM), from the exact binary value.

; HL = buffer (at least 24 bytes), DE = binary64, A = digits -> BC = length of the text.
; Rules (runtime/ref/rt_fmt_flt.c): rounded to `digits` digits, ties away from 0; without exponent
; for -5 <= d <= 13, else d.ddd E+dd; trailing zeros of the fraction dropped; "0.5"; no space.
rt_fmt_flt:
        LD      (rt_fmt_flt__o),HL
        LD      (rt_fmt_flt__start),HL
        CP      2                       ; DEFNUM 1 prints 2 digits (probe6 T8), at most 13
        JR      NC,rt_fmt_flt__d1
        LD      A,2
rt_fmt_flt__d1:
        CP      14
        JR      C,rt_fmt_flt__d2
        LD      A,13
rt_fmt_flt__d2:
        LD      (rt_fmt_flt__n),A
        PUSH    DE                      ; 0: "0"
        LD      HL,6
        ADD     HL,DE
        LD      A,(HL)
        AND     &F0
        LD      C,A
        INC     HL
        LD      A,(HL)
        AND     &7F
        OR      C
        POP     DE
        JR      NZ,rt_fmt_flt__nz
        LD      A,'0'
        CALL    rt_fmt_flt__put
        JP      rt_fmt_flt__end
rt_fmt_flt__nz:
        LD      HL,rt_fmt_flt__ds
        LD      A,(rt_fmt_flt__n)
        CALL    rt_flt_dec
        OR      A
        JR      Z,rt_fmt_flt__pos
        LD      A,'-'
        CALL    rt_fmt_flt__put
rt_fmt_flt__pos:
        LD      A,(rt_fmt_flt__n)       ; len: without trailing zeros (at least 1)
        LD      C,A
rt_fmt_flt__tz:
        LD      A,C
        CP      1
        JR      Z,rt_fmt_flt__tz2
        LD      HL,rt_fmt_flt__ds-1
        LD      B,0
        ADD     HL,BC
        LD      A,(HL)
        CP      '0'
        JR      NZ,rt_fmt_flt__tz2
        DEC     C
        JR      rt_fmt_flt__tz
rt_fmt_flt__tz2:
        LD      A,C
        LD      (rt_fmt_flt__len),A
        LD      HL,(rt_flt_dec_d)       ; -5 <= d <= 13: without exponent
        LD      DE,5
        ADD     HL,DE                   ; d + 5 in 0..18
        BIT     7,H
        JP      NZ,rt_fmt_flt__exp
        LD      A,H
        OR      A
        JP      NZ,rt_fmt_flt__exp
        LD      A,L
        CP      19
        JP      NC,rt_fmt_flt__exp
        LD      A,(rt_flt_dec_d+1)
        BIT     7,A
        JR      NZ,rt_fmt_flt__neg
        LD      A,(rt_flt_dec_d)        ; d >= 0: digits 0..d (zeros after len), then .rest
        INC     A
        LD      C,A                     ; C = d + 1
        LD      B,0                     ; B = index
rt_fmt_flt__i1:
        LD      A,(rt_fmt_flt__len)
        CP      B
        LD      A,'0'
        JR      Z,rt_fmt_flt__i2
        JR      C,rt_fmt_flt__i2
        LD      HL,rt_fmt_flt__ds
        LD      E,B
        LD      D,0
        ADD     HL,DE
        LD      A,(HL)
rt_fmt_flt__i2:
        CALL    rt_fmt_flt__put
        INC     B
        LD      A,B
        CP      C
        JR      NZ,rt_fmt_flt__i1
        LD      A,(rt_fmt_flt__len)     ; len > d + 1: the fraction
        CP      B
        JP      Z,rt_fmt_flt__end
        JP      C,rt_fmt_flt__end
        LD      A,'.'
        CALL    rt_fmt_flt__put
rt_fmt_flt__f1:
        LD      HL,rt_fmt_flt__ds
        LD      E,B
        LD      D,0
        ADD     HL,DE
        LD      A,(HL)
        CALL    rt_fmt_flt__put
        INC     B
        LD      A,(rt_fmt_flt__len)
        CP      B
        JR      NZ,rt_fmt_flt__f1
        JP      rt_fmt_flt__end
rt_fmt_flt__neg:                        ; d < 0: "0." zeros, digits
        LD      A,'0'
        CALL    rt_fmt_flt__put
        LD      A,'.'
        CALL    rt_fmt_flt__put
        LD      A,(rt_flt_dec_d)
        NEG
        DEC     A                       ; -d - 1 zeros
        JR      Z,rt_fmt_flt__n2
        LD      B,A
rt_fmt_flt__n1:
        LD      A,'0'
        CALL    rt_fmt_flt__put
        DJNZ    rt_fmt_flt__n1
rt_fmt_flt__n2:
        LD      HL,rt_fmt_flt__ds
        LD      A,(rt_fmt_flt__len)
        LD      B,A
rt_fmt_flt__n3:
        LD      A,(HL)
        CALL    rt_fmt_flt__put
        INC     HL
        DJNZ    rt_fmt_flt__n3
        JP      rt_fmt_flt__end
rt_fmt_flt__exp:                        ; d.ddd E+dd
        LD      A,(rt_fmt_flt__ds)
        CALL    rt_fmt_flt__put
        LD      A,'.'
        CALL    rt_fmt_flt__put
        LD      A,(rt_fmt_flt__len)
        CP      1
        JR      NZ,rt_fmt_flt__x1
        LD      A,'0'
        CALL    rt_fmt_flt__put
        JR      rt_fmt_flt__x3
rt_fmt_flt__x1:
        LD      HL,rt_fmt_flt__ds+1
        DEC     A
        LD      B,A
rt_fmt_flt__x2:
        LD      A,(HL)
        CALL    rt_fmt_flt__put
        INC     HL
        DJNZ    rt_fmt_flt__x2
rt_fmt_flt__x3:
        LD      A,'E'
        CALL    rt_fmt_flt__put
        LD      HL,(rt_flt_dec_d)
        LD      A,'+'
        BIT     7,H
        JR      Z,rt_fmt_flt__x4
        XOR     A
        SUB     L
        LD      L,A
        SBC     A,A
        SUB     H
        LD      H,A
        LD      A,'-'
rt_fmt_flt__x4:
        CALL    rt_fmt_flt__put
        LD      DE,100                  ; |d| <= 324: hundreds only if needed, then two digits
        LD      B,'0'-1
        OR      A
rt_fmt_flt__h:
        INC     B
        SBC     HL,DE
        JR      NC,rt_fmt_flt__h
        ADD     HL,DE
        LD      A,B
        CP      '0'
        JR      Z,rt_fmt_flt__t
        CALL    rt_fmt_flt__put
rt_fmt_flt__t:
        LD      A,L
        LD      B,'0'-1
rt_fmt_flt__t1:
        INC     B
        SUB     10
        JR      NC,rt_fmt_flt__t1
        ADD     A,10+'0'
        LD      C,A
        LD      A,B
        CALL    rt_fmt_flt__put
        LD      A,C
        CALL    rt_fmt_flt__put
rt_fmt_flt__end:
        LD      HL,(rt_fmt_flt__o)
        LD      DE,(rt_fmt_flt__start)
        OR      A
        SBC     HL,DE
        LD      B,H
        LD      C,L
        RET

rt_fmt_flt__put:
        PUSH    HL
        LD      HL,(rt_fmt_flt__o)
        LD      (HL),A
        INC     HL
        LD      (rt_fmt_flt__o),HL
        POP     HL
        RET

;! section bss
rt_fmt_flt__o:
        DS      2
rt_fmt_flt__start:
        DS      2
rt_fmt_flt__n:
        DS      1
rt_fmt_flt__len:
        DS      1
rt_fmt_flt__ds:
        DS      16
