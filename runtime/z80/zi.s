;! module    zi
;! provides  zi_add zi_sub zi_and zi_or zi_xor zi_not zi_neg zi_mov zi_cmp zi_cmp_code zi_addv zi_subv zi_mulv
;! group     core
;! clobbers  AF BC DE HL
;! doc       32-bit integers of the generated code (D30): HL = result, DE = a, BC = b.

; zi_cmp: zi_cmp_code 0 eq, 1 ne, 2 lt, 3 le, 4 gt, 5 ge (signed) -> -1 or 0.
; zi_addv, zi_subv, zi_mulv: CF set if the result does not fit 32 bits (then it is undefined).

zi_add:
        CALL    zi__add
        RET
zi_addv:
        CALL    zi__add
        JP      PE,zi__ovf              ; V of the highest byte
        OR      A
        RET
zi__add:
        OR      A
        PUSH    AF
        LD      A,4
zi__a1:
        LD      (zi__n),A
        POP     AF
        LD      A,(BC)
        EX      DE,HL
        ADC     A,(HL)
        EX      DE,HL
        LD      (HL),A
        PUSH    AF
        INC     HL
        INC     DE
        INC     BC
        LD      A,(zi__n)
        DEC     A
        JR      NZ,zi__a1
        POP     AF
        RET

zi_sub:
        CALL    zi__sub
        RET
zi_subv:
        CALL    zi__sub
        JP      PE,zi__ovf
        OR      A
        RET
zi__sub:                                ; (HL) = (DE) - (BC)
        OR      A
        PUSH    AF
        LD      A,4
zi__s1:
        LD      (zi__n),A
        LD      A,(BC)
        LD      (zi__t),A
        POP     AF
        LD      A,(DE)
        PUSH    HL
        LD      HL,zi__t
        SBC     A,(HL)
        POP     HL
        LD      (HL),A
        PUSH    AF
        INC     HL
        INC     DE
        INC     BC
        LD      A,(zi__n)
        DEC     A
        JR      NZ,zi__s1
        POP     AF
        RET
zi__ovf:
        SCF
        RET

zi_and:
        LD      A,&A2                   ; opcode AND D
        JR      zi__logic
zi_or:
        LD      A,&B2                   ; OR D
        JR      zi__logic
zi_xor:
        LD      A,&AA                   ; XOR D
zi__logic:
        LD      (zi__op),A
        LD      A,4
zi__l1:
        PUSH    AF
        PUSH    DE
        LD      A,(BC)
        LD      (zi__t),A
        LD      A,(DE)
        LD      D,A
        LD      A,(zi__t)
        CALL    zi__opc
        POP     DE
        LD      (HL),A
        INC     HL
        INC     DE
        INC     BC
        POP     AF
        DEC     A
        JR      NZ,zi__l1
        RET
zi__opc:                                ; the logic instruction on A and D
        PUSH    HL
        LD      HL,(zi__op)
        LD      H,&C9                   ; RET
        LD      (zi__code),HL
        POP     HL
        JP      zi__code

zi_not:
        LD      B,4
zi__n1:
        LD      A,(DE)
        CPL
        LD      (HL),A
        INC     HL
        INC     DE
        DJNZ    zi__n1
        RET

zi_neg:
        LD      B,4
        OR      A
zi__g1:
        LD      A,(DE)
        LD      C,A
        LD      A,0
        SBC     A,C
        LD      (HL),A
        INC     HL
        INC     DE
        DJNZ    zi__g1
        RET

zi_mov:
        EX      DE,HL
        LD      BC,4
        LDIR
        RET

; signed comparison by the flags of a - b
zi_cmp:
        PUSH    HL
        LD      HL,zi__t+1              ; a - b into a scratch area: Z over all bytes, S/V of the last
        CALL    zi__sub
        LD      C,0                     ; C = 1 if a < b (S xor V)
        JP      PE,zi__v
        JP      P,zi__ns
        INC     C
        JR      zi__ns
zi__v:  JP      M,zi__ns
        INC     C
zi__ns:
        LD      HL,zi__t+1              ; B = 1 if a = b
        LD      A,(HL)
        INC     HL
        OR      (HL)
        INC     HL
        OR      (HL)
        INC     HL
        OR      (HL)
        LD      B,0
        JR      NZ,zi__ne
        INC     B
zi__ne:
        LD      A,(zi_cmp_code)
        OR      A
        JR      Z,zi__r_eq
        DEC     A
        JR      Z,zi__r_ne
        DEC     A
        JR      Z,zi__r_lt
        DEC     A
        JR      Z,zi__r_le
        DEC     A
        JR      Z,zi__r_gt
        LD      A,C                     ; ge: not lt
        XOR     1
        JR      zi__put
