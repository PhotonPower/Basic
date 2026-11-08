;! module    fp_add
;! provides  fp_add fp_sub fp_neg
;! requires  fp_finish fp_zero8 fp_w_unpack fp_w_shl1 fp_w_shr fp_w_add fp_w_sub fp_w_cmp fp_w_iszero fp_xa fp_xb fp_rm fp_re fp_rn fp_t
;! group     float
;! clobbers  AF BC DE HL
;! doc       Addition and subtraction (ties to even), negation: HL = destination, DE = a, BC = b.

; HL = destination, DE = a: -a (0 stays +0)
fp_neg:
        PUSH    HL
        EX      DE,HL
        LD      BC,8
        LDIR
        POP     HL
        LD      BC,6                    ; zero (be = 0)?
        ADD     HL,BC
        LD      A,(HL)
        AND     &F0
        LD      C,A
        INC     HL
        LD      A,(HL)
        AND     &7F
        OR      C
        JR      Z,fp_add__nz
        LD      A,(HL)
        XOR     &80
        LD      (HL),A
        RET
fp_add__nz:
        LD      BC,-7
        ADD     HL,BC
        JP      fp_zero8

; HL = destination, DE = a, BC = b: a - b
fp_sub:
        PUSH    HL
        PUSH    DE
        LD      HL,fp_add__nb
        LD      D,B
        LD      E,C
        CALL    fp_neg
        POP     DE
        POP     HL
        LD      BC,fp_add__nb

; HL = destination, DE = a, BC = b: a + b
fp_add:
        LD      (fp_add__dst),HL
        LD      (fp_add__pa),DE
        LD      (fp_add__pb),BC
        EX      DE,HL
        LD      DE,fp_xa
        CALL    fp_w_unpack
        LD      HL,(fp_add__pb)
        LD      DE,fp_xb
        CALL    fp_w_unpack
        LD      A,(fp_xb+11)            ; b = 0: a
        OR      A
        JR      Z,fp_add__b
        LD      HL,(fp_add__pa)
        LD      A,(fp_xa+11)
        OR      A
        JR      Z,fp_add__copy
        LD      HL,(fp_add__dst)
        JP      fp_zero8
fp_add__b:
        LD      A,(fp_xa+11)            ; a = 0: b
        OR      A
        JR      Z,fp_add__go
        LD      HL,(fp_add__pb)
fp_add__copy:
        LD      DE,(fp_add__dst)
        LD      BC,8
        LDIR
        RET
fp_add__go:
        LD      HL,(fp_xb+8)            ; |b| > |a|: exchange (a gets the larger magnitude)
        LD      DE,(fp_xa+8)
        OR      A
        SBC     HL,DE                   ; b.e - a.e
        JR      Z,fp_add__eqe
        JP      M,fp_add__ok            ; b.e < a.e
        JP      PO,fp_add__swap         ; b.e > a.e, no overflow
        JR      fp_add__ok              ; (no 16-bit overflow occurs for real exponents)
fp_add__eqe:
        LD      HL,fp_xb
        LD      DE,fp_xa
        CALL    fp_w_cmp
        JR      Z,fp_add__ok
        JR      C,fp_add__ok
fp_add__swap:
        LD      HL,fp_xa
        LD      DE,fp_xb
        LD      B,12
fp_add__sw:
        LD      A,(DE)
        LD      C,(HL)
        LD      (HL),A
        LD      A,C
        LD      (DE),A
        INC     HL
        INC     DE
        DJNZ    fp_add__sw
fp_add__ok:
        LD      HL,fp_xa                ; mx = a.m << 10 -> fp_rm, my = b.m << 10 -> fp_t
        LD      DE,fp_rm
        CALL    fp_add__shl10
        LD      HL,fp_xb
        LD      DE,fp_t
        CALL    fp_add__shl10
        LD      HL,(fp_xa+8)            ; d = a.e - b.e (>= 0)
        LD      DE,(fp_xb+8)
        OR      A
        SBC     HL,DE
        LD      A,H
        OR      A
        JR      NZ,fp_add__far
        LD      A,L
        CP      63
        JR      NC,fp_add__far
        LD      HL,fp_t                 ; my >>= d with the sticky bit
        CALL    fp_w_shr
        JR      fp_add__sum
fp_add__far:
        LD      HL,fp_t                 ; only the sticky bit is left
        LD      B,8
fp_add__f0:
        LD      (HL),0
        INC     HL
        DJNZ    fp_add__f0
        LD      A,1
        LD      (fp_t),A
fp_add__sum:
        LD      A,(fp_xa+10)
        LD      C,A
        LD      A,(fp_xb+10)
        CP      C
        LD      HL,fp_rm
        LD      DE,fp_t
        JR      NZ,fp_add__diff
        CALL    fp_w_add
        JR      fp_add__fin
fp_add__diff:
        CALL    fp_w_sub
        LD      HL,fp_rm
        CALL    fp_w_iszero
        JR      NZ,fp_add__fin
        LD      HL,(fp_add__dst)        ; x - x is +0
        JP      fp_zero8
fp_add__fin:
        LD      A,(fp_xa+10)
        LD      (fp_rn),A
        LD      HL,(fp_xa+8)
        LD      BC,-10
        ADD     HL,BC
        LD      (fp_re),HL
        LD      HL,(fp_add__dst)
        JP      fp_finish

; HL = unpacked value, DE = 8 bytes: m << 10
fp_add__shl10:
        PUSH    DE
        INC     DE                      ; one byte up: bytes 0..6 -> 1..7
        LD      BC,7
        LDIR
        POP     HL
        LD      (HL),0
        PUSH    HL
        CALL    fp_w_shl1
        POP     HL
        JP      fp_w_shl1

;! section bss
fp_add__dst:
        DS      2
fp_add__pa:
        DS      2
fp_add__pb:
        DS      2
fp_add__nb:
        DS      8
