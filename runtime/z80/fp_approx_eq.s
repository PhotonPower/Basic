;! module    fp_approx_eq
;! provides  fp_approx_eq
;! requires  fp_w_unpack fp_w_cmp fp_xa fp_xb
;! group     float
;! clobbers  AF BC DE HL
;! doc       GFA "==": both values rounded to 28 significant bits (ties away from 0), then compared. HL = 32-bit 1/0.

fp_approx_eq:
        LD      (fp_approx_eq__dst),HL
        PUSH    BC
        EX      DE,HL
        LD      DE,fp_xa
        CALL    fp_w_unpack
        POP     HL
        LD      DE,fp_xb
        CALL    fp_w_unpack
        LD      A,(fp_xa+11)            ; a zero: equal only to zero
        LD      C,A
        LD      A,(fp_xb+11)
        OR      C
        JR      Z,fp_approx_eq__nz
        LD      A,(fp_xa+11)
        LD      C,A
        LD      A,(fp_xb+11)
        AND     C
        JR      fp_approx_eq__put
fp_approx_eq__nz:
        LD      A,(fp_xa+10)
        LD      C,A
        LD      A,(fp_xb+10)
        CP      C
        LD      A,0
        JR      NZ,fp_approx_eq__put
        LD      HL,fp_xa
        CALL    fp_approx_eq__key
        LD      HL,fp_xb
        CALL    fp_approx_eq__key
        LD      HL,(fp_xa+8)
        LD      DE,(fp_xb+8)
        OR      A
        SBC     HL,DE
        LD      A,0
        JR      NZ,fp_approx_eq__put
        LD      HL,fp_xa
        LD      DE,fp_xb
        CALL    fp_w_cmp
        LD      A,0
        JR      NZ,fp_approx_eq__put
        INC     A
fp_approx_eq__put:
        LD      HL,(fp_approx_eq__dst)
        LD      (HL),A
        INC     HL
        XOR     A
        LD      (HL),A
        INC     HL
        LD      (HL),A
        INC     HL
        LD      (HL),A
        RET

; HL = unpacked value: m = (m + 2^24) >> 25; 2^28: m >>= 1, e += 1
fp_approx_eq__key:
        PUSH    HL
        INC     HL                      ; + 2^24: byte 3
        INC     HL
        INC     HL
        LD      B,5
        SCF
        CCF
        LD      A,(HL)
        ADD     A,1
        LD      (HL),A
        JR      NC,fp_approx_eq__k2
fp_approx_eq__k1:
        INC     HL
        DEC     B
        JR      Z,fp_approx_eq__k2
        INC     (HL)
        JR      Z,fp_approx_eq__k1
fp_approx_eq__k2:
        POP     HL                      ; >> 25: three bytes, then one bit
        PUSH    HL
        LD      D,H
        LD      E,L
        INC     HL
        INC     HL
        INC     HL
        LD      BC,5
        LDIR
        XOR     A
        LD      (DE),A
        INC     DE
        LD      (DE),A
        INC     DE
        LD      (DE),A
        POP     HL
        PUSH    HL
        LD      BC,7
        ADD     HL,BC
        OR      A
        LD      B,8
fp_approx_eq__r:
        RR      (HL)
        DEC     HL
        DJNZ    fp_approx_eq__r
        POP     HL
        INC     HL                      ; m = 2^28 (byte 3 = &10, rest 0)?
        INC     HL
        INC     HL
        LD      A,(HL)
        CP      &10
        RET     NZ
        DEC     HL
        LD      A,(HL)
        DEC     HL
        OR      (HL)
        DEC     HL
        OR      (HL)
        RET     NZ
        LD      (HL),0                  ; m = 2^27, e += 1
        INC     HL
        INC     HL
        INC     HL
        LD      (HL),&08
        LD      BC,5
        ADD     HL,BC                   ; +8: e
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        INC     DE
        LD      (HL),D
        DEC     HL
        LD      (HL),E
        RET

;! section bss
fp_approx_eq__dst:
        DS      2
