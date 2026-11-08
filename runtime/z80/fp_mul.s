;! module    fp_mul
;! provides  fp_mul
;! requires  fp_finish fp_zero8 fp_w_unpack fp_xa fp_xb fp_rm fp_re fp_rn
;! group     float
;! clobbers  AF BC DE HL
;! doc       Multiplication, correctly rounded (ties to even): HL = destination, DE = a, BC = b.

; P = a.m * b.m (106 bits): schoolbook over bytes, row i adds a[i] * b (7 bytes each) at byte i with
; the carry in one byte, zero bytes are skipped; the 8 x 8 bit products from a table of quarter
; squares (a*b = T[a+b] - T[|a-b|], 1 KB on a page boundary, filled on the first call); m = P >> 42 with
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
        LD      A,(fp_mul__ready)       ; the table on the first call
        OR      A
        CALL    Z,fp_mul__init
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
        LD      C,(HL)                  ; C = a[i]
        INC     HL
        LD      (fp_mul__ap),HL
        LD      A,C
        OR      A
        JR      Z,fp_mul__nr            ; a zero byte adds nothing (P[i+7] stays 0)
        LD      IX,(fp_mul__pp)
        LD      IY,fp_xb
        LD      D,fp_mul__tab/256       ; D = the page of the table
        LD      B,0
        LD      A,7
        LD      (fp_mul__j),A
fp_mul__st1:
        LD      A,(IY+0)                ; b[j]
        INC     IY
        OR      A
        JR      Z,fp_mul__z
        LD      L,A                     ; HL = a[i] * b[j] = T[a+b] - T[|a-b|], T[n] = n*n/4
        SUB     C
        JR      NC,fp_mul__d
        NEG
fp_mul__d:
        LD      E,A                     ; DE -> TL[d]
        LD      A,L
        ADD     A,C
        LD      L,A
        SBC     A,A                     ; s >= 256: two pages further
        AND     2
        ADD     A,D
        LD      H,A                     ; HL -> TL[s]
        LD      A,(HL)
        INC     H
        LD      H,(HL)
        LD      L,A                     ; HL = T[s]
        EX      DE,HL                   ; DE = T[s], HL -> TL[d]
        LD      A,E
        SUB     (HL)
        LD      E,A
        INC     H
        LD      A,D
        SBC     A,(HL)
        DEC     H
        LD      D,H                     ; the page again
        LD      H,A
        LD      L,E
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

; the table T[n] = n*n/4 (n = 0..511): page P low bytes of 0..255, P+1 their high bytes, P+2 and
; P+3 the same for 256..511; T[n+1] = T[n] + (n+1)/2
fp_mul__init:
        LD      A,1
        LD      (fp_mul__ready),A
        LD      HL,0                    ; HL = T[n]
        LD      BC,0                    ; BC = n
        LD      DE,fp_mul__tab
        LD      A,2
fp_mul__ih:
        PUSH    AF
fp_mul__il:
        LD      A,L
        LD      (DE),A
        INC     D
        LD      A,H
        LD      (DE),A
        DEC     D
        INC     BC
        PUSH    BC
        SRL     B
        RR      C
        ADD     HL,BC
        POP     BC
        INC     E
        JR      NZ,fp_mul__il
        INC     D
        INC     D
        POP     AF
        DEC     A
        JR      NZ,fp_mul__ih
        RET

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
fp_mul__ready:
        DS      1
fp_mul__pad:                            ; the table on a page boundary
        DS      (256-($ & 255)) & 255
fp_mul__tab:
        DS      1024
fp_mul__p:
        DS      16
