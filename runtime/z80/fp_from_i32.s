;! module    fp_from_i32
;! provides  fp_from_i32
;! requires  fp_round_pack fp_rm fp_re fp_rn
;! group     float
;! clobbers  AF BC DE HL
;! doc       Exact conversion of a 32-bit integer: HL = destination, DE = the integer (4 bytes).

fp_from_i32:
        PUSH    HL
        EX      DE,HL
        LD      DE,fp_rm
        LD      BC,4
        LDIR                            ; low 4 bytes of m
        XOR     A
        LD      (DE),A
        INC     DE
        LD      (DE),A
        INC     DE
        LD      (DE),A
        INC     DE
        LD      (DE),A
        LD      (fp_re),A
        LD      (fp_re+1),A
        LD      A,(fp_rm+3)
        RLCA
        AND     1
        LD      (fp_rn),A
        JR      Z,fp_from_i32__pos
        LD      HL,fp_rm                ; the magnitude: 0 - v (as 64 bits)
        LD      B,4
        OR      A
fp_from_i32__neg:
        LD      A,0
        SBC     A,(HL)
        LD      (HL),A
        INC     HL
        DJNZ    fp_from_i32__neg
fp_from_i32__pos:
        POP     HL
        JP      fp_round_pack
