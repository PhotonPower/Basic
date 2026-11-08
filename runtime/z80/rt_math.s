;! module    rt_math
;! provides  rt_exp rt_log rt_log10 rt_sin rt_cos rt_tan rt_atn rt_asin rt_acos rt_powf rt_deg rt_rad
;! requires  fp_add fp_sub fp_mul fp_div fp_sqrt fp_cmp fp_int fp_neg fp_abs fp_to_i32 fp_from_i32 fp_finish fp_w_unpack fp_xa fp_rm fp_re fp_rn rt_error
;! group     float
;! clobbers  AF BC DE HL
;! doc       Transcendental functions: the same sequence of basic operations as runtime/ref/rt_math.c (bit for bit).

; All routines: HL = result, DE = x (rt_powf: BC = y). Generated from the description in the
; commit that added this module; every line is one basic operation of runtime/ref/rt_math.c.

; CALL rt_math__op / DW function, destination, a, b: function(HL = destination, DE = a, BC = b)
rt_math__op:
        EX      (SP),HL                 ; HL = the 8 bytes after the CALL
        PUSH    DE
        PUSH    BC
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        INC     HL
        LD      (rt_math__fp),DE
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        INC     HL
        LD      (rt_math__opd),DE
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        INC     HL
        LD      (rt_math__opa),DE
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        INC     HL
        LD      (rt_math__opb),DE
        POP     BC
        POP     DE
        EX      (SP),HL                 ; the return address after the data
        LD      HL,(rt_math__fp)
        PUSH    HL
        LD      HL,(rt_math__opd)
        LD      DE,(rt_math__opa)
        LD      BC,(rt_math__opb)
        RET                             ; into the function; it returns behind the data

; HL = table, B = number of coefficients, rt_math__z: rt_math__p = Horner from the first one
rt_math__horner:
        PUSH    BC
        LD      DE,rt_math__p
        LD      BC,8
        LDIR
        POP     BC
        DEC     B
rt_math__h1:
        PUSH    BC
        PUSH    HL
        LD      HL,rt_math__p
        LD      DE,rt_math__p
        LD      BC,rt_math__z
        CALL    fp_mul
        POP     BC                      ; BC = the coefficient
        PUSH    BC
        LD      HL,rt_math__p
        LD      DE,rt_math__p
        CALL    fp_add
        POP     HL
        LD      DE,8
        ADD     HL,DE
        POP     BC
        DJNZ    rt_math__h1
        RET

; HL = destination, DE = x (8 bytes): copy
rt_math__copy:
        EX      DE,HL
        LD      BC,8
        LDIR
        RET

; A = 1 if the comparison result in rt_math__cr is > 0, etc.: Z/NZ by the caller
; ---- EXP ----
rt_exp:
        LD      (rt_math__dst),HL
        LD      HL,rt_math__x
        CALL    rt_math__copy
        CALL    rt_math__op
        DW      fp_cmp,rt_math__cr,rt_math__x,rt_math__c_expmax
        LD      A,(rt_math__cr)
        DEC     A
        JR      NZ,rt_math__e1
        LD      HL,(rt_math__dst)
        CALL    rt_math__zero
        LD      A,1
        JP      rt_error
rt_math__e1:
        CALL    rt_math__op
        DW      fp_cmp,rt_math__cr,rt_math__x,rt_math__c_expmin
        LD      A,(rt_math__cr)
        CP      &FF
        JP      Z,rt_math__retzero
        CALL    rt_math__op
        DW      fp_mul,rt_math__t,rt_math__x,rt_math__c_invln2
        CALL    rt_math__op
        DW      fp_add,rt_math__t,rt_math__t,rt_math__c_half
        CALL    rt_math__op
        DW      fp_int,rt_math__kf,rt_math__t,0
        CALL    rt_math__op
        DW      fp_mul,rt_math__t,rt_math__kf,rt_math__c_ln2hi
        CALL    rt_math__op
        DW      fp_sub,rt_math__r,rt_math__x,rt_math__t
        CALL    rt_math__op
        DW      fp_mul,rt_math__t,rt_math__kf,rt_math__c_ln2lo
        CALL    rt_math__op
        DW      fp_sub,rt_math__r,rt_math__r,rt_math__t
        CALL    rt_math__op
        DW      rt_math__setz,rt_math__z,rt_math__r,0
        LD      HL,rt_math__t_expc
        LD      B,12
        CALL    rt_math__horner
        CALL    rt_math__op
        DW      fp_mul,rt_math__p,rt_math__p,rt_math__r
        CALL    rt_math__op
        DW      fp_add,rt_math__p,rt_math__p,rt_math__c_one
        CALL    rt_math__op
        DW      fp_mul,rt_math__p,rt_math__p,rt_math__r
        CALL    rt_math__op
        DW      fp_add,rt_math__p,rt_math__p,rt_math__c_one
        CALL    rt_math__op
        DW      fp_to_i32,rt_math__k,rt_math__kf,0
        LD      HL,(rt_math__dst)
        LD      DE,rt_math__p
        LD      BC,(rt_math__k)
        JP      rt_math__scale
