;! module    fp_round
;! provides  fp_round
;! requires  fp_mul fp_div fp_add fp_cmp fp_int fp_abs
;! group     float
;! clobbers  AF BC DE HL
;! doc       ROUND(x,n) = INT(x*10^n+0.5)/10^n (ties upwards, probe6 T11): HL = result, DE -> x, BC -> n.

; For |x * 10^n| >= 2^52 the value has no digits left to round: x unchanged (runtime/ref/fp_round.c).
fp_round:
        LD      (fp_round__dst),HL
        PUSH    BC
        EX      DE,HL
        LD      DE,fp_round__x
        LD      BC,8
        LDIR
        POP     HL
        LD      DE,fp_round__n
        LD      BC,4
        LDIR
        LD      A,(fp_round__x+6)       ; x = 0: 0
        AND     &F0
        LD      C,A
        LD      A,(fp_round__x+7)
        AND     &7F
        OR      C
        JP      Z,fp_round__zero
        LD      HL,(fp_round__n)        ; k = |n|; more than 308: n > 0 -> x, else 0
        LD      DE,(fp_round__n+2)
        BIT     7,D
        JR      Z,fp_round__pk
        XOR     A
        SUB     L
        LD      L,A
        LD      A,0
        SBC     A,H
        LD      H,A
        LD      A,0
        SBC     A,E
        LD      E,A
        LD      A,0
        SBC     A,D
        LD      D,A
fp_round__pk:
        LD      A,D
        OR      E
        JR      NZ,fp_round__big
        PUSH    HL
        LD      BC,309
        OR      A
        SBC     HL,BC
        POP     HL
        JR      C,fp_round__ok
fp_round__big:
        LD      A,(fp_round__n+3)
        BIT     7,A
        JP      NZ,fp_round__zero
        JP      fp_round__retx
fp_round__ok:
        LD      (fp_round__k),HL
        LD      HL,fp_round__p          ; p = 10^k
        LD      DE,fp_round__p+1
        LD      (HL),0
        LD      BC,7
        LDIR
        LD      A,&F0
        LD      (fp_round__p+6),A
        LD      A,&3F
        LD      (fp_round__p+7),A
fp_round__pw:
        LD      HL,(fp_round__k)
        LD      A,H
        OR      L
        JR      Z,fp_round__pd
        DEC     HL
        LD      (fp_round__k),HL
        LD      HL,fp_round__p
        LD      DE,fp_round__p
        LD      BC,fp_round__ten
        CALL    fp_mul
        JR      fp_round__pw
fp_round__pd:
        LD      HL,fp_round__a          ; a = |x|
        LD      DE,fp_round__x
        CALL    fp_abs
        LD      A,(fp_round__n+3)
        BIT     7,A
        JR      NZ,fp_round__neg
        LD      HL,fp_round__y          ; n >= 0: |x| >= 2^52 / p -> x; y = x * p
        LD      DE,fp_round__2p52
        LD      BC,fp_round__p
        CALL    fp_div
        LD      HL,fp_round__c
        LD      DE,fp_round__a
        LD      BC,fp_round__y
        CALL    fp_cmp
        LD      A,(fp_round__c)
        BIT     7,A
        JR      Z,fp_round__retx
        LD      HL,fp_round__y
        LD      DE,fp_round__x
        LD      BC,fp_round__p
        CALL    fp_mul
        JR      fp_round__r
fp_round__neg:
        LD      HL,fp_round__y          ; n < 0: y = x / p; |y| >= 2^52 -> x
        LD      DE,fp_round__x
        LD      BC,fp_round__p
        CALL    fp_div
        LD      HL,fp_round__a
        LD      DE,fp_round__y
        CALL    fp_abs
        LD      HL,fp_round__c
        LD      DE,fp_round__a
        LD      BC,fp_round__2p52
        CALL    fp_cmp
        LD      A,(fp_round__c)
        BIT     7,A
        JR      Z,fp_round__retx
fp_round__r:
        LD      HL,fp_round__y          ; y = INT(y + 0.5)
        LD      DE,fp_round__y
        LD      BC,fp_round__half
        CALL    fp_add
        LD      HL,fp_round__y
        LD      DE,fp_round__y
        CALL    fp_int
        LD      A,(fp_round__n+3)
        BIT     7,A
        LD      HL,(fp_round__dst)
        LD      DE,fp_round__y
        LD      BC,fp_round__p
        JP      Z,fp_div
        JP      fp_mul
fp_round__retx:
        LD      HL,fp_round__x
        LD      DE,(fp_round__dst)
        LD      BC,8
        LDIR
        RET
fp_round__zero:
        LD      HL,(fp_round__dst)
        LD      B,8
fp_round__z:
        LD      (HL),0
        INC     HL
        DJNZ    fp_round__z
        RET

;! section data
fp_round__ten:
        DB      0,0,0,0,0,0,&24,&40     ; 10
fp_round__half:
        DB      0,0,0,0,0,0,&E0,&3F     ; 0.5
fp_round__2p52:
        DB      0,0,0,0,0,0,&30,&43     ; 2^52

;! section bss
fp_round__dst:
        DS      2
fp_round__x:
        DS      8
fp_round__n:
        DS      4
fp_round__k:
        DS      2
fp_round__p:
        DS      8
fp_round__a:
        DS      8
fp_round__y:
        DS      8
fp_round__c:
        DS      4
