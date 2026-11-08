;! module    rt_rinstr
;! provides  rt_str_rinstr
;! requires  rt_argp
;! group     string
;! clobbers  AF BC DE HL
;! doc       RINSTR (probe20 T6): the last b$ that ends before position x, i.e. in LEFT$(a$,x-1).

; HL = result, DE = a, BC = b, rt_argp -> x (LEN(a$)+1 if not given) -> position (0: none; an
; empty a$: 0)
rt_str_rinstr:
        PUSH    HL
        EX      DE,HL                   ; a: characters, n
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        INC     HL
        LD      (rt_rinstr__a),DE
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        LD      (rt_rinstr__n),DE
        LD      A,D
        OR      E
        JR      Z,rt_rinstr__none
        LD      H,B                     ; b: characters, m
        LD      L,C
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        INC     HL
        LD      (rt_rinstr__b),DE
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        LD      (rt_rinstr__m),DE
        LD      HL,(rt_argp)            ; x limited to ..32767 (negative: none)
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        INC     HL
        LD      A,(HL)
        INC     HL
        LD      H,(HL)
        BIT     7,H
        JR      NZ,rt_rinstr__none
        OR      H
        JR      NZ,rt_rinstr__big
        BIT     7,D
        JR      Z,rt_rinstr__x
rt_rinstr__big:
        LD      DE,32767
rt_rinstr__x:
        EX      DE,HL                   ; s = min(x - m, n - m + 1)
        LD      DE,(rt_rinstr__m)
        OR      A
        SBC     HL,DE
        PUSH    HL
        LD      HL,(rt_rinstr__n)
        SBC     HL,DE
        INC     HL
        POP     DE                      ; DE = x - m, HL = n - m + 1
        PUSH    HL
        OR      A
        SBC     HL,DE                   ; signed: n-m+1 - (x-m) < 0 -> take n-m+1
        POP     HL
        JP      PE,rt_rinstr__ov
        JP      M,rt_rinstr__s
        JR      rt_rinstr__dx
rt_rinstr__ov:
        JP      P,rt_rinstr__s
rt_rinstr__dx:
        EX      DE,HL
rt_rinstr__s:                           ; HL = s
        BIT     7,H
        JR      NZ,rt_rinstr__none
        LD      A,H
        OR      L
        JR      Z,rt_rinstr__none
        PUSH    HL                      ; compare m characters at a + s - 1
        LD      DE,(rt_rinstr__a)
        ADD     HL,DE
        DEC     HL
        LD      DE,(rt_rinstr__b)
        LD      BC,(rt_rinstr__m)
rt_rinstr__c:
        LD      A,B
        OR      C
        JR      Z,rt_rinstr__hit
        LD      A,(DE)
        CP      (HL)
        JR      NZ,rt_rinstr__no
        INC     HL
        INC     DE
        DEC     BC
        JR      rt_rinstr__c
rt_rinstr__no:
        POP     HL
        DEC     HL
        JR      rt_rinstr__s
rt_rinstr__hit:
        POP     DE
        JR      rt_rinstr__out
rt_rinstr__none:
        LD      DE,0
rt_rinstr__out:
        POP     HL
        LD      (HL),E
        INC     HL
        LD      (HL),D
        INC     HL
        LD      (HL),0
        INC     HL
        LD      (HL),0
        RET

;! section bss
rt_rinstr__a:
        DS      2
rt_rinstr__n:
        DS      2
rt_rinstr__b:
        DS      2
rt_rinstr__m:
        DS      2