; ---- LOG, LOG10 ----
rt_log10:
        PUSH    HL
        LD      HL,rt_math__l10
        CALL    rt_log
        POP     HL
        LD      DE,rt_math__l10
        LD      BC,rt_math__c_invln10
        JP      fp_mul
rt_log:
        LD      (rt_math__dst),HL
        EX      DE,HL
        LD      DE,fp_xa
        CALL    fp_w_unpack
        LD      A,(fp_xa+11)
        LD      C,A
        LD      A,(fp_xa+10)
        OR      C
        JR      Z,rt_math__l1
        LD      HL,(rt_math__dst)
        CALL    rt_math__zero
        LD      A,6
        JP      rt_error
rt_math__l1:
        LD      HL,(fp_xa+8)
        LD      DE,52
        ADD     HL,DE
        LD      (rt_math__e),HL
        LD      A,&F0
        CALL    rt_math__mant          ; m = 1023 << 52 | frac
        CALL    rt_math__op
        DW      fp_cmp,rt_math__cr,rt_math__m,rt_math__c_sqrt2
        LD      A,(rt_math__cr)
        DEC     A
        JR      NZ,rt_math__l2
        LD      A,&E0
        CALL    rt_math__mant          ; m = 1022 << 52 | frac
        LD      HL,(rt_math__e)
        INC     HL
        LD      (rt_math__e),HL
rt_math__l2:
        CALL    rt_math__op
        DW      fp_sub,rt_math__f,rt_math__m,rt_math__c_one
        CALL    rt_math__op
        DW      fp_add,rt_math__t,rt_math__c_two,rt_math__f
        CALL    rt_math__op
        DW      fp_div,rt_math__s,rt_math__f,rt_math__t
        CALL    rt_math__op
        DW      fp_mul,rt_math__z,rt_math__s,rt_math__s
        LD      HL,rt_math__t_logc
        LD      B,11
        CALL    rt_math__horner
        CALL    rt_math__op
        DW      fp_mul,rt_math__r,rt_math__p,rt_math__z
        CALL    rt_math__op
        DW      fp_sub,rt_math__t,rt_math__f,rt_math__r
        CALL    rt_math__op
        DW      fp_mul,rt_math__t,rt_math__s,rt_math__t
        CALL    rt_math__op
        DW      fp_sub,rt_math__l,rt_math__f,rt_math__t
        LD      HL,(rt_math__e)
        LD      (rt_math__k),HL
        LD      A,H
        RLCA
        SBC     A,A
        LD      (rt_math__k+2),A
        LD      (rt_math__k+3),A
        CALL    rt_math__op
        DW      fp_from_i32,rt_math__ef,rt_math__k,0
        CALL    rt_math__op
        DW      fp_mul,rt_math__t,rt_math__ef,rt_math__c_ln2lo
        CALL    rt_math__op
        DW      fp_add,rt_math__t,rt_math__t,rt_math__l
        CALL    rt_math__op
        DW      fp_mul,rt_math__r,rt_math__ef,rt_math__c_ln2hi
        LD      HL,(rt_math__dst)
        LD      DE,rt_math__r
        LD      BC,rt_math__t
        JP      fp_add
; ---- argument reduction: rt_math__x -> rt_math__r, rt_math__q = quadrant ----
rt_math__reduce:
        XOR     A
        LD      (rt_math__q),A
