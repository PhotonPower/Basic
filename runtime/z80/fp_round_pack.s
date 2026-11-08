;! module    fp_round_pack
;! provides  fp_round_pack fp_finish fp_zero8
;! requires  fp_w_iszero fp_w_shl1 fp_w_shr1 fp_rm fp_re fp_rn rt_error
;! group     float
;! clobbers  AF BC DE HL
;! doc       Rounds m * 2^e (fp_rm, fp_re, sign fp_rn) to binary64 at (HL), ties to even.

; fp_round_pack: CF set on overflow (the result is then 0). Below 2^-1022 the result is 0.
; fp_finish: the same, overflow -> error 1.

fp_finish:
        CALL    fp_round_pack
        RET     NC
        LD      A,1                     ; overflow
        JP      rt_error

fp_round_pack:
        LD      (fp_round_pack__dst),HL
        LD      HL,fp_rm
        CALL    fp_w_iszero
        JP      Z,fp_round_pack__zero
fp_round_pack__nb:                      ; normalise: whole bytes first
        LD      A,(fp_rm+7)
        OR      A
        JR      NZ,fp_round_pack__nbit
        LD      HL,fp_rm+6
        LD      DE,fp_rm+7
        LD      BC,7
        LDDR
        XOR     A
        LD      (fp_rm),A
        LD      HL,(fp_re)
        LD      BC,-8
        ADD     HL,BC
        LD      (fp_re),HL
        JR      fp_round_pack__nb
fp_round_pack__nbit:                    ; then bits until bit 63 is set
        LD      A,(fp_rm+7)
        BIT     7,A
        JR      NZ,fp_round_pack__norm
        LD      HL,fp_rm
        CALL    fp_w_shl1
        LD      HL,(fp_re)
        DEC     HL
        LD      (fp_re),HL
        JR      fp_round_pack__nbit
fp_round_pack__norm:                    ; the 11 bits below the 53 kept ones
        LD      A,(fp_rm)
        LD      (fp_round_pack__rlo),A
        LD      A,(fp_rm+1)
        AND     7
        LD      (fp_round_pack__rhi),A
        LD      HL,fp_rm+1              ; m >>= 11: one byte, then 3 bits
        LD      DE,fp_rm
        LD      BC,7
        LDIR
        XOR     A
        LD      (fp_rm+7),A
        LD      HL,fp_rm
        CALL    fp_w_shr1
        LD      HL,fp_rm
        CALL    fp_w_shr1
        LD      HL,fp_rm
        CALL    fp_w_shr1
        LD      HL,(fp_re)
        LD      BC,11
        ADD     HL,BC
        LD      (fp_re),HL
        LD      A,(fp_round_pack__rhi)  ; round: above half, or half and m odd
        CP      4
        JR      C,fp_round_pack__pack
        JR      NZ,fp_round_pack__up
        LD      A,(fp_round_pack__rlo)
        OR      A
        JR      NZ,fp_round_pack__up
        LD      A,(fp_rm)
        AND     1
        JR      Z,fp_round_pack__pack
fp_round_pack__up:
        LD      HL,fp_rm
        LD      B,8
fp_round_pack__inc:
        INC     (HL)
        JR      NZ,fp_round_pack__inc2
        INC     HL
        DJNZ    fp_round_pack__inc
fp_round_pack__inc2:
        LD      A,(fp_rm+6)             ; 2^53: one bit back
        BIT     5,A
        JR      Z,fp_round_pack__pack
        LD      HL,fp_rm
        CALL    fp_w_shr1
        LD      HL,(fp_re)
        INC     HL
        LD      (fp_re),HL
fp_round_pack__pack:
        LD      HL,(fp_re)              ; be = e + 1075
        LD      BC,1075
        ADD     HL,BC
        BIT     7,H
        JR      NZ,fp_round_pack__zero  ; below the range
        LD      A,H
        OR      L
        JR      Z,fp_round_pack__zero
        LD      DE,2047
        OR      A
        SBC     HL,DE
        JR      NC,fp_round_pack__ovf
        ADD     HL,DE                   ; HL = be (1..2046)
        LD      (fp_round_pack__be),HL
        LD      DE,(fp_round_pack__dst)
        LD      HL,fp_rm
        LD      BC,6
        LDIR                            ; bytes 0..5
        LD      A,(fp_round_pack__be)   ; byte 6: be bits 3..0, fraction bits 51..48
        RLCA
        RLCA
        RLCA
        RLCA
        AND     &F0
        LD      C,A
        LD      A,(fp_rm+6)
        AND     &0F
        OR      C
        LD      (DE),A
        INC     DE
        LD      HL,(fp_round_pack__be)  ; byte 7: sign, be bits 10..4
        LD      B,4
fp_round_pack__sh:
        SRL     H
        RR      L
        DJNZ    fp_round_pack__sh
        LD      A,(fp_rn)
        RRCA
        AND     &80
        OR      L
        LD      (DE),A
        OR      A                       ; NC
        RET
fp_round_pack__ovf:
        LD      HL,(fp_round_pack__dst)
        CALL    fp_zero8
        SCF
        RET
fp_round_pack__zero:
        LD      HL,(fp_round_pack__dst)
        CALL    fp_zero8
        OR      A
        RET

; HL = 8 bytes: +0
fp_zero8:
        LD      B,8
fp_round_pack__z:
        LD      (HL),0
        INC     HL
        DJNZ    fp_round_pack__z
        RET

;! section bss
fp_round_pack__dst:
        DS      2
fp_round_pack__rlo:
        DS      1
fp_round_pack__rhi:
        DS      1
fp_round_pack__be:
        DS      2
