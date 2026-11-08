;! module    rt_int
;! provides  rt_idiv rt_imod rt_imul rt_conv_word rt_conv_byte rt_wrap_word rt_wrap_byte rt_iadd_chk rt_isub_chk rt_imul_chk
;! requires  rt_error rt_argp zi_addv zi_subv zi_mulv
;! group     core
;! clobbers  AF BC DE HL
;! doc       32-bit integer helpers: DIV/MOD (error 0), multiplication, range checks (errors 4, 3), wrap.

; HL = result, DE = a, BC = b (pointers to 32-bit values). The _chk routines get the error number
; as the third argument (rt_argp: pointer to a 32-bit value).

rt_idiv:
        CALL    rt_int__divmod
        RET     C                       ; division by 0: the result is 0
        LD      DE,rt_int__q
        JR      rt_int__out
rt_imod:
        CALL    rt_int__divmod
        RET     C
        LD      DE,rt_int__r
rt_int__out:
        LD      HL,(rt_int__dst)
        EX      DE,HL
        LD      BC,4
        LDIR
        RET

; (DE) / (BC) truncated towards 0 -> rt_int__q, remainder rt_int__r (sign of the dividend); CF
; set after the error for b = 0
rt_int__divmod:
        LD      (rt_int__dst),HL
        LD      H,B
        LD      L,C
        LD      A,(HL)
        INC     HL
        OR      (HL)
        INC     HL
        OR      (HL)
        INC     HL
        OR      (HL)
        JR      NZ,rt_int__ok
        LD      HL,(rt_int__dst)
        CALL    rt_int__zero
        XOR     A                       ; division by zero
        CALL    rt_error
        SCF
        RET
rt_int__ok:
        PUSH    BC
        EX      DE,HL
        LD      DE,rt_int__a
        CALL    rt_int__abs
        LD      (rt_int__sa),A          ; sign of the dividend
        POP     HL
        LD      DE,rt_int__b
        CALL    rt_int__abs
        LD      C,A
        LD      A,(rt_int__sa)
        XOR     C
        LD      (rt_int__sq),A          ; sign of the quotient
        LD      HL,rt_int__q            ; q = r = 0
        LD      B,8
rt_int__z0:
        LD      (HL),0
        INC     HL
        DJNZ    rt_int__z0
        LD      A,32
rt_int__loop:
        PUSH    AF
        LD      HL,rt_int__a            ; the next bit of |a| into r
        OR      A
        LD      B,4
rt_int__l1:
        RL      (HL)
        INC     HL
        DJNZ    rt_int__l1
        LD      HL,rt_int__r
        LD      B,4
rt_int__l2:
        RL      (HL)
        INC     HL
        DJNZ    rt_int__l2
        LD      HL,rt_int__q            ; q <<= 1
        OR      A
        LD      B,4
rt_int__l3:
        RL      (HL)
        INC     HL
        DJNZ    rt_int__l3
        LD      HL,rt_int__r+3          ; r >= |b|?
        LD      DE,rt_int__b+3
        LD      B,4
rt_int__c1:
        LD      A,(DE)
        LD      C,A
        LD      A,(HL)
        CP      C
        JR      NZ,rt_int__c2
        DEC     HL
        DEC     DE
        DJNZ    rt_int__c1
rt_int__c2:
        JR      C,rt_int__next
        LD      HL,rt_int__r            ; r -= |b|, q |= 1
        LD      DE,rt_int__b
        OR      A
        LD      B,4
rt_int__s1:
        LD      A,(DE)
        LD      C,A
        LD      A,(HL)
        SBC     A,C
        LD      (HL),A
        INC     HL
        INC     DE
        DJNZ    rt_int__s1
        LD      HL,rt_int__q
        SET     0,(HL)
rt_int__next:
        POP     AF
        DEC     A
        JR      NZ,rt_int__loop
        LD      A,(rt_int__sq)
        OR      A
        LD      HL,rt_int__q
        CALL    NZ,rt_int__neg
        LD      A,(rt_int__sa)
        OR      A
        LD      HL,rt_int__r
        CALL    NZ,rt_int__neg
        OR      A                       ; NC
        RET

