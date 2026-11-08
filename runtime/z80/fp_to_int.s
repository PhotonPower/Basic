;! module    fp_to_int
;! provides  fp_to_i32 fp_to_i16 fp_to_u8
;! requires  fp_w_unpack fp_w_shl1 fp_w_shr1 fp_xa fp_rm rt_error
;! group     core
;! clobbers  AF BC DE HL
;! doc       Float to %, & or |: truncation towards 0 (result as 32 bits at (HL)), range error 2, 4 or 3.

; HL = destination (4 bytes), DE = binary64
fp_to_i32:
        LD      A,2
        JR      fp_to_int__go
fp_to_i16:
        LD      A,4
        JR      fp_to_int__go
fp_to_u8:
        LD      A,3
fp_to_int__go:
        LD      (fp_to_int__err),A
        LD      (fp_to_int__dst),HL
        EX      DE,HL
        LD      DE,fp_xa
        CALL    fp_w_unpack
        LD      HL,fp_rm                ; mag = 0
        LD      B,8
fp_to_int__z:
        LD      (HL),0
        INC     HL
        DJNZ    fp_to_int__z
        LD      A,(fp_xa+11)
        OR      A
        JR      NZ,fp_to_int__range     ; 0
        LD      HL,(fp_xa+8)            ; e
        BIT     7,H
        JR      NZ,fp_to_int__neg
        LD      A,H                     ; e >= 0: e > 9 is out of every range
        OR      A
        JP      NZ,fp_to_int__bad
        LD      A,L
        CP      10
        JP      NC,fp_to_int__bad
        PUSH    AF
        LD      HL,fp_xa
        LD      DE,fp_rm
        LD      BC,8
        LDIR
        POP     AF
        OR      A
        JR      Z,fp_to_int__range
fp_to_int__sl:
        PUSH    AF
        LD      HL,fp_rm
        CALL    fp_w_shl1
        POP     AF
        DEC     A
        JR      NZ,fp_to_int__sl
        JR      fp_to_int__range
fp_to_int__neg:                         ; e < 0: mag = m >> -e (0 for e <= -53)
        LD      DE,-53
        OR      A
        PUSH    HL
        SBC     HL,DE
        POP     HL
        JP      M,fp_to_int__range
        JR      Z,fp_to_int__range
        XOR     A
        SUB     L                       ; -e (1..52)
        PUSH    AF
        LD      HL,fp_xa
        LD      DE,fp_rm
        LD      BC,8
        LDIR
        POP     AF
fp_to_int__sr:
        PUSH    AF
        LD      HL,fp_rm
        CALL    fp_w_shr1
        POP     AF
        DEC     A
        JR      NZ,fp_to_int__sr
fp_to_int__range:                       ; mag (< 2^62) against the range of the type
        LD      HL,fp_rm+4              ; above 32 bits: never
        LD      B,4
        XOR     A
fp_to_int__hi:
        OR      (HL)
        INC     HL
        DJNZ    fp_to_int__hi
        JP      NZ,fp_to_int__bad
        LD      A,(fp_to_int__err)
        CP      2
        JR      Z,fp_to_int__i32
        CP      4
        JR      Z,fp_to_int__i16
        LD      A,(fp_rm+3)             ; byte: mag <= 255, and a negative value only 0
        LD      C,A
        LD      A,(fp_rm+2)
        OR      C
        LD      C,A
        LD      A,(fp_rm+1)
        OR      C
        JP      NZ,fp_to_int__bad
        LD      A,(fp_xa+10)
        OR      A
        JR      Z,fp_to_int__ok
        LD      A,(fp_rm)
        OR      A
        JP      NZ,fp_to_int__bad
        JR      fp_to_int__ok
fp_to_int__i16:                         ; mag <= &7FFF, negative <= &8000
        LD      A,(fp_rm+3)
        LD      C,A
        LD      A,(fp_rm+2)
        OR      C
        JP      NZ,fp_to_int__bad
        LD      HL,(fp_rm)
        LD      DE,&8000
        JR      fp_to_int__lim
fp_to_int__i32:                         ; mag <= &7FFFFFFF, negative <= &80000000
        LD      HL,(fp_rm+2)
        LD      DE,&8000
        LD      A,H                     ; compare the high word first
        CP      &80
        JR      C,fp_to_int__ok
        JP      NZ,fp_to_int__bad
        LD      A,L
        OR      A
        JP      NZ,fp_to_int__bad
        LD      HL,(fp_rm)              ; high word &8000: only -2^31
        LD      A,H
        OR      L
        JP      NZ,fp_to_int__bad
        LD      A,(fp_xa+10)
        OR      A
        JP      Z,fp_to_int__bad
        JR      fp_to_int__ok
fp_to_int__lim:                         ; HL = mag (16 bit): < DE, or = DE and negative
        OR      A
        SBC     HL,DE
        JR      C,fp_to_int__ok
        JP      NZ,fp_to_int__bad
        LD      A,(fp_xa+10)
        OR      A
        JP      Z,fp_to_int__bad
fp_to_int__ok:                          ; the result: mag or -mag as 32 bits
        LD      DE,(fp_to_int__dst)
        LD      HL,fp_rm
        LD      BC,4
        LDIR
        LD      A,(fp_xa+10)
        OR      A
        RET     Z
        LD      HL,(fp_to_int__dst)
        LD      B,4
        OR      A
fp_to_int__ng:
        LD      A,0
        SBC     A,(HL)
        LD      (HL),A
        INC     HL
        DJNZ    fp_to_int__ng
        RET
fp_to_int__bad:
        LD      HL,(fp_to_int__dst)
        XOR     A
        LD      (HL),A
        INC     HL
        LD      (HL),A
        INC     HL
        LD      (HL),A
        INC     HL
        LD      (HL),A
        LD      A,(fp_to_int__err)
        JP      rt_error

;! section bss
fp_to_int__err:
        DS      1
fp_to_int__dst:
        DS      2