zi__r_eq:
        LD      A,B
        JR      zi__put
zi__r_ne:
        LD      A,B
        XOR     1
        JR      zi__put
zi__r_lt:
        LD      A,C
        JR      zi__put
zi__r_le:
        LD      A,C
        OR      B
        JR      zi__put
zi__r_gt:
        LD      A,C
        OR      B
        XOR     1
zi__put:                                ; A = 1 true -> -1, 0 -> 0
        NEG
        POP     HL
        LD      (HL),A
        INC     HL
        LD      (HL),A
        INC     HL
        LD      (HL),A
        INC     HL
        LD      (HL),A
        RET

; signed multiplication with overflow: |a| * |b| as 64 bits, then the range
zi_mulv:
        LD      (zi__dst),HL
        LD      H,D
        LD      L,E
        LD      DE,zi__a
        PUSH    BC
        CALL    zi__abs                 ; zi__a = |a|, sign in zi__sg
        LD      A,(zi__sg)
        LD      (zi__s2),A
        POP     HL
        LD      DE,zi__b
        CALL    zi__abs
        LD      A,(zi__sg)
        LD      C,A
        LD      A,(zi__s2)
        XOR     C
        LD      (zi__s2),A              ; sign of the product
        LD      HL,zi__p                ; p = 0 (8 bytes)
        LD      B,8
zi__m0:
        LD      (HL),0
        INC     HL
        DJNZ    zi__m0
        LD      A,32
zi__m1:
        PUSH    AF
        LD      HL,zi__p                ; p <<= 1
        OR      A
        LD      B,8
zi__m2:
        RL      (HL)
        INC     HL
        DJNZ    zi__m2
        LD      HL,zi__b                ; the next bit of |b| (from the top)
        OR      A
        LD      B,4
zi__m3:
        RL      (HL)
        INC     HL
        DJNZ    zi__m3
        JR      NC,zi__m5
        LD      HL,zi__p                ; p += |a|
        LD      DE,zi__a
        OR      A
        LD      B,4
zi__m4:
        LD      A,(DE)
        ADC     A,(HL)
        LD      (HL),A
        INC     HL
        INC     DE
        DJNZ    zi__m4
        LD      B,4
zi__m6:
        LD      A,(HL)
        ADC     A,0
        LD      (HL),A
        INC     HL
        DJNZ    zi__m6
zi__m5:
        POP     AF
        DEC     A
        JR      NZ,zi__m1
        LD      HL,(zi__dst)            ; the low 32 bits with the sign (also on overflow: rt_imul)
        LD      DE,zi__p
        EX      DE,HL
        LD      BC,4
        LDIR
        LD      A,(zi__s2)
        OR      A
        JR      Z,zi__mchk
        LD      HL,(zi__dst)
        LD      D,H
        LD      E,L
        CALL    zi_neg
zi__mchk:
        LD      HL,zi__p+4              ; high 32 bits must be 0
        LD      A,(HL)
        INC     HL
        OR      (HL)
        INC     HL
        OR      (HL)
        INC     HL
        OR      (HL)
        JP      NZ,zi__ovf
        LD      A,(zi__p+3)             ; below 2^31, or exactly 2^31 when negative
        BIT     7,A
        JR      Z,zi__mok
        CP      &80
        JP      NZ,zi__ovf
        LD      HL,zi__p
        LD      A,(HL)
        INC     HL
        OR      (HL)
        INC     HL
        OR      (HL)
        JP      NZ,zi__ovf
        LD      A,(zi__s2)
        OR      A
        JP      Z,zi__ovf
zi__mok:
        OR      A                       ; NC: no overflow
        RET
zi__abs:                                ; HL -> a: (DE) = |a|, zi__sg = 1 if negative
        PUSH    DE
        LD      BC,4
        LDIR
        POP     HL
        INC     HL
        INC     HL
        INC     HL
        LD      A,(HL)
        RLCA
        AND     1
        LD      (zi__sg),A
        RET     Z
        DEC     HL
        DEC     HL
        DEC     HL
        LD      D,H
        LD      E,L
        JP      zi_neg

;! section bss
zi_cmp_code:
        DS      1
zi__n:
        DS      1
zi__t:
        DS      5
zi__op:
        DS      2
zi__code:
        DS      2
zi__dst:
        DS      2
zi__sg:
        DS      1
zi__s2:
        DS      1
zi__a:
        DS      4
zi__b:
        DS      4
zi__p:
        DS      8