rt_math__rd:
        CALL    rt_math__op
        DW      fp_abs,rt_math__t,rt_math__x,0
        CALL    rt_math__op
        DW      fp_cmp,rt_math__cr,rt_math__t,rt_math__c_pio4
        LD      A,(rt_math__cr)
        DEC     A
        JR      Z,rt_math__rd1
        LD      HL,rt_math__r
        LD      DE,rt_math__x
        JP      rt_math__copy
rt_math__rd1:
        CALL    rt_math__op
        DW      fp_mul,rt_math__t,rt_math__x,rt_math__c_twoopi
        CALL    rt_math__op
        DW      fp_add,rt_math__t,rt_math__t,rt_math__c_half
        CALL    rt_math__op
        DW      fp_int,rt_math__fn,rt_math__t,0
        XOR     A                       ; clear the low 27 bits
        LD      (rt_math__fn),A
        LD      (rt_math__fn+1),A
        LD      (rt_math__fn+2),A
        LD      A,(rt_math__fn+3)
        AND     &F8
        LD      (rt_math__fn+3),A
        CALL    rt_math__op
        DW      fp_mul,rt_math__t,rt_math__fn,rt_math__c_quarter
        CALL    rt_math__op
        DW      fp_int,rt_math__t,rt_math__t,0
        CALL    rt_math__op
        DW      fp_mul,rt_math__t,rt_math__t,rt_math__c_four
        CALL    rt_math__op
        DW      fp_sub,rt_math__t,rt_math__fn,rt_math__t
        CALL    rt_math__op
        DW      fp_to_i32,rt_math__k,rt_math__t,0
        LD      A,(rt_math__k)
        LD      HL,rt_math__q
        ADD     A,(HL)
        AND     3
        LD      (HL),A
        CALL    rt_math__op
        DW      fp_mul,rt_math__t,rt_math__fn,rt_math__c_pio2a
        CALL    rt_math__op
        DW      fp_sub,rt_math__x,rt_math__x,rt_math__t
        CALL    rt_math__op
        DW      fp_mul,rt_math__t,rt_math__fn,rt_math__c_pio2b
        CALL    rt_math__op
        DW      fp_sub,rt_math__x,rt_math__x,rt_math__t
        CALL    rt_math__op
        DW      fp_mul,rt_math__t,rt_math__fn,rt_math__c_pio2c
        CALL    rt_math__op
        DW      fp_sub,rt_math__x,rt_math__x,rt_math__t
        CALL    rt_math__op
        DW      fp_mul,rt_math__t,rt_math__fn,rt_math__c_pio2d
        CALL    rt_math__op
        DW      fp_sub,rt_math__x,rt_math__x,rt_math__t
        JP      rt_math__rd
rt_math__ksin:                          ; rt_math__s = sin kernel of rt_math__r
        CALL    rt_math__op
        DW      fp_mul,rt_math__z,rt_math__r,rt_math__r
        LD      HL,rt_math__t_sinc
        LD      B,8
        CALL    rt_math__horner
        CALL    rt_math__op
        DW      fp_mul,rt_math__t,rt_math__r,rt_math__z
        CALL    rt_math__op
        DW      fp_mul,rt_math__t,rt_math__t,rt_math__p
        CALL    rt_math__op
        DW      fp_add,rt_math__s,rt_math__r,rt_math__t
        RET
rt_math__kcos:                          ; rt_math__c = cos kernel of rt_math__r
        CALL    rt_math__op
        DW      fp_mul,rt_math__z,rt_math__r,rt_math__r
        LD      HL,rt_math__t_cosc
        LD      B,8
        CALL    rt_math__horner
        CALL    rt_math__op
        DW      fp_mul,rt_math__t,rt_math__z,rt_math__p
        CALL    rt_math__op
        DW      fp_add,rt_math__c,rt_math__c_one,rt_math__t
        RET
; ---- SIN ----
rt_sin:
        LD      (rt_math__dst),HL
        LD      HL,rt_math__x
        CALL    rt_math__copy
        CALL    rt_math__reduce
        CALL    rt_math__ksin
        CALL    rt_math__kcos
        LD      A,(rt_math__q)
        CP      0
        JR      NZ,rt_math__sin0
        LD      HL,(rt_math__dst)
        LD      DE,rt_math__s
        JP      rt_math__copy
