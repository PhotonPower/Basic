;! module    fp_w
;! provides  fp_w_unpack fp_w_shl1 fp_w_shr1 fp_w_add fp_w_sub fp_w_cmp fp_w_iszero fp_w_shr fp_w_clear fp_w_copy
;! provides  fp_xa fp_xb fp_rm fp_re fp_rn fp_t
;! group     float
;! clobbers  AF BC DE HL
;! doc       Work areas and 64-bit primitives of the floating-point core (little-endian, 8 bytes).

; Unpacked value (12 bytes, as fp_parts of runtime/ref): +0..+7 significand m (2^52 <= m < 2^53),
; +8/+9 exponent e (signed), +10 sign (1 negative), +11 1 if the value is zero (then all 0):
; value = (-1)^sign * m * 2^e.

; HL = binary64, DE = unpacked value
fp_w_unpack:
        LD      (fp_w__ptr),DE
        LD      DE,fp_w__src
        LD      BC,8
        LDIR
        LD      A,(fp_w__src+7)         ; be = bits 62..52
        AND     &7F
        LD      H,A
        LD      A,(fp_w__src+6)
        LD      L,A
        SRL     H                       ; >> 4 (unrolled)
        RR      L
        SRL     H
        RR      L
        SRL     H
        RR      L
        SRL     H
        RR      L
        LD      A,H
        OR      L
        JR      Z,fp_w__zero
        LD      BC,-1075
        ADD     HL,BC
        LD      (fp_w__e),HL            ; e = be - 1075
        LD      DE,(fp_w__ptr)
        LD      HL,fp_w__src
        LD      BC,6
        LDIR                            ; m0..m5
        LD      A,(fp_w__src+6)
        AND     &0F
        OR      &10                     ; the hidden bit 52
        LD      (DE),A
        INC     DE
        XOR     A
        LD      (DE),A                  ; m7
        INC     DE
        LD      HL,(fp_w__e)
        LD      A,L
        LD      (DE),A
        INC     DE
        LD      A,H
        LD      (DE),A
        INC     DE
        LD      A,(fp_w__src+7)
        RLCA
        AND     1
        LD      (DE),A                  ; sign
        INC     DE
        XOR     A
        LD      (DE),A                  ; not zero
        RET
fp_w__zero:
        LD      HL,(fp_w__ptr)
        LD      B,11
fp_w__z1:
        LD      (HL),0
        INC     HL
        DJNZ    fp_w__z1
        LD      (HL),1
        RET

; HL = 8 bytes: shift left by 1 (carry out in CF; unrolled)
fp_w_shl1:
        OR      A
        RL      (HL)
        INC     HL
        RL      (HL)
        INC     HL
        RL      (HL)
        INC     HL
        RL      (HL)
        INC     HL
        RL      (HL)
        INC     HL
        RL      (HL)
        INC     HL
        RL      (HL)
        INC     HL
        RL      (HL)
        RET

; HL = 8 bytes: shift right by 1 (bit 0 out in CF; unrolled)
fp_w_shr1:
        LD      BC,7
        ADD     HL,BC
        OR      A
        RR      (HL)
        DEC     HL
        RR      (HL)
        DEC     HL
        RR      (HL)
        DEC     HL
        RR      (HL)
        DEC     HL
        RR      (HL)
        DEC     HL
        RR      (HL)
        DEC     HL
        RR      (HL)
        DEC     HL
        RR      (HL)
        RET

; HL = destination, DE = operand: (HL) += (DE), carry out in CF
fp_w_add:
        OR      A
        LD      B,8
fp_w__a1:
        LD      A,(DE)
        ADC     A,(HL)
        LD      (HL),A
        INC     HL
        INC     DE
        DJNZ    fp_w__a1
        RET

; HL = destination, DE = operand: (HL) -= (DE), borrow out in CF
fp_w_sub:
        OR      A
        LD      B,8
fp_w__s1:
        LD      A,(DE)
        LD      C,A
        LD      A,(HL)
        SBC     A,C
        LD      (HL),A
        INC     HL
        INC     DE
        DJNZ    fp_w__s1
        RET

; HL = a, DE = b (unsigned 64 bit): CF set if a < b, Z set if a = b
fp_w_cmp:
        LD      BC,7
        ADD     HL,BC
        EX      DE,HL
        ADD     HL,BC
        EX      DE,HL                   ; both at byte 7
        LD      B,8
fp_w__c1:
        LD      A,(DE)
        LD      C,A
        LD      A,(HL)
        CP      C
        RET     NZ                      ; CF: a < b
        DEC     HL
        DEC     DE
        DJNZ    fp_w__c1
        RET                             ; Z, NC: equal

; HL = 8 bytes -> Z set if all 0
fp_w_iszero:
        LD      B,8
        XOR     A
fp_w__i1:
        OR      (HL)
        INC     HL
        DJNZ    fp_w__i1
        OR      A
        RET

; HL = 8 bytes: all 0
fp_w_clear:
        LD      B,8
fp_w__k1:
        LD      (HL),0
        INC     HL
        DJNZ    fp_w__k1
        RET

; HL = source, DE = destination: 8 bytes
fp_w_copy:
        LD      BC,8
        LDIR
        RET

; HL = 8 bytes, A = n (0..255): shift right by n; the bits shifted out are ORed into bit 0
; (sticky). n >= 64 leaves 0 or 1.
fp_w_shr:
        LD      (fp_w__ptr),HL
        LD      C,A
        XOR     A
        LD      (fp_w__lost),A
fp_w__by:
        LD      A,C                     ; whole bytes first
        CP      8
        JR      C,fp_w__bits
        SUB     8
        LD      C,A
        LD      HL,(fp_w__ptr)
        LD      A,(HL)                  ; the lowest byte is lost
        LD      B,A
        LD      A,(fp_w__lost)
        OR      B
        LD      (fp_w__lost),A
        LD      D,H
        LD      E,L
        INC     HL
        PUSH    BC
        LD      BC,7
        LDIR                            ; bytes 1..7 down to 0..6
        POP     BC
        XOR     A
        LD      (DE),A                  ; byte 7 = 0
        JR      fp_w__by
fp_w__bits:
        LD      A,C
        OR      A
        JR      Z,fp_w__st
fp_w__b1:
        LD      HL,(fp_w__ptr)
        PUSH    BC
        CALL    fp_w_shr1
        POP     BC
        JR      NC,fp_w__b2
        LD      A,1
        LD      (fp_w__lost),A
fp_w__b2:
        DEC     C
        JR      NZ,fp_w__b1
fp_w__st:
        LD      A,(fp_w__lost)
        OR      A
        RET     Z
        LD      HL,(fp_w__ptr)
        SET     0,(HL)
        RET

;! section bss
fp_w__src:
        DS      8
fp_w__e:
        DS      2
fp_w__ptr:
        DS      2
fp_w__lost:
        DS      1
fp_xa:
        DS      12
fp_xb:
        DS      12
fp_rm:
        DS      8
fp_re:
        DS      2
fp_rn:
        DS      1
fp_t:
        DS      16
