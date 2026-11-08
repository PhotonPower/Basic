;! module    fp_sqrt
;! provides  fp_sqrt
;! requires  fp_finish fp_zero8 fp_w_unpack fp_w_shl1 fp_xa fp_rm fp_re fp_rn rt_error
;! group     float
;! clobbers  AF BC DE HL
;! doc       Square root, correctly rounded (ties to even): HL = destination, DE = a; a < 0 -> error 5.

; As fp_sqrt of runtime/ref: R = m * 2^72 (e even), r = floor(sqrt(R)), inexact if r^2 != R. Here
; digit by digit: two bits of R into the rest, trial = 4r + 1, subtract if it fits.
fp_sqrt:
        LD      (fp_sqrt__dst),HL
        EX      DE,HL
        LD      DE,fp_xa
        CALL    fp_w_unpack
        LD      A,(fp_xa+11)
        OR      A
        JR      Z,fp_sqrt__nz
        LD      HL,(fp_sqrt__dst)
        JP      fp_zero8
fp_sqrt__nz:
        LD      A,(fp_xa+10)
        OR      A
        JR      Z,fp_sqrt__pos
        LD      HL,(fp_sqrt__dst)       ; negative
        CALL    fp_zero8
        LD      A,5
        JP      rt_error
fp_sqrt__pos:
        LD      A,(fp_xa+8)             ; e odd: m <<= 1, e -= 1
        AND     1
        JR      Z,fp_sqrt__even
        LD      HL,fp_xa
        CALL    fp_w_shl1
        LD      HL,(fp_xa+8)
        DEC     HL
        LD      (fp_xa+8),HL
fp_sqrt__even:
        LD      HL,fp_sqrt__r           ; R (16 bytes) = m << 72: bytes 9..15 = m bytes 0..6
        LD      B,16
fp_sqrt__c0:
        LD      (HL),0
        INC     HL
        DJNZ    fp_sqrt__c0
        LD      HL,fp_xa
        LD      DE,fp_sqrt__r+9
        LD      BC,7
        LDIR
        LD      HL,fp_sqrt__rest        ; rest and root = 0 (10 bytes each)
        LD      B,20
fp_sqrt__c1:
        LD      (HL),0
        INC     HL
        DJNZ    fp_sqrt__c1
        LD      A,64
fp_sqrt__loop:
        PUSH    AF
        CALL    fp_sqrt__in             ; rest = rest << 2 | the next two bits of R
        CALL    fp_sqrt__in
        LD      HL,fp_sqrt__root        ; trial = root << 2 | 1
        LD      DE,fp_sqrt__t
        LD      BC,10
        LDIR
        LD      HL,fp_sqrt__t
        CALL    fp_sqrt__shl10
        LD      HL,fp_sqrt__t
        CALL    fp_sqrt__shl10
        LD      HL,fp_sqrt__t
        SET     0,(HL)
        LD      HL,fp_sqrt__root        ; root <<= 1
        CALL    fp_sqrt__shl10
        LD      HL,fp_sqrt__rest+9      ; rest >= trial?
        LD      DE,fp_sqrt__t+9
        LD      B,10
fp_sqrt__cmp:
        LD      A,(DE)
        LD      C,A
        LD      A,(HL)
        CP      C
        JR      NZ,fp_sqrt__cd
        DEC     HL
        DEC     DE
        DJNZ    fp_sqrt__cmp
fp_sqrt__cd:
        JR      C,fp_sqrt__next         ; rest < trial
        LD      HL,fp_sqrt__rest        ; rest -= trial, root |= 1
        LD      DE,fp_sqrt__t
        LD      B,10
        OR      A
fp_sqrt__sub:
        LD      A,(DE)
        LD      C,A
        LD      A,(HL)
        SBC     A,C
        LD      (HL),A
        INC     HL
        INC     DE
        DJNZ    fp_sqrt__sub
        LD      HL,fp_sqrt__root
        SET     0,(HL)
fp_sqrt__next:
        POP     AF
        DEC     A
        JR      NZ,fp_sqrt__loop
        LD      HL,fp_sqrt__root        ; m = root, sticky: rest /= 0
        LD      DE,fp_rm
        LD      BC,8
        LDIR
        LD      HL,fp_sqrt__rest
        LD      B,10
        XOR     A
fp_sqrt__st:
        OR      (HL)
        INC     HL
        DJNZ    fp_sqrt__st
        JR      Z,fp_sqrt__e
        LD      HL,fp_rm
        SET     0,(HL)
fp_sqrt__e:
        LD      HL,(fp_xa+8)            ; e = (e - 72) / 2
        LD      BC,-72
        ADD     HL,BC
        SRA     H
        RR      L
        LD      (fp_re),HL
        XOR     A
        LD      (fp_rn),A
        LD      HL,(fp_sqrt__dst)
        JP      fp_finish

; the top bit of R into the rest: R <<= 1, rest = rest << 1 | carry
fp_sqrt__in:
        LD      HL,fp_sqrt__r
        OR      A
        LD      B,16
fp_sqrt__i1:
        RL      (HL)
        INC     HL
        DJNZ    fp_sqrt__i1
        LD      HL,fp_sqrt__rest
        LD      B,10
fp_sqrt__i2:
        RL      (HL)
        INC     HL
        DJNZ    fp_sqrt__i2
        RET

; HL = 10 bytes: << 1
fp_sqrt__shl10:
        OR      A
        LD      B,10
fp_sqrt__s1:
        RL      (HL)
        INC     HL
        DJNZ    fp_sqrt__s1
        RET

;! section bss
fp_sqrt__dst:
        DS      2
fp_sqrt__r:
        DS      16
fp_sqrt__rest:
        DS      10
fp_sqrt__root:
        DS      10
fp_sqrt__t:
        DS      10
