;! module    fp_mul
;! provides  fp_mul
;! requires  fp_finish fp_zero8 fp_w_unpack fp_xa fp_xb fp_rm fp_re fp_rn
;! group     float
;! clobbers  AF BC DE HL
;! doc       Multiplication, correctly rounded (ties to even): HL = destination, DE = a, BC = b.

; P = a.m * b.m (106 bits): schoolbook over bytes, row i adds a[i] * b (7 bytes each, 8 x 8 bit
; products unrolled) at byte i with the carry in one byte, zero bytes are skipped; m = P >> 42 with
; the lost bits as sticky bit, e = a.e + b.e + 42 (as fp_mul of runtime/ref). IX and IY are
; pointers here (saved).
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
        PUSH    IX
        PUSH    IY
        LD      HL,fp_mul__p            ; row i: P[i..i+7] += a[i] * b (the 7 bytes of b.m), the carry in B
        LD      (fp_mul__pp),HL
        LD      HL,fp_xa
        LD      (fp_mul__ap),HL
        LD      A,7
        LD      (fp_mul__i),A
fp_mul__row:
        LD      HL,(fp_mul__ap)
        LD      E,(HL)                  ; DE = a[i]
        INC     HL
        LD      (fp_mul__ap),HL
        LD      D,0
        LD      A,E
        OR      A
        JR      Z,fp_mul__nr            ; a zero byte adds nothing (P[i+7] stays 0)
        LD      IX,(fp_mul__pp)
        LD      IY,fp_xb
        LD      B,0
        LD      A,7
        LD      (fp_mul__j),A
fp_mul__st1:
        LD      A,(IY+0)                ; b[j]
        INC     IY
        OR      A
        JR      Z,fp_mul__z
        LD      HL,0                    ; HL = a[i] * b[j] (8 x 8 bits, unrolled)
        ADD     HL,HL
        RLA
        JR      NC,fp_mul__k0
        ADD     HL,DE
fp_mul__k0:
        ADD     HL,HL
        RLA
        JR      NC,fp_mul__k1
        ADD     HL,DE
fp_mul__k1:
        ADD     HL,HL
        RLA
        JR      NC,fp_mul__k2
        ADD     HL,DE
fp_mul__k2:
        ADD     HL,HL
        RLA
        JR      NC,fp_mul__k3
        ADD     HL,DE
fp_mul__k3:
        ADD     HL,HL
        RLA
        JR      NC,fp_mul__k4
        ADD     HL,DE
fp_mul__k4:
        ADD     HL,HL
        RLA
        JR      NC,fp_mul__k5
        ADD     HL,DE
fp_mul__k5:
        ADD     HL,HL
        RLA
        JR      NC,fp_mul__k6
        ADD     HL,DE
fp_mul__k6:
        ADD     HL,HL
        RLA
        JR      NC,fp_mul__k7
        ADD     HL,DE
fp_mul__k7:
        LD      A,L                     ; + carry
        ADD     A,B
        LD      L,A
        JR      NC,fp_mul__s2
        INC     H
fp_mul__s2:
        LD      A,(IX+0)                ; into P, the high byte is the next carry
        ADD     A,L
        LD      (IX+0),A
        LD      A,H
        ADC     A,0
        LD      B,A
        JR      fp_mul__s3
fp_mul__z:
        LD      A,(IX+0)                ; b[j] = 0: only the carry
        ADD     A,B
        LD      (IX+0),A
        LD      A,0
        ADC     A,A
        LD      B,A
fp_mul__s3:
        INC     IX
        LD      HL,fp_mul__j
        DEC     (HL)
        JP      NZ,fp_mul__st1
        LD      (IX+0),B                ; P[i+7] = carry (nothing there yet)
fp_mul__nr:
        LD      HL,(fp_mul__pp)
        INC     HL
        LD      (fp_mul__pp),HL
        LD      HL,fp_mul__i
        DEC     (HL)
        JP      NZ,fp_mul__row
        POP     IY
        POP     IX
        LD      HL,fp_mul__p            ; sticky: bytes 0..4 and the 2 low bits of byte 5
        LD      A,(HL)
        INC     HL
        OR      (HL)
        INC     HL
        OR      (HL)
        INC     HL
        OR      (HL)
        INC     HL
        OR      (HL)
        INC     HL
        LD      C,A
        LD      A,(HL)
        AND     3
        OR      C
        LD      (fp_mul__sk),A
        LD      D,2                     ; bytes 5..13 >> 2 (P >> 42 in bytes 5..12)
fp_mul__r2:
        LD      HL,fp_mul__p+13
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
        DEC     HL
        RR      (HL)
        DEC     D
        JR      NZ,fp_mul__r2
        LD      HL,fp_mul__p+5          ; m = bytes 5..12
        LD      DE,fp_rm
        LD      BC,8
        LDIR
        LD      A,(fp_mul__sk)
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
fp_mul__pp:
        DS      2
fp_mul__ap:
        DS      2
fp_mul__i:
        DS      1
fp_mul__j:
        DS      1
fp_mul__sk:
        DS      1
fp_mul__p:
        DS      16
