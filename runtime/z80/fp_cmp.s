;! module    fp_cmp
;! provides  fp_cmp
;! requires  fp_w_unpack fp_w_cmp fp_xa fp_xb
;! group     float
;! clobbers  AF BC DE HL
;! doc       Exact comparison: HL = destination (32-bit -1, 0 or 1), DE = a, BC = b.

fp_cmp:
        LD      (fp_cmp__dst),HL
        PUSH    BC
        EX      DE,HL
        LD      DE,fp_xa
        CALL    fp_w_unpack
        POP     HL
        LD      DE,fp_xb
        CALL    fp_w_unpack
        LD      A,(fp_xa+11)
        LD      C,A
        LD      A,(fp_xb+11)
        AND     C
        JR      NZ,fp_cmp__eq           ; both 0
        LD      A,(fp_xa+11)
        OR      A
        JR      Z,fp_cmp__anz
        LD      A,(fp_xb+10)            ; a = 0: b negative -> 1, else -1
        OR      A
        JR      NZ,fp_cmp__gt
        JR      fp_cmp__lt
fp_cmp__anz:
        LD      A,(fp_xb+11)
        OR      A
        JR      Z,fp_cmp__bnz
        LD      A,(fp_xa+10)            ; b = 0: a negative -> -1, else 1
        OR      A
        JR      NZ,fp_cmp__lt
        JR      fp_cmp__gt
fp_cmp__bnz:
        LD      A,(fp_xa+10)
        LD      C,A
        LD      A,(fp_xb+10)
        CP      C
        JR      Z,fp_cmp__same
        LD      A,C                     ; signs differ
        OR      A
        JR      NZ,fp_cmp__lt
        JR      fp_cmp__gt
fp_cmp__same:
        LD      HL,(fp_xa+8)            ; magnitude: exponent, then significand
        LD      DE,(fp_xb+8)
        OR      A
        SBC     HL,DE
        JR      Z,fp_cmp__m
        LD      A,1                     ; |a| > |b| if a.e > b.e
        JP      P,fp_cmp__sign
        LD      A,-1
        JR      fp_cmp__sign
fp_cmp__m:
        LD      HL,fp_xa
        LD      DE,fp_xb
        CALL    fp_w_cmp
        JR      Z,fp_cmp__eq
        LD      A,1
        JR      NC,fp_cmp__sign
        LD      A,-1
fp_cmp__sign:                           ; A = comparison of the magnitudes; negative: reversed
        LD      C,A
        LD      A,(fp_xa+10)
        OR      A
        LD      A,C
        JR      Z,fp_cmp__put
        NEG
        JR      fp_cmp__put
fp_cmp__eq:
        XOR     A
        JR      fp_cmp__put
fp_cmp__lt:
        LD      A,-1
        JR      fp_cmp__put
fp_cmp__gt:
        LD      A,1
fp_cmp__put:                            ; A = -1, 0, 1 as 32 bits
        LD      HL,(fp_cmp__dst)
        LD      (HL),A
        INC     HL
        RLCA
        SBC     A,A                     ; 0 or &FF
        LD      (HL),A
        INC     HL
        LD      (HL),A
        INC     HL
        LD      (HL),A
        RET

;! section bss
fp_cmp__dst:
        DS      2
