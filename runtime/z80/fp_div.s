;! module    fp_div
;! provides  fp_div
;! requires  fp_finish fp_zero8 fp_w_unpack fp_w_shl1 fp_w_sub fp_w_cmp fp_w_iszero fp_xa fp_xb fp_rm fp_re fp_rn fp_t rt_error
;! group     float
;! clobbers  AF BC DE HL
;! doc       Division, correctly rounded (ties to even): HL = destination, DE = a, BC = b; b = 0 -> error 0.

; Restoring division as fp_div of runtime/ref: q = floor(a.m / b.m * 2^63), the rest as sticky bit,
; e = a.e - b.e - 63.
fp_div:
        LD      (fp_div__dst),HL
        PUSH    BC
        EX      DE,HL
        LD      DE,fp_xa
        CALL    fp_w_unpack
        POP     HL
        LD      DE,fp_xb
        CALL    fp_w_unpack
        LD      A,(fp_xb+11)
        OR      A
        JR      Z,fp_div__nz
        LD      HL,(fp_div__dst)        ; division by 0
        CALL    fp_zero8
        XOR     A
        JP      rt_error
fp_div__nz:
        LD      A,(fp_xa+11)
        OR      A
        JR      Z,fp_div__go
        LD      HL,(fp_div__dst)
        JP      fp_zero8
fp_div__go:
        LD      HL,fp_xa                ; r = a.m (fp_t), q = 0 (fp_rm)
        LD      DE,fp_t
        LD      BC,8
        LDIR
        LD      HL,fp_rm
        LD      B,8
fp_div__c:
        LD      (HL),0
        INC     HL
        DJNZ    fp_div__c
        LD      A,64
fp_div__loop:
        PUSH    AF
        LD      HL,fp_rm                ; q <<= 1
        CALL    fp_w_shl1
        LD      HL,fp_t                 ; r >= b.m: subtract, q |= 1
        LD      DE,fp_xb
        CALL    fp_w_cmp
        JR      C,fp_div__no
        LD      HL,fp_t
        LD      DE,fp_xb
        CALL    fp_w_sub
        LD      HL,fp_rm
        SET     0,(HL)
fp_div__no:
        LD      HL,fp_t                 ; r <<= 1
        CALL    fp_w_shl1
        POP     AF
        DEC     A
        JR      NZ,fp_div__loop
        LD      HL,fp_t
        CALL    fp_w_iszero
        JR      Z,fp_div__e
        LD      HL,fp_rm
        SET     0,(HL)
fp_div__e:
        LD      HL,(fp_xa+8)            ; e = a.e - b.e - 63
        LD      DE,(fp_xb+8)
        OR      A
        SBC     HL,DE
        LD      DE,-63
        ADD     HL,DE
        LD      (fp_re),HL
        LD      A,(fp_xa+10)
        LD      C,A
        LD      A,(fp_xb+10)
        XOR     C
        LD      (fp_rn),A
        LD      HL,(fp_div__dst)
        JP      fp_finish

;! section bss
fp_div__dst:
        DS      2
