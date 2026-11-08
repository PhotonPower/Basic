;! module    ix_bn
;! provides  ix_bn_zero ix_bn_copy ix_bn_mul10 ix_bn_mul100 ix_bn_shl ix_bn_shr1 ix_bn_cmp ix_bn_sub ix_bn_iszero
;! provides  ix_bn_addb ix_bn_bitlen ix_bn_shr
;! group     float
;! clobbers  AF BC DE HL
;! doc       Unsigned big integers of 176 bytes (1408 bits, little-endian) for the number <-> text conversions.

ix_bn__N EQU 176

; HL = number: 0
ix_bn_zero:
        LD      B,ix_bn__N
ix_bn__z:
        LD      (HL),0
        INC     HL
        DJNZ    ix_bn__z
        RET

; HL = source, DE = destination
ix_bn_copy:
        LD      BC,ix_bn__N
        LDIR
        RET

; HL = number: Z if 0
ix_bn_iszero:
        LD      B,ix_bn__N
        XOR     A
ix_bn__i:
        OR      (HL)
        INC     HL
        DJNZ    ix_bn__i
        OR      A
        RET

; HL = number: *= 10
ix_bn_mul10:
        LD      B,ix_bn__N
        LD      C,0                     ; carry
ix_bn__m10:
        PUSH    HL
        LD      L,(HL)
        LD      H,0
        ADD     HL,HL                   ; 2b
        LD      D,H
        LD      E,L
        ADD     HL,HL
        ADD     HL,HL                   ; 8b
        ADD     HL,DE                   ; 10b
        LD      E,C
        LD      D,0
        ADD     HL,DE                   ; + carry
        LD      C,H
        LD      A,L
        POP     HL
        LD      (HL),A
        INC     HL
        DJNZ    ix_bn__m10
        RET

; HL = number: *= 100
ix_bn_mul100:
        LD      B,ix_bn__N
        LD      C,0
ix_bn__m100:
        PUSH    HL
        PUSH    BC
        LD      L,(HL)
        LD      H,0
        ADD     HL,HL
        ADD     HL,HL                   ; 4b
        LD      D,H
        LD      E,L
        ADD     HL,HL
        ADD     HL,HL
        ADD     HL,HL                   ; 32b
        LD      B,H
        LD      C,L
        ADD     HL,HL                   ; 64b
        ADD     HL,BC
        ADD     HL,DE                   ; 100b
        POP     BC
        LD      E,C
        LD      D,0
        ADD     HL,DE
        LD      C,H
        LD      A,L
        POP     HL
        LD      (HL),A
        INC     HL
        DJNZ    ix_bn__m100
        RET

; HL = number, BC = bits (0..1279): <<= bits
ix_bn_shl:
        LD      (ix_bn__p),HL
ix_bn__sb:                              ; whole bytes
        LD      A,B
        OR      A
        JR      NZ,ix_bn__byte
        LD      A,C
        CP      8
        JR      C,ix_bn__bits
ix_bn__byte:
        PUSH    BC
        LD      HL,(ix_bn__p)           ; bytes 0..N-2 up to 1..N-1
        LD      DE,ix_bn__N-2
        ADD     HL,DE
        LD      D,H
        LD      E,L
        INC     DE
        LD      BC,ix_bn__N-1
        LDDR
        LD      HL,(ix_bn__p)
        LD      (HL),0
        POP     BC
        LD      HL,-8
        ADD     HL,BC
        LD      B,H
        LD      C,L
        JR      ix_bn__sb
ix_bn__bits:
        OR      A
        RET     Z
ix_bn__b1:
        PUSH    AF
        LD      HL,(ix_bn__p)
        OR      A
        LD      B,ix_bn__N
ix_bn__b2:
        RL      (HL)
        INC     HL
        DJNZ    ix_bn__b2
        POP     AF
        DEC     A
        JR      NZ,ix_bn__b1
        RET

