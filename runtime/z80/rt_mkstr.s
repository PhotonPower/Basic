;! module    rt_mkstr
;! provides  rt_mki rt_mkl rt_mks rt_mkd rt_cvi rt_cvl rt_cvs rt_cvd
;! requires  rt_str_newtemp fp_w_unpack fp_w_shr fp_w_shr1 fp_xa fp_rm fp_re fp_rn fp_finish rt_error
;! group     strings
;! clobbers  AF BC DE HL
;! doc       MKI$ MKL$ MKS$ MKD$ and CVI CVL CVS CVD (runtime/ref/rt_mk.c): big-endian, GFA 8-byte float, IEEE single.

; MKx$: DE -> value -> HL = temporary string. CVx: HL = result, DE = descriptor; a shorter string
; counts as padded with zero bytes.

; B = count: the first B bytes of rt_mkstr__b -> HL = temporary with them
rt_mkstr__temp:
        LD      C,B
        LD      B,0
        PUSH    BC
        CALL    rt_str_newtemp          ; HL = descriptor, DE = characters
        POP     BC
        PUSH    HL
        LD      HL,rt_mkstr__b
        LDIR
        POP     HL
        RET

; DE = descriptor, B = count: the first B bytes in rt_mkstr__b (missing ones 0); keeps HL
rt_mkstr__get:
        PUSH    HL
        PUSH    BC
        XOR     A
        LD      HL,rt_mkstr__b
        LD      B,8
rt_mkstr__g0:
        LD      (HL),A
        INC     HL
        DJNZ    rt_mkstr__g0
        POP     BC
        EX      DE,HL
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        INC     HL
        LD      C,(HL)
        INC     HL
        LD      A,(HL)                  ; A:C = length
        OR      A
        JR      NZ,rt_mkstr__g1
        LD      A,C
        CP      B
        JR      NC,rt_mkstr__g1
        LD      B,C
rt_mkstr__g1:
        LD      A,B
        OR      A
        JR      Z,rt_mkstr__g9
        LD      C,B
        LD      B,0
        EX      DE,HL
        LD      DE,rt_mkstr__b
        LDIR
rt_mkstr__g9:
        POP     HL
        RET

; DE -> integer: MKI$ (2 bytes) and MKL$ (4 bytes), the high byte first
rt_mki:
        EX      DE,HL
        LD      A,(HL)
        LD      (rt_mkstr__b+1),A
        INC     HL
        LD      A,(HL)
        LD      (rt_mkstr__b),A
        LD      B,2
        JR      rt_mkstr__temp
rt_mkl:
        LD      HL,rt_mkstr__b+3
        LD      B,4
rt_mkstr__l1:
        LD      A,(DE)
        LD      (HL),A
        INC     DE
        DEC     HL
        DJNZ    rt_mkstr__l1
        LD      B,4
        JR      rt_mkstr__temp

; HL = result, DE = descriptor: CVI (sign-extended) and CVL
rt_cvi:
        LD      B,2
        CALL    rt_mkstr__get
        LD      A,(rt_mkstr__b+1)
        LD      (HL),A
        INC     HL
        LD      A,(rt_mkstr__b)
        LD      (HL),A
        INC     HL
        RLCA
        SBC     A,A
        LD      (HL),A
        INC     HL
        LD      (HL),A
        RET
rt_cvl:
        LD      B,4
        CALL    rt_mkstr__get
        LD      DE,rt_mkstr__b+3
        LD      B,4
rt_mkstr__c1:
        LD      A,(DE)
        LD      (HL),A
        INC     HL
        DEC     DE
        DJNZ    rt_mkstr__c1
        RET

; DE -> x: unpack into fp_xa; Z set if x = 0 (then rt_mkstr__b is 0); HL = biased exponent
rt_mkstr__unp:
        EX      DE,HL
        LD      DE,fp_xa
        CALL    fp_w_unpack
        LD      HL,0
        LD      (rt_mkstr__b),HL
        LD      (rt_mkstr__b+2),HL
        LD      (rt_mkstr__b+4),HL
        LD      (rt_mkstr__b+6),HL
        LD      A,(fp_xa+11)
        OR      A
        JR      Z,rt_mkstr__u1
        XOR     A                       ; Z
        RET
