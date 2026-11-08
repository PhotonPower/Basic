;! module    rt_for
;! provides  rt_for_wrap_int rt_for_wrap_word rt_for_wrap_byte rt_for_wrap_bool
;! requires  fp_trunc fp_cmp fp_add fp_sub fp_to_i32
;! group     core
;! clobbers  AF BC DE HL
;! doc       FOR with an integer variable: the new value (DE -> float) wraps like INC (probe2 T13) -> HL.

rt_for_wrap_int:
        JP      rt_for__wrap
rt_for_wrap_word:
        CALL    rt_for__wrap            ; the low 16 bits sign-extended
        LD      HL,(rt_for__dst)
        INC     HL
        LD      A,(HL)
        RLCA
        SBC     A,A
        INC     HL
        LD      (HL),A
        INC     HL
        LD      (HL),A
        RET
rt_for_wrap_byte:
        CALL    rt_for__wrap            ; the low 8 bits
        LD      HL,(rt_for__dst)
        INC     HL
        XOR     A
        LD      (HL),A
        INC     HL
        LD      (HL),A
        INC     HL
        LD      (HL),A
        RET
rt_for_wrap_bool:
        CALL    rt_for__wrap            ; not 0: -1
        LD      HL,(rt_for__dst)
        LD      A,(HL)
        INC     HL
        OR      (HL)
        INC     HL
        OR      (HL)
        INC     HL
        OR      (HL)
        DEC     HL
        DEC     HL
        DEC     HL
        JR      Z,rt_for__b
        LD      A,&FF
rt_for__b:
        LD      (HL),A
        INC     HL
        LD      (HL),A
        INC     HL
        LD      (HL),A
        INC     HL
        LD      (HL),A
        RET

; truncate, bring into -2^31 .. 2^31-1 by +/- 2^32 (|x| < 2^32), convert
rt_for__wrap:
        LD      (rt_for__dst),HL
        LD      HL,rt_for__t
        CALL    fp_trunc                ; t = TRUNC(x)
        LD      HL,rt_for__c
        LD      DE,rt_for__t
        LD      BC,rt_for__p31
        CALL    fp_cmp
        LD      A,(rt_for__c)
        BIT     7,A
        JR      NZ,rt_for__low
        LD      HL,rt_for__t            ; >= 2^31: - 2^32
        LD      DE,rt_for__t
        LD      BC,rt_for__p32
        CALL    fp_sub
        JR      rt_for__conv
rt_for__low:
        LD      HL,rt_for__c            ; < -2^31: + 2^32
        LD      DE,rt_for__t
        LD      BC,rt_for__m31
        CALL    fp_cmp
        LD      A,(rt_for__c)
        BIT     7,A
        JR      Z,rt_for__conv
        LD      HL,rt_for__t
        LD      DE,rt_for__t
        LD      BC,rt_for__p32
        CALL    fp_add
rt_for__conv:
        LD      HL,(rt_for__dst)
        LD      DE,rt_for__t
        JP      fp_to_i32

;! section data
rt_for__p31:
        DB      0,0,0,0,0,0,&E0,&41     ; 2^31
rt_for__m31:
        DB      0,0,0,0,0,0,&E0,&C1     ; -2^31
rt_for__p32:
        DB      0,0,0,0,0,0,&F0,&41     ; 2^32

;! section bss
rt_for__dst:
        DS      2
rt_for__t:
        DS      8
rt_for__c:
        DS      4