rt_math__sin0:
        CP      1
        JR      NZ,rt_math__sin1
        LD      HL,(rt_math__dst)
        LD      DE,rt_math__c
        JP      rt_math__copy
rt_math__sin1:
        CP      2
        JR      NZ,rt_math__sin2
        LD      HL,(rt_math__dst)
        LD      DE,rt_math__s
        JP      fp_neg
rt_math__sin2:
        CP      3
        JR      NZ,rt_math__sin3
        LD      HL,(rt_math__dst)
        LD      DE,rt_math__c
        JP      fp_neg
rt_math__sin3:
        RET
; ---- COS ----
rt_cos:
        LD      (rt_math__dst),HL
        LD      HL,rt_math__x
        CALL    rt_math__copy
        CALL    rt_math__reduce
        CALL    rt_math__ksin
        CALL    rt_math__kcos
        LD      A,(rt_math__q)
        CP      0
        JR      NZ,rt_math__cos0
        LD      HL,(rt_math__dst)
        LD      DE,rt_math__c
        JP      rt_math__copy
rt_math__cos0:
        CP      1
        JR      NZ,rt_math__cos1
        LD      HL,(rt_math__dst)
        LD      DE,rt_math__s
        JP      fp_neg
rt_math__cos1:
        CP      2
        JR      NZ,rt_math__cos2
        LD      HL,(rt_math__dst)
        LD      DE,rt_math__c
        JP      fp_neg
rt_math__cos2:
        CP      3
        JR      NZ,rt_math__cos3
        LD      HL,(rt_math__dst)
        LD      DE,rt_math__s
        JP      rt_math__copy
rt_math__cos3:
        RET
; ---- TAN ----
rt_tan:
        LD      (rt_math__dst),HL
        LD      HL,rt_math__x
        CALL    rt_math__copy
        CALL    rt_math__reduce
        CALL    rt_math__ksin
        CALL    rt_math__kcos
        LD      A,(rt_math__q)
        AND     1
        JR      NZ,rt_math__t1
        LD      HL,(rt_math__dst)
        LD      DE,rt_math__s
        LD      BC,rt_math__c
        JP      fp_div
rt_math__t1:
        CALL    rt_math__op
        DW      fp_div,rt_math__t,rt_math__c,rt_math__s
        LD      HL,(rt_math__dst)
        LD      DE,rt_math__t
        JP      fp_neg
; ---- ATN ----
rt_atn:
        LD      (rt_math__dst),HL
        LD      HL,rt_math__x
        CALL    rt_math__copy
        CALL    rt_math__atn
        LD      HL,(rt_math__dst)
        LD      DE,rt_math__a
        JP      rt_math__copy
rt_math__atn:                           ; rt_math__a = ATN(rt_math__x)
        LD      A,(rt_math__x+7)
        AND     &80
        LD      (rt_math__neg),A
        XOR     A
        LD      (rt_math__inv),A
        LD      (rt_math__six),A
        CALL    rt_math__op
        DW      fp_abs,rt_math__tt,rt_math__x,0
        CALL    rt_math__op
        DW      fp_cmp,rt_math__cr,rt_math__tt,rt_math__c_one
        LD      A,(rt_math__cr)
        DEC     A
        JR      NZ,rt_math__a1
        CALL    rt_math__op
        DW      fp_div,rt_math__tt,rt_math__c_one,rt_math__tt
        LD      A,1
        LD      (rt_math__inv),A
rt_math__a1:
        CALL    rt_math__op
        DW      fp_cmp,rt_math__cr,rt_math__tt,rt_math__c_tanpi12
        LD      A,(rt_math__cr)
        DEC     A
        JR      NZ,rt_math__a2
        CALL    rt_math__op
        DW      fp_mul,rt_math__t,rt_math__tt,rt_math__c_sqrt3
        CALL    rt_math__op
        DW      fp_sub,rt_math__t,rt_math__t,rt_math__c_one
        CALL    rt_math__op
        DW      fp_add,rt_math__u,rt_math__tt,rt_math__c_sqrt3
        CALL    rt_math__op
        DW      fp_div,rt_math__tt,rt_math__t,rt_math__u
        LD      A,1
        LD      (rt_math__six),A