rt_mkstr__u1:
        LD      HL,(fp_xa+8)
        LD      BC,1075
        ADD     HL,BC
        OR      1                       ; NZ
        RET

; HL = biased exponent, A = k (>= 2): the significand of fp_xa >> k, rounded to nearest, ties to
; even; C = the bit that a carry reaches (0..7), E = its byte: then >> 1 more and HL + 1
rt_mkstr__rnd:
        LD      (rt_mkstr__ex),HL
        PUSH    DE
        PUSH    BC
        SUB     2
        LD      HL,fp_xa                ; >> k-2 with sticky: bit 1 = round bit, bit 0 = sticky
        CALL    fp_w_shr
        LD      A,(fp_xa)
        AND     3
        LD      (rt_mkstr__rs),A
        LD      HL,fp_xa
        CALL    fp_w_shr1
        LD      HL,fp_xa
        CALL    fp_w_shr1
        LD      A,(rt_mkstr__rs)
        CP      2
        JR      C,rt_mkstr__r9          ; round bit 0
        JR      NZ,rt_mkstr__up         ; above the half
        LD      A,(fp_xa)
        RRCA
        JR      NC,rt_mkstr__r9         ; tie, already even
rt_mkstr__up:
        LD      HL,fp_xa
rt_mkstr__inc:
        INC     (HL)
        INC     HL
        JR      Z,rt_mkstr__inc
rt_mkstr__r9:
        POP     BC
        POP     DE
        LD      D,0
        LD      HL,fp_xa
        ADD     HL,DE
        LD      A,(HL)
        LD      B,C
        INC     B
rt_mkstr__r1:
        RRCA
        DJNZ    rt_mkstr__r1            ; CF = the carry bit
        LD      HL,(rt_mkstr__ex)
        RET     NC
        INC     HL
        LD      (rt_mkstr__ex),HL
        LD      HL,fp_xa
        CALL    fp_w_shr1
        LD      HL,(rt_mkstr__ex)
        RET

; DE -> x: MKD$(x): 48-bit mantissa, exponent word = biased exponent (negated if negative)
rt_mkd:
        CALL    rt_mkstr__unp
        JR      Z,rt_mkstr__d9
        LD      A,5
        LD      E,6
        LD      C,0
        CALL    rt_mkstr__rnd
        LD      A,(fp_xa+10)
        OR      A
        JR      Z,rt_mkstr__d1
        EX      DE,HL                   ; negate
        XOR     A
        LD      H,A
        LD      L,A
        SBC     HL,DE
rt_mkstr__d1:
        LD      A,H
        LD      (rt_mkstr__b+6),A
        LD      A,L
        LD      (rt_mkstr__b+7),A
        LD      HL,fp_xa+5
        LD      DE,rt_mkstr__b
        LD      B,6
rt_mkstr__d2:
        LD      A,(HL)
        LD      (DE),A
        DEC     HL
        INC     DE
        DJNZ    rt_mkstr__d2
rt_mkstr__d9:
        LD      B,8
        JP      rt_mkstr__temp

; DE -> x: MKS$(x): IEEE single (ties to even; overflow error 1; below the smallest normal 0)
rt_mks:
        CALL    rt_mkstr__unp
        JR      Z,rt_mkstr__s9
        LD      A,29
        LD      E,3
        LD      C,0
        CALL    rt_mkstr__rnd
        LD      BC,-896                 ; single exponent
        ADD     HL,BC
        BIT     7,H
        JR      NZ,rt_mkstr__s9         ; negative: 0
        LD      A,H
        OR      A
        JR      NZ,rt_mkstr__ov
        LD      A,L
        OR      A
        JR      Z,rt_mkstr__s9
        CP      255
        JR      NC,rt_mkstr__ov
        LD      A,(fp_xa)
        LD      (rt_mkstr__b+3),A
        LD      A,(fp_xa+1)
        LD      (rt_mkstr__b+2),A
        LD      A,(fp_xa+2)
        AND     &7F
        BIT     0,L
        JR      Z,rt_mkstr__s1
        OR      &80
