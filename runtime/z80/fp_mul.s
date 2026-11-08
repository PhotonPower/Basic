;! module    fp_mul
;! provides  fp_mul
;! requires  fp_finish fp_zero8 fp_w_unpack fp_w_shl1 fp_xa fp_xb fp_rm fp_re fp_rn fp_t
;! group     float
;! clobbers  AF BC DE HL
;! doc       Multiplication, correctly rounded (ties to even): HL = destination, DE = a, BC = b.

; P = a.m * b.m (106 bits, shift and add over the 53 bits of b.m); m = P >> 42 with the lost bits
; as sticky bit, e = a.e + b.e + 42 (as fp_mul of runtime/ref).
fp_mul:
        LD      (fp_mul__dst),HL
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
        OR      C
        JR      Z,fp_mul__go
        LD      HL,(fp_mul__dst)
        JP      fp_zero8
fp_mul__go:
        LD      HL,fp_mul__p            ; P = 0
        LD      B,16
fp_mul__c:
        LD      (HL),0
        INC     HL
        DJNZ    fp_mul__c
        LD      HL,fp_xb                ; multiplier b.m << 11: bit 52 at bit 63
        LD      DE,fp_t+1
        LD      BC,7
        LDIR
        XOR     A
        LD      (fp_t),A
        LD      B,3
fp_mul__m3:
        PUSH    BC
        LD      HL,fp_t
        CALL    fp_w_shl1
        POP     BC
        DJNZ    fp_mul__m3
        LD      A,53
fp_mul__loop:
        PUSH    AF
        LD      HL,fp_mul__p            ; P <<= 1 (16 bytes)
        OR      A
        LD      B,16
fp_mul__sl:
        RL      (HL)
        INC     HL
        DJNZ    fp_mul__sl
        LD      HL,fp_t                 ; next bit of the multiplier
        CALL    fp_w_shl1
        JR      NC,fp_mul__next
        LD      HL,fp_mul__p            ; P += a.m
        LD      DE,fp_xa
        OR      A
        LD      B,8
fp_mul__ad:
        LD      A,(DE)
        ADC     A,(HL)
        LD      (HL),A
        INC     HL
        INC     DE
        DJNZ    fp_mul__ad
        LD      B,8
fp_mul__ca:
        LD      A,(HL)
        ADC     A,0
        LD      (HL),A
        INC     HL
        DJNZ    fp_mul__ca
fp_mul__next:
        POP     AF
        DEC     A
        JR      NZ,fp_mul__loop
        XOR     A                       ; P >>= 2, the lost bits in C
        LD      C,A
        LD      D,2
fp_mul__r2:
        LD      HL,fp_mul__p+15
        OR      A
        LD      B,16
fp_mul__sr:
        RR      (HL)
        DEC     HL
        DJNZ    fp_mul__sr
        JR      NC,fp_mul__r3
        LD      C,1
fp_mul__r3:
        DEC     D
        JR      NZ,fp_mul__r2
        LD      HL,fp_mul__p            ; and the lowest 40 bits of P >> 2
        LD      B,5
        LD      A,C
fp_mul__st:
        OR      (HL)
        INC     HL
        DJNZ    fp_mul__st
        LD      C,A
        LD      HL,fp_mul__p+5          ; m = bytes 5..12
        LD      DE,fp_rm
        LD      BC,8
        PUSH    AF
        LDIR
        POP     AF
        OR      A
        JR      Z,fp_mul__e
        LD      HL,fp_rm
        SET     0,(HL)
fp_mul__e:
        LD      HL,(fp_xa+8)            ; e = a.e + b.e + 42
        LD      DE,(fp_xb+8)
        ADD     HL,DE
        LD      DE,42
        ADD     HL,DE
        LD      (fp_re),HL
        LD      A,(fp_xa+10)
        LD      C,A
        LD      A,(fp_xb+10)
        XOR     C
        LD      (fp_rn),A
        LD      HL,(fp_mul__dst)
        JP      fp_finish

;! section bss
fp_mul__dst:
        DS      2
fp_mul__p:
        DS      16