rt_math__a2:
        CALL    rt_math__op
        DW      fp_mul,rt_math__z,rt_math__tt,rt_math__tt
        LD      HL,rt_math__t_atnc
        LD      B,14
        CALL    rt_math__horner
        CALL    rt_math__op
        DW      fp_mul,rt_math__t,rt_math__tt,rt_math__z
        CALL    rt_math__op
        DW      fp_mul,rt_math__t,rt_math__t,rt_math__p
        CALL    rt_math__op
        DW      fp_add,rt_math__a,rt_math__tt,rt_math__t
        LD      A,(rt_math__six)
        OR      A
        JR      Z,rt_math__a3
        CALL    rt_math__op
        DW      fp_add,rt_math__a,rt_math__c_pio6,rt_math__a
rt_math__a3:
        LD      A,(rt_math__inv)
        OR      A
        JR      Z,rt_math__a4
        CALL    rt_math__op
        DW      fp_sub,rt_math__a,rt_math__c_pio2,rt_math__a
rt_math__a4:
        LD      A,(rt_math__neg)
        OR      A
        RET     Z
        CALL    rt_math__op
        DW      fp_neg,rt_math__a,rt_math__a,0
        RET
; ---- ASIN, ACOS ----
rt_asin:
        LD      (rt_math__dst),HL
        LD      HL,rt_math__x
        CALL    rt_math__copy
        CALL    rt_math__asin
        RET     C
        LD      HL,(rt_math__dst)
        LD      DE,rt_math__a
        JP      rt_math__copy
rt_math__asin:                          ; rt_math__a = ASIN(rt_math__x); CF after error 67
        CALL    rt_math__op
        DW      fp_abs,rt_math__t,rt_math__x,0
        CALL    rt_math__op
        DW      fp_cmp,rt_math__cr,rt_math__t,rt_math__c_one
        LD      A,(rt_math__cr)
        CP      1
        JR      NZ,rt_math__s1
        LD      HL,(rt_math__dst)
        CALL    rt_math__zero
        LD      A,67
        CALL    rt_error
        SCF
        RET
rt_math__s1:
        OR      A
        JR      NZ,rt_math__s2       ; |x| < 1
        LD      HL,rt_math__a
        LD      DE,rt_math__c_pio2
        CALL    rt_math__copy
        LD      A,(rt_math__x+7)
        AND     &80
        RET     Z
        CALL    rt_math__op
        DW      fp_neg,rt_math__a,rt_math__a,0
        OR      A
        RET
rt_math__s2:
        CALL    rt_math__op
        DW      fp_sub,rt_math__t,rt_math__c_one,rt_math__x
        CALL    rt_math__op
        DW      fp_add,rt_math__u,rt_math__c_one,rt_math__x
        CALL    rt_math__op
        DW      fp_mul,rt_math__t,rt_math__t,rt_math__u
        CALL    rt_math__op
        DW      fp_sqrt,rt_math__t,rt_math__t,0
        CALL    rt_math__op
        DW      fp_div,rt_math__x,rt_math__x,rt_math__t
        CALL    rt_math__atn
        OR      A
        RET
rt_acos:
        LD      (rt_math__dst),HL
        LD      HL,rt_math__x0
        CALL    rt_math__copy
        CALL    rt_math__op
        DW      fp_abs,rt_math__aa,rt_math__x0,0
        CALL    rt_math__op
        DW      fp_cmp,rt_math__cr,rt_math__aa,rt_math__c_one
        LD      A,(rt_math__cr)
        CP      1
        JR      NZ,rt_math__c1
        LD      HL,(rt_math__dst)
        CALL    rt_math__zero
        LD      A,67
        JP      rt_error
rt_math__c1:
        CALL    rt_math__op
        DW      fp_cmp,rt_math__cr,rt_math__aa,rt_math__c_half
        LD      A,(rt_math__cr)
        CP      1
        JR      Z,rt_math__c2
        LD      HL,rt_math__x
        LD      DE,rt_math__x0
        CALL    rt_math__copy
        CALL    rt_math__asin
        RET     C
        LD      HL,(rt_math__dst)
        LD      DE,rt_math__c_pio2
        LD      BC,rt_math__a
        JP      fp_sub
