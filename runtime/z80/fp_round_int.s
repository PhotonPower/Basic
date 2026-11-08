;! module    fp_round_int
;! provides  fp_trunc fp_int fp_frac fp_cint
;! requires  fp_round_pack fp_sub fp_zero8 fp_w_unpack fp_w_add fp_xa fp_rm fp_re fp_rn fp_t
;! group     float
;! clobbers  AF BC DE HL
;! doc       TRUNC/FIX, INT (floor), FRAC, CINT (nearest, ties away from 0): HL = destination, DE = a.

; HL = destination, DE = a
fp_trunc:
        LD      (fp_round_int__dst),HL
        LD      (fp_round_int__src),DE
        CALL    fp_round_int__unp       ; Z: |a| >= 1 or 0 (a itself); C: |a| < 1 (0)
        JP      Z,fp_round_int__copy
        JP      C,fp_round_int__zero
        CALL    fp_round_int__mask      ; clear the -e lowest bits of m
        JP      fp_round_int__pack

; HL = destination, DE = a: floor
fp_int:
        LD      (fp_round_int__dst2),HL
        LD      (fp_round_int__src2),DE
        CALL    fp_trunc
        LD      HL,(fp_round_int__src2) ; negative and not whole: one less
        LD      BC,7
        ADD     HL,BC
        BIT     7,(HL)
        RET     Z
        LD      HL,(fp_round_int__src2)
        LD      DE,(fp_round_int__dst2)
        LD      B,8
fp_round_int__ne:
        LD      A,(DE)
        CP      (HL)
        JR      NZ,fp_round_int__dec
        INC     HL
        INC     DE
        DJNZ    fp_round_int__ne
        RET
fp_round_int__dec:
        LD      HL,(fp_round_int__dst2)
        LD      DE,(fp_round_int__dst2)
        LD      BC,fp_round_int__one
        JP      fp_sub

; HL = destination, DE = a: a - TRUNC(a)
fp_frac:
        LD      (fp_round_int__dst2),HL
        LD      (fp_round_int__src2),DE
        LD      HL,fp_round_int__tmp
        CALL    fp_trunc
        LD      HL,(fp_round_int__dst2)
        LD      DE,(fp_round_int__src2)
        LD      BC,fp_round_int__tmp
        JP      fp_sub

; HL = destination, DE = a: nearest, ties away from 0
fp_cint:
        LD      (fp_round_int__dst),HL
        LD      (fp_round_int__src),DE
        CALL    fp_round_int__unp
        JP      Z,fp_round_int__copy
        LD      HL,(fp_xa+8)            ; e < -53 (|a| < 0.5): 0
        LD      DE,-53
        OR      A
        SBC     HL,DE
        JP      M,fp_round_int__zero
        LD      A,(fp_xa+8)             ; m += 2^(-e-1)
        NEG
        DEC     A
        LD      HL,fp_t                 ; fp_t = 1 << (-e-1)
        LD      B,8
fp_round_int__c:
        LD      (HL),0
        INC     HL
        DJNZ    fp_round_int__c
        LD      C,A
        SRL     A
        SRL     A
        SRL     A
        LD      E,A
        LD      D,0
        LD      HL,fp_t
        ADD     HL,DE
        LD      A,C
        AND     7
        LD      B,A
        LD      A,1
        JR      Z,fp_round_int__b2
fp_round_int__b1:
        ADD     A,A
        DJNZ    fp_round_int__b1
fp_round_int__b2:
        LD      (HL),A
        LD      HL,fp_xa
        LD      DE,fp_t
        CALL    fp_w_add
        CALL    fp_round_int__mask
fp_round_int__pack:
        LD      HL,fp_xa
        LD      DE,fp_rm
        LD      BC,8
        LDIR
        LD      HL,(fp_xa+8)
        LD      (fp_re),HL
        LD      A,(fp_xa+10)
        LD      (fp_rn),A
        LD      HL,(fp_round_int__dst)
        JP      fp_round_pack
fp_round_int__copy:
        LD      HL,(fp_round_int__src)
        LD      DE,(fp_round_int__dst)
        LD      BC,8
        LDIR
        RET
fp_round_int__zero:
        LD      HL,(fp_round_int__dst)
        JP      fp_zero8

; unpacks the source: Z if it is 0 or e >= 0; C (NZ) if e <= -53; else NZ NC
fp_round_int__unp:
        LD      HL,(fp_round_int__src)
        LD      DE,fp_xa
        CALL    fp_w_unpack
        LD      A,(fp_xa+11)
        OR      A
        JR      NZ,fp_round_int__z
        LD      A,(fp_xa+9)
        BIT     7,A
        JR      Z,fp_round_int__z       ; e >= 0
        LD      HL,(fp_xa+8)            ; e <= -53?
        LD      DE,-52
        OR      A
        SBC     HL,DE                   ; e + 52 < 0
        JP      M,fp_round_int__lo
        LD      A,1
        OR      A                       ; NZ, NC
        RET
fp_round_int__lo:
        LD      A,1
        OR      A                       ; NZ
        SCF                             ; C
        RET
fp_round_int__z:
        XOR     A                       ; Z
        RET

; clears the -e lowest bits of the significand in fp_xa (1 <= -e <= 53)
fp_round_int__mask:
        LD      A,(fp_xa+8)
        NEG
        LD      C,A                     ; C = bits to clear
        LD      HL,fp_xa
fp_round_int__mb:
        LD      A,C
        CP      8
        JR      C,fp_round_int__mr
        LD      (HL),0
        INC     HL
        SUB     8
        LD      C,A
        JR      fp_round_int__mb
fp_round_int__mr:
        OR      A
        RET     Z
        LD      B,A                     ; mask = &FF << C
        LD      A,&FF
fp_round_int__ms:
        ADD     A,A
        DJNZ    fp_round_int__ms
        AND     (HL)
        LD      (HL),A
        RET

;! section data
fp_round_int__one:
        DB      0,0,0,0,0,0,&F0,&3F

;! section bss
fp_round_int__dst:
        DS      2
fp_round_int__src:
        DS      2
fp_round_int__dst2:
        DS      2
fp_round_int__src2:
        DS      2
fp_round_int__tmp:
        DS      8