rt_mkstr__s1:
        LD      (rt_mkstr__b+1),A
        LD      A,(fp_xa+10)
        RRCA                            ; sign to bit 7
        LD      B,A
        LD      A,L
        SRL     A
        OR      B
        LD      (rt_mkstr__b),A
rt_mkstr__s9:
        LD      B,4
        JP      rt_mkstr__temp
rt_mkstr__ov:
        LD      A,1
        CALL    rt_error
        LD      HL,0
        LD      (rt_mkstr__b),HL
        LD      (rt_mkstr__b+2),HL
        JR      rt_mkstr__s9

; HL = result, DE = descriptor: CVD (outside binary64: error 1 or 0)
rt_cvd:
        LD      B,8
        CALL    rt_mkstr__get
        PUSH    HL
        LD      HL,rt_mkstr__b+5
        LD      DE,fp_rm
        LD      B,6
        XOR     A
rt_mkstr__v1:
        LD      C,(HL)
        OR      C
        EX      DE,HL
        LD      (HL),C
        EX      DE,HL
        DEC     HL
        INC     DE
        DJNZ    rt_mkstr__v1
        LD      C,A
        XOR     A
        LD      (fp_rm+6),A
        LD      (fp_rm+7),A
        OR      C
        POP     HL
        JR      Z,rt_mkstr__zero
        PUSH    HL
        LD      A,(rt_mkstr__b+6)
        LD      H,A
        LD      A,(rt_mkstr__b+7)
        LD      L,A
        XOR     A
        BIT     7,H
        JR      Z,rt_mkstr__v2
        EX      DE,HL
        LD      H,A
        LD      L,A
        SBC     HL,DE
        LD      A,1
rt_mkstr__v2:
        LD      (fp_rn),A
        LD      BC,-&3FE-48
        ADD     HL,BC
        LD      (fp_re),HL
        POP     HL
        JP      fp_finish
rt_mkstr__zero:
        LD      B,8
rt_mkstr__z1:
        LD      (HL),0
        INC     HL
        DJNZ    rt_mkstr__z1
        RET

; HL = result, DE = descriptor: CVS (infinity, NaN: error 1; denormal: 0)
rt_cvs:
        LD      B,4
        CALL    rt_mkstr__get
        LD      A,(rt_mkstr__b+1)       ; exponent = bits 30..23
        RLCA
        LD      A,(rt_mkstr__b)
        RLA
        OR      A
        JR      Z,rt_mkstr__zero
        CP      255
        JR      NZ,rt_mkstr__s2
        PUSH    HL
        LD      A,1
        CALL    rt_error
        POP     HL
        JR      rt_mkstr__zero
rt_mkstr__s2:
        PUSH    HL
        LD      L,A
        LD      H,0
        LD      BC,-150
        ADD     HL,BC
        LD      (fp_re),HL
        LD      HL,0
        LD      (fp_rm+4),HL
        LD      (fp_rm+6),HL
        LD      (fp_rm+3),HL            ; byte 3 = 0
        LD      A,(rt_mkstr__b+3)
        LD      (fp_rm),A
        LD      A,(rt_mkstr__b+2)
        LD      (fp_rm+1),A
        LD      A,(rt_mkstr__b+1)
        OR      &80                     ; the hidden bit 23
        LD      (fp_rm+2),A
        LD      A,(rt_mkstr__b)
        RLCA
        AND     1
        LD      (fp_rn),A
        POP     HL
        JP      fp_finish

;! section bss
rt_mkstr__b:
        DS      8
rt_mkstr__ex:
        DS      2
rt_mkstr__rs:
        DS      1