; HL = number: >>= 1
ix_bn_shr1:
        LD      DE,ix_bn__N-1
        ADD     HL,DE
        OR      A
        LD      B,ix_bn__N
ix_bn__r:
        RR      (HL)
        DEC     HL
        DJNZ    ix_bn__r
        RET

; HL = a, DE = b: CF if a < b, Z if a = b
ix_bn_cmp:
        LD      BC,ix_bn__N-1
        ADD     HL,BC
        EX      DE,HL
        ADD     HL,BC
        EX      DE,HL
        LD      B,ix_bn__N
ix_bn__c:
        LD      A,(DE)
        LD      C,A
        LD      A,(HL)
        CP      C
        RET     NZ
        DEC     HL
        DEC     DE
        DJNZ    ix_bn__c
        RET

; HL = a, DE = b: a -= b (a >= b)
ix_bn_sub:
        LD      B,ix_bn__N
        OR      A
ix_bn__s:
        LD      A,(DE)
        LD      C,A
        LD      A,(HL)
        SBC     A,C
        LD      (HL),A
        INC     HL
        INC     DE
        DJNZ    ix_bn__s
        RET

; HL = number, A = 0..255: += A
ix_bn_addb:
        ADD     A,(HL)
        LD      (HL),A
        RET     NC
        LD      B,ix_bn__N-1
ix_bn__ab:
        INC     HL
        INC     (HL)
        RET     NZ
        DJNZ    ix_bn__ab
        RET

; HL = number -> HL = its bit length (0 for 0)
ix_bn_bitlen:
        LD      DE,ix_bn__N-1
        ADD     HL,DE
        LD      B,ix_bn__N
ix_bn__bl:
        LD      A,(HL)
        OR      A
        JR      NZ,ix_bn__bf
        DEC     HL
        DJNZ    ix_bn__bl
        LD      HL,0
        RET
ix_bn__bf:                              ; B = index + 1 of the top byte, A = it
        LD      C,0
ix_bn__bb:
        INC     C
        SRL     A
        JR      NZ,ix_bn__bb
        LD      L,B                     ; (B - 1) * 8 + C
        DEC     L
        LD      H,0
        ADD     HL,HL
        ADD     HL,HL
        ADD     HL,HL
        LD      B,0
        ADD     HL,BC
        RET

; HL = number, BC = bits: >>= bits; A = 1 if a set bit was shifted out (sticky), else 0
ix_bn_shr:
        LD      (ix_bn__p),HL
        XOR     A
        LD      (ix_bn__lost),A
ix_bn__rb:                              ; whole bytes
        LD      A,B
        OR      A
        JR      NZ,ix_bn__rbyte
        LD      A,C
        CP      8
        JR      C,ix_bn__rbits
ix_bn__rbyte:
        PUSH    BC
        LD      HL,(ix_bn__p)
        LD      A,(HL)
        OR      A
        JR      Z,ix_bn__r0
        LD      A,1
        LD      (ix_bn__lost),A
ix_bn__r0:
        LD      D,H
        LD      E,L
        INC     HL
        LD      BC,ix_bn__N-1
        LDIR
        XOR     A
        LD      (DE),A
        POP     BC
        LD      HL,-8
        ADD     HL,BC
        LD      B,H
        LD      C,L
        JR      ix_bn__rb
ix_bn__rbits:
        OR      A
        JR      Z,ix_bn__rend
ix_bn__r1:
        PUSH    AF
        LD      HL,(ix_bn__p)
        CALL    ix_bn_shr1
        JR      NC,ix_bn__r2
        LD      A,1
        LD      (ix_bn__lost),A
ix_bn__r2:
        POP     AF
        DEC     A
        JR      NZ,ix_bn__r1
ix_bn__rend:
        LD      A,(ix_bn__lost)
        RET

;! section bss
ix_bn__lost:
        DS      1
ix_bn__p:
        DS      2