rt_math__c2:
        CALL    rt_math__op
        DW      fp_sub,rt_math__x,rt_math__c_one,rt_math__aa
        CALL    rt_math__op
        DW      fp_mul,rt_math__x,rt_math__x,rt_math__c_half
        CALL    rt_math__op
        DW      fp_sqrt,rt_math__x,rt_math__x,0
        CALL    rt_math__asin
        RET     C
        CALL    rt_math__op
        DW      fp_mul,rt_math__h,rt_math__c_two,rt_math__a
        LD      A,(rt_math__x0+7)
        AND     &80
        JR      NZ,rt_math__c3
        LD      HL,(rt_math__dst)
        LD      DE,rt_math__h
        JP      rt_math__copy
rt_math__c3:
        LD      HL,(rt_math__dst)
        LD      DE,rt_math__c_pi
        LD      BC,rt_math__h
        JP      fp_sub
; ---- x^y (not an integer exponent), DEG, RAD ----
rt_powf:
        LD      (rt_math__dst),HL
        PUSH    BC
        LD      HL,rt_math__x0
        CALL    rt_math__copy
        POP     DE
        LD      HL,rt_math__y
        CALL    rt_math__copy
        LD      HL,rt_math__x0
        CALL    rt_math__iszero
        JR      NZ,rt_math__w1
        LD      HL,(rt_math__dst)
        CALL    rt_math__zero
        LD      A,(rt_math__y+7)
        AND     &80
        RET     Z
        XOR     A
        JP      rt_error
rt_math__w1:
        LD      HL,(rt_math__dst)
        LD      (rt_math__dst2),HL
        LD      HL,rt_math__w
        LD      DE,rt_math__x0
        CALL    rt_log
        CALL    rt_math__op
        DW      fp_mul,rt_math__w,rt_math__y,rt_math__w
        LD      HL,(rt_math__dst2)
        LD      DE,rt_math__w
        JP      rt_exp
rt_deg:
        PUSH    HL
        LD      HL,rt_math__t
        LD      BC,rt_math__c_d180
        CALL    fp_mul
        POP     HL
        LD      DE,rt_math__t
        LD      BC,rt_math__c_pi
        JP      fp_div
rt_rad:
        PUSH    HL
        LD      HL,rt_math__t
        LD      BC,rt_math__c_pi
        CALL    fp_mul
        POP     HL
        LD      DE,rt_math__t
        LD      BC,rt_math__c_d180
        JP      fp_div

; HL = destination, DE = x, BC = k (16 bit): x * 2^k (overflow: error 1; below 2^-1022: 0)
rt_math__scale:
        LD      (rt_math__sd),HL
        LD      (rt_math__sk),BC
        EX      DE,HL
        LD      DE,fp_xa
        CALL    fp_w_unpack
        LD      A,(fp_xa+11)
        OR      A
        JR      Z,rt_math__sc1
        LD      HL,(rt_math__sd)
        JP      rt_math__zero
rt_math__sc1:
        LD      HL,fp_xa
        LD      DE,fp_rm
        LD      BC,8
        LDIR
        LD      HL,(fp_xa+8)
        LD      DE,(rt_math__sk)
        ADD     HL,DE
        LD      (fp_re),HL
        LD      A,(fp_xa+10)
        LD      (fp_rn),A
        LD      HL,(rt_math__sd)
        JP      fp_finish

; A = &F0 or &E0: rt_math__m = the fraction of fp_xa with the biased exponent 1023 or 1022
rt_math__mant:
        PUSH    AF
        LD      HL,fp_xa
        LD      DE,rt_math__m
        LD      BC,6
        LDIR
        LD      A,(fp_xa+6)
        AND     &0F
        LD      C,A
        POP     AF
                                        ; &F0: 1023 (&3FF), &E0: 1022 (&3FE)
        OR      C
        LD      (rt_math__m+6),A
        LD      A,&3F
        LD      (rt_math__m+7),A
        RET

rt_math__retzero:
        LD      HL,(rt_math__dst)
rt_math__zero:
        LD      B,8
rt_math__z1:
        LD      (HL),0
        INC     HL
        DJNZ    rt_math__z1
        RET

; HL = 8 bytes: Z if the value is 0 (exponent 0)
rt_math__iszero:
        LD      DE,6
        ADD     HL,DE
        LD      A,(HL)
        AND     &F0
        LD      C,A
        INC     HL
        LD      A,(HL)
        AND     &7F
        OR      C
        RET