; HL -> 32 bits: (DE) = |value|, A = 1 if it was negative
rt_int__abs:
        PUSH    DE
        LD      BC,4
        LDIR
        POP     HL
        INC     HL
        INC     HL
        INC     HL
        LD      A,(HL)
        DEC     HL
        DEC     HL
        DEC     HL
        RLCA
        AND     1
        RET     Z
        CALL    rt_int__neg
        LD      A,1
        RET

; HL -> 32 bits: negated in place
rt_int__neg:
        LD      B,4
        OR      A
rt_int__n1:
        LD      A,0
        SBC     A,(HL)
        LD      (HL),A
        INC     HL
        DJNZ    rt_int__n1
        RET

; HL -> 32 bits: 0
rt_int__zero:
        XOR     A
        LD      (HL),A
        INC     HL
        LD      (HL),A
        INC     HL
        LD      (HL),A
        INC     HL
        LD      (HL),A
        RET

; the low 32 bits of a * b: |a| * |b| through zi_mulv (its result is right modulo 2^32 even on overflow)
rt_imul:
        JP      zi_mulv

; with the error number from rt_argp: outside 32 bits the error, the result 0
rt_iadd_chk:
        LD      (rt_int__dst),HL
        CALL    zi_addv
        JR      rt_int__chk
rt_isub_chk:
        LD      (rt_int__dst),HL
        CALL    zi_subv
        JR      rt_int__chk
rt_imul_chk:
        LD      (rt_int__dst),HL
        CALL    zi_mulv
rt_int__chk:
        RET     NC
        LD      HL,(rt_int__dst)
        CALL    rt_int__zero
        LD      HL,(rt_argp)
        LD      A,(HL)
        JP      rt_error

; HL = result, DE -> value: -32768..32767, else error 4 (result 0)
rt_conv_word:
        PUSH    HL
        EX      DE,HL
        LD      DE,rt_int__a
        LD      BC,4
        LDIR
        POP     HL
        LD      A,(rt_int__a+1)         ; bytes 2, 3 must repeat the sign of byte 1
        RLCA
        SBC     A,A
        LD      C,A
        LD      A,(rt_int__a+2)
        CP      C
        JR      NZ,rt_int__ew
        LD      A,(rt_int__a+3)
        CP      C
        JR      NZ,rt_int__ew
        EX      DE,HL
        LD      HL,rt_int__a
        LD      BC,4
        LDIR
        RET
rt_int__ew:
        CALL    rt_int__zero
        LD      A,4
        JP      rt_error

; HL = result, DE -> value: 0..255, else error 3 (result 0)
rt_conv_byte:
        PUSH    HL
        EX      DE,HL
        LD      DE,rt_int__a
        LD      BC,4
        LDIR
        POP     HL
        LD      A,(rt_int__a+1)
        LD      C,A
        LD      A,(rt_int__a+2)
        OR      C
        LD      C,A
        LD      A,(rt_int__a+3)
        OR      C
        JR      NZ,rt_int__eb
        LD      A,(rt_int__a)
        LD      (HL),A
        INC     HL
        XOR     A
        LD      (HL),A
        INC     HL
        LD      (HL),A
        INC     HL
        LD      (HL),A
        RET
rt_int__eb:
        CALL    rt_int__zero
        LD      A,3
        JP      rt_error

; HL = result, DE -> value: the low 16 bits sign-extended / the low 8 bits
rt_wrap_word:
        EX      DE,HL
        LD      A,(HL)
        LD      (DE),A
        INC     HL
        INC     DE
        LD      A,(HL)
        LD      (DE),A
        INC     DE
        RLCA
        SBC     A,A
        LD      (DE),A
        INC     DE
        LD      (DE),A
        RET
rt_wrap_byte:
        EX      DE,HL
        LD      A,(HL)
        LD      (DE),A
        INC     DE
        XOR     A
        LD      (DE),A
        INC     DE
        LD      (DE),A
        INC     DE
        LD      (DE),A
        RET

;! section bss
rt_int__dst:
        DS      2
rt_int__a:
        DS      4
rt_int__b:
        DS      4
rt_int__q:
        DS      4
rt_int__r:
        DS      4
rt_int__sa:
        DS      1
rt_int__sq:
        DS      1
