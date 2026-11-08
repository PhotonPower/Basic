;! module    rt_power
;! provides  rt_power
;! requires  fp_trunc fp_cmp fp_mul fp_div fp_abs fp_to_i32 rt_error rt_powf
;! group     float
;! clobbers  AF BC DE HL
;! doc       x^y: integer y by repeated squaring, otherwise rt_powf (as platform/pc power): HL = result, DE = x, BC = y.

rt_power:
        LD      (rt_power__dst),HL
        PUSH    BC
        EX      DE,HL
        LD      DE,rt_power__b
        LD      BC,8
        LDIR
        POP     HL
        LD      DE,rt_power__y
        LD      BC,8
        LDIR
        LD      HL,rt_power__t          ; an integer exponent?
        LD      DE,rt_power__y
        CALL    fp_trunc
        LD      HL,rt_power__c
        LD      DE,rt_power__t
        LD      BC,rt_power__y
        CALL    fp_cmp
        LD      A,(rt_power__c)
        OR      A
        JR      Z,rt_power__int
        LD      HL,(rt_power__dst)      ; not an integer: EXP(y*LOG(x))
        LD      DE,rt_power__b
        LD      BC,rt_power__y
        JP      rt_powf
rt_power__int:
        LD      HL,rt_power__a          ; |y| <= 2^31 - 1, else error 1
        LD      DE,rt_power__y
        CALL    fp_abs
        LD      HL,rt_power__c
        LD      DE,rt_power__a
        LD      BC,rt_power__lim
        CALL    fp_cmp
        LD      A,(rt_power__c)
        CP      1
        JR      NZ,rt_power__ok
        LD      HL,(rt_power__dst)
        LD      B,8
rt_power__z:
        LD      (HL),0
        INC     HL
        DJNZ    rt_power__z
        LD      A,1
        JP      rt_error
rt_power__ok:
        LD      HL,rt_power__n          ; n = |y| as 32 bits
        LD      DE,rt_power__a
        CALL    fp_to_i32
        LD      HL,rt_power__r          ; r = 1
        LD      DE,rt_power__one
        EX      DE,HL
        LD      BC,8
        LDIR
rt_power__loop:
        LD      HL,rt_power__n          ; n = 0: done
        LD      A,(HL)
        INC     HL
        OR      (HL)
        INC     HL
        OR      (HL)
        INC     HL
        OR      (HL)
        JR      Z,rt_power__fin
        LD      A,(rt_power__n)         ; n odd: r *= b
        AND     1
        JR      Z,rt_power__sq
        LD      HL,rt_power__r
        LD      DE,rt_power__r
        LD      BC,rt_power__b
        CALL    fp_mul
rt_power__sq:
        LD      HL,rt_power__n+3        ; n >>= 1
        OR      A
        RR      (HL)
        DEC     HL
        RR      (HL)
        DEC     HL
        RR      (HL)
        DEC     HL
        RR      (HL)
        LD      A,(HL)                  ; n > 0: b *= b
        INC     HL
        OR      (HL)
        INC     HL
        OR      (HL)
        INC     HL
        OR      (HL)
        JR      Z,rt_power__fin
        LD      HL,rt_power__b
        LD      DE,rt_power__b
        LD      BC,rt_power__b
        CALL    fp_mul
        JR      rt_power__loop
rt_power__fin:
        LD      A,(rt_power__y+7)       ; a negative exponent: 1 / r
        AND     &80
        JR      NZ,rt_power__inv
        LD      HL,rt_power__r
        LD      DE,(rt_power__dst)
        LD      BC,8
        LDIR
        RET
rt_power__inv:
        LD      HL,(rt_power__dst)
        LD      DE,rt_power__one
        LD      BC,rt_power__r
        JP      fp_div

;! section data
rt_power__one:
        DB      0,0,0,0,0,0,&F0,&3F
rt_power__lim:
        DB      0,0,&C0,&FF,&FF,&FF,&DF,&41     ; 2^31 - 1

;! section bss
rt_power__dst:
        DS      2
rt_power__b:
        DS      8
rt_power__y:
        DS      8
rt_power__t:
        DS      8
rt_power__a:
        DS      8
rt_power__r:
        DS      8
rt_power__c:
        DS      4
rt_power__n:
        DS      4