; HL = destination, DE = x: copy (as a "function" for rt_math__op)
rt_math__setz:
        JP      rt_math__copy

;! section data
rt_math__c_half:
        DB      &00,&00,&00,&00,&00,&00,&E0,&3F
rt_math__c_two:
        DB      &00,&00,&00,&00,&00,&00,&00,&40
rt_math__c_four:
        DB      &00,&00,&00,&00,&00,&00,&10,&40
rt_math__c_quarter:
        DB      &00,&00,&00,&00,&00,&00,&D0,&3F
rt_math__c_one:
        DB      &00,&00,&00,&00,&00,&00,&F0,&3F
rt_math__c_d180:
        DB      &00,&00,&00,&00,&00,&80,&66,&40
rt_math__c_pi:
        DB      &18,&2D,&44,&54,&FB,&21,&09,&40
rt_math__c_pio2:
        DB      &18,&2D,&44,&54,&FB,&21,&F9,&3F
rt_math__c_pio4:
        DB      &18,&2D,&44,&54,&FB,&21,&E9,&3F
rt_math__c_pio6:
        DB      &66,&73,&2D,&38,&52,&C1,&E0,&3F
rt_math__c_twoopi:
        DB      &83,&C8,&C9,&6D,&30,&5F,&E4,&3F
rt_math__c_pio2a:
        DB      &00,&00,&00,&50,&FB,&21,&F9,&3F
rt_math__c_pio2b:
        DB      &00,&00,&00,&60,&B4,&10,&51,&3E
rt_math__c_pio2c:
        DB      &00,&00,&00,&30,&26,&A6,&91,&3C
rt_math__c_pio2d:
        DB      &4A,&34,&07,&37,&E0,&A2,&E8,&3A
rt_math__c_ln2hi:
        DB      &00,&00,&E0,&FE,&42,&2E,&E6,&3F
rt_math__c_ln2lo:
        DB      &76,&3C,&79,&35,&EF,&39,&EA,&3D
rt_math__c_invln2:
        DB      &FE,&82,&2B,&65,&47,&15,&F7,&3F
rt_math__c_invln10:
        DB      &0E,&E5,&26,&15,&7B,&CB,&DB,&3F
rt_math__c_sqrt2:
        DB      &CD,&3B,&7F,&66,&9E,&A0,&F6,&3F
rt_math__c_sqrt3:
        DB      &AA,&4C,&58,&E8,&7A,&B6,&FB,&3F
rt_math__c_tanpi12:
        DB      &56,&CD,&9E,&5E,&14,&26,&D1,&3F
rt_math__c_expmax:
        DB      &EF,&39,&FA,&FE,&42,&2E,&86,&40
rt_math__c_expmin:
        DB      &00,&00,&00,&00,&00,&C0,&87,&C0
rt_math__t_expc:
        DB      &09,&6D,&A8,&13,&46,&12,&E6,&3D
        DB      &98,&D8,&F8,&EF,&D8,&EE,&21,&3E
        DB      &E4,&44,&F5,&67,&45,&E6,&5A,&3E
        DB      &5C,&9F,&78,&B7,&4F,&7E,&92,&3E
        DB      &34,&C7,&56,&A5,&E3,&1D,&C7,&3E
        DB      &1A,&A0,&01,&1A,&A0,&01,&FA,&3E
        DB      &1A,&A0,&01,&1A,&A0,&01,&2A,&3F
        DB      &17,&6C,&C1,&16,&6C,&C1,&56,&3F
        DB      &11,&11,&11,&11,&11,&11,&81,&3F
        DB      &55,&55,&55,&55,&55,&55,&A5,&3F
        DB      &55,&55,&55,&55,&55,&55,&C5,&3F
        DB      &00,&00,&00,&00,&00,&00,&E0,&3F
rt_math__t_sinc:
        DB      &4A,&AD,&30,&70,&C7,&52,&E9,&3C
        DB      &1F,&B8,&33,&E7,&F3,&E7,&6A,&BD
        DB      &09,&6D,&A8,&13,&46,&12,&E6,&3D
        DB      &E4,&44,&F5,&67,&45,&E6,&5A,&BE
        DB      &34,&C7,&56,&A5,&E3,&1D,&C7,&3E
        DB      &1A,&A0,&01,&1A,&A0,&01,&2A,&BF
        DB      &11,&11,&11,&11,&11,&11,&81,&3F
        DB      &55,&55,&55,&55,&55,&55,&C5,&BF
rt_math__t_cosc:
        DB      &1F,&B8,&33,&E7,&F3,&E7,&2A,&3D
        DB      &9D,&7C,&C0,&A8,&74,&39,&A9,&BD
        DB      &98,&D8,&F8,&EF,&D8,&EE,&21,&3E
        DB      &5C,&9F,&78,&B7,&4F,&7E,&92,&BE
        DB      &1A,&A0,&01,&1A,&A0,&01,&FA,&3E
        DB      &17,&6C,&C1,&16,&6C,&C1,&56,&BF
        DB      &55,&55,&55,&55,&55,&55,&A5,&3F
        DB      &00,&00,&00,&00,&00,&00,&E0,&BF
rt_math__t_logc:
        DB      &64,&21,&0B,&59,&C8,&42,&B6,&3F
        DB      &18,&86,&61,&18,&86,&61,&B8,&3F
        DB      &28,&AF,&A1,&BC,&86,&F2,&BA,&3F
        DB      &1E,&1E,&1E,&1E,&1E,&1E,&BE,&3F
        DB      &11,&11,&11,&11,&11,&11,&C1,&3F
        DB      &14,&3B,&B1,&13,&3B,&B1,&C3,&3F
        DB      &46,&17,&5D,&74,&D1,&45,&C7,&3F
        DB      &1C,&C7,&71,&1C,&C7,&71,&CC,&3F
        DB      &92,&24,&49,&92,&24,&49,&D2,&3F
        DB      &9A,&99,&99,&99,&99,&99,&D9,&3F
        DB      &55,&55,&55,&55,&55,&55,&E5,&3F
rt_math__t_atnc:
        DB      &96,&7B,&1A,&61,&B9,&A7,&A1,&3F
        DB      &68,&2F,&A1,&BD,&84,&F6,&A2,&BF
        DB      &7B,&14,&AE,&47,&E1,&7A,&A4,&3F
        DB      &64,&21,&0B,&59,&C8,&42,&A6,&BF
        DB      &18,&86,&61,&18,&86,&61,&A8,&3F
        DB      &28,&AF,&A1,&BC,&86,&F2,&AA,&BF
        DB      &1E,&1E,&1E,&1E,&1E,&1E,&AE,&3F
        DB      &11,&11,&11,&11,&11,&11,&B1,&BF
        DB      &14,&3B,&B1,&13,&3B,&B1,&B3,&3F
        DB      &46,&17,&5D,&74,&D1,&45,&B7,&BF
        DB      &1C,&C7,&71,&1C,&C7,&71,&BC,&3F
        DB      &92,&24,&49,&92,&24,&49,&C2,&BF
        DB      &9A,&99,&99,&99,&99,&99,&C9,&3F
        DB      &55,&55,&55,&55,&55,&55,&D5,&BF

;! section bss
rt_math__dst:
        DS      2
rt_math__dst2:
        DS      2
rt_math__fp:
        DS      2
rt_math__opd:
        DS      2
rt_math__opa:
        DS      2
rt_math__opb:
        DS      2
rt_math__a:
        DS      8
rt_math__sd:
        DS      2
rt_math__sk:
        DS      2
rt_math__q:
        DS      1
rt_math__neg:
        DS      1
rt_math__inv:
        DS      1
rt_math__six:
        DS      1
rt_math__cr:
        DS      4
rt_math__k:
        DS      4
rt_math__e:
        DS      4
rt_math__x:
        DS      8
rt_math__x0:
        DS      8
rt_math__y:
        DS      8
rt_math__t:
        DS      8
rt_math__u:
        DS      8
rt_math__tt:
        DS      8
rt_math__kf:
        DS      8
rt_math__r:
        DS      8
rt_math__p:
        DS      8
rt_math__z:
        DS      8
rt_math__m:
        DS      8
rt_math__f:
        DS      8
rt_math__s:
        DS      8
rt_math__l:
        DS      8
rt_math__ef:
        DS      8
rt_math__fn:
        DS      8
rt_math__c:
        DS      8
rt_math__aa:
        DS      8
rt_math__h:
        DS      8
rt_math__w:
        DS      8
rt_math__l10:
        DS      8
