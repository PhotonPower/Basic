;! module    rt_strfn
;! provides  rt_str_concat rt_str_cmp rt_str_len rt_str_asc rt_str_chr rt_str_left rt_str_right rt_str_mid rt_str_space
;! provides  rt_str_string rt_str_instr rt_str_upper rt_str_trim rt_str_str
;! requires  rt_str_newtemp rt_error rt_fmt_flt rt_defnum rt_argp
;! group     string
;! clobbers  AF BC DE HL
;! doc       String functions (as runtime/m68k): strings as descriptor addresses, numbers as pointers (D30).

; String results: HL = temporary descriptor. Number results: HL = pointer to the 32-bit result.
; A new temporary may move other blocks (compaction): source characters are read after it.

; HL -> 32-bit value: A = 0 and HL = value if 0..32767; A = 1 if negative; A = 2 if larger
rt_strfn__int:
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        INC     HL
        LD      A,(HL)
        INC     HL
        LD      H,(HL)
        LD      L,A
        BIT     7,H
        LD      A,1
        RET     NZ
        LD      A,H
        OR      L
        LD      A,2
        RET     NZ
        BIT     7,D
        RET     NZ
        EX      DE,HL
        XOR     A
        RET

; HL = descriptor -> BC = its length, DE = its characters
rt_strfn__get:
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        INC     HL
        LD      C,(HL)
        INC     HL
        LD      B,(HL)
        RET

; error 10, the result "" (also negative lengths, probe4 T45)
rt_strfn__err10:
        LD      A,10
        CALL    rt_error
        LD      BC,0
        JP      rt_str_newtemp

; rt_strfn__src = descriptor, BC = offset (0-based), rt_strfn__cnt = count -> HL = temporary with the part
rt_strfn__sub:
        LD      (rt_strfn__off),BC
        LD      BC,(rt_strfn__cnt)
        CALL    rt_str_newtemp          ; HL = descriptor, DE = characters
        PUSH    HL
        LD      BC,(rt_strfn__cnt)
        LD      A,B
        OR      C
        JR      Z,rt_strfn__s9
        LD      HL,(rt_strfn__src)
        LD      A,(HL)
        INC     HL
        LD      H,(HL)
        LD      L,A
        PUSH    BC
        LD      BC,(rt_strfn__off)
        ADD     HL,BC
        POP     BC
        LDIR
rt_strfn__s9:
        POP     HL
        RET

; DE = a, BC = b -> HL = a + b (error 10 above 32767 characters)
rt_str_concat:
        LD      (rt_strfn__src),DE
        LD      (rt_strfn__src2),BC
        LD      H,B
        LD      L,C
        INC     HL
        INC     HL
        LD      C,(HL)
        INC     HL
        LD      B,(HL)                  ; BC = len(b)
        EX      DE,HL
        INC     HL
        INC     HL
        LD      E,(HL)
        INC     HL
        LD      D,(HL)                  ; DE = len(a)
        LD      (rt_strfn__cnt),DE
        EX      DE,HL
        ADD     HL,BC
        JP      C,rt_strfn__err10
        BIT     7,H
        JP      NZ,rt_strfn__err10
        LD      B,H
        LD      C,L
        CALL    rt_str_newtemp
        PUSH    HL
        PUSH    DE
        LD      HL,(rt_strfn__src)      ; a
        CALL    rt_strfn__get
        EX      DE,HL                   ; HL = characters of a, BC = length
        POP     DE
        LD      A,B
        OR      C
        JR      Z,rt_strfn__c1
        LDIR
rt_strfn__c1:
        PUSH    DE
        LD      HL,(rt_strfn__src2)     ; b
        CALL    rt_strfn__get
        EX      DE,HL
        POP     DE
        LD      A,B
        OR      C
        JR      Z,rt_strfn__c2
        LDIR
rt_strfn__c2:
        POP     HL
        RET

; HL = result, DE = a, BC = b -> -1, 0, 1 (bytes unsigned; a prefix is smaller)
rt_str_cmp:
        PUSH    HL
        PUSH    BC
        EX      DE,HL
        CALL    rt_strfn__get           ; a: DE chars, BC length
        LD      (rt_strfn__off),BC
        POP     HL
        PUSH    DE
        CALL    rt_strfn__get           ; b: DE chars, BC length
        POP     HL                      ; HL = a chars, DE = b chars
rt_strfn__cl:
        PUSH    HL
        LD      HL,(rt_strfn__off)
        LD      A,H
        OR      L
        POP     HL
        JR      Z,rt_strfn__aend
        LD      A,B
        OR      C
        JR      Z,rt_strfn__gt
        LD      A,(DE)
        PUSH    BC
        LD      C,A
        LD      A,(HL)
        CP      C
        POP     BC
        JR      C,rt_strfn__lt
        JR      NZ,rt_strfn__gt
        INC     HL
        INC     DE
        DEC     BC
        PUSH    HL
        LD      HL,(rt_strfn__off)
        DEC     HL
        LD      (rt_strfn__off),HL
        POP     HL
        JR      rt_strfn__cl
rt_strfn__aend:
        LD      A,B
        OR      C
        JR      Z,rt_strfn__eq
rt_strfn__lt:
        LD      A,-1
        JR      rt_strfn__put
rt_strfn__gt:
        LD      A,1
        JR      rt_strfn__put
rt_strfn__eq:
        XOR     A
rt_strfn__put:                          ; A = -1, 0, 1 -> 32 bits at the result
        POP     HL
        LD      (HL),A
        INC     HL
        RLCA
        SBC     A,A
        LD      (HL),A
        INC     HL
        LD      (HL),A
        INC     HL
        LD      (HL),A
        RET

; HL = result, DE = s: LEN
rt_str_len:
        PUSH    HL
        EX      DE,HL
        CALL    rt_strfn__get
        POP     HL
rt_strfn__w16:                          ; BC as 32 bits at HL
        LD      (HL),C
        INC     HL
        LD      (HL),B
        INC     HL
        LD      (HL),0
        INC     HL
        LD      (HL),0
        RET

; HL = result, DE = s: ASC (0 for "")
rt_str_asc:
        PUSH    HL
        EX      DE,HL
        CALL    rt_strfn__get
        POP     HL
        LD      A,B
        OR      C
        LD      BC,0
        JR      Z,rt_strfn__w16
        LD      A,(DE)
        LD      C,A
        JR      rt_strfn__w16

; DE -> code: CHR$(code AND 255)
rt_str_chr:
        LD      A,(DE)
        LD      (rt_strfn__ch),A
        LD      BC,1
        CALL    rt_str_newtemp
        LD      A,(rt_strfn__ch)
        LD      (DE),A
        RET

; DE = s, BC -> n: LEFT$
rt_str_left:
        CALL    rt_strfn__lr
        RET     C
        LD      BC,0
        JP      rt_strfn__sub

; DE = s, BC -> n: RIGHT$
rt_str_right:
        CALL    rt_strfn__lr
        RET     C
        LD      HL,(rt_strfn__len)      ; offset = length - n
        LD      BC,(rt_strfn__cnt)
        OR      A
        SBC     HL,BC
        LD      B,H
        LD      C,L
        JP      rt_strfn__sub

; common part of LEFT$/RIGHT$: rt_strfn__cnt = min(n, length); CF set with HL = "" after error 10
rt_strfn__lr:
        LD      (rt_strfn__src),DE
        LD      H,B
        LD      L,C
        CALL    rt_strfn__int
        CP      1
        JR      NZ,rt_strfn__lr1
        CALL    rt_strfn__err10
        SCF
        RET
rt_strfn__lr1:
        LD      (rt_strfn__cnt),HL
        PUSH    AF
        LD      HL,(rt_strfn__src)
        CALL    rt_strfn__get
        LD      (rt_strfn__len),BC
        POP     AF
        CP      2
        JR      Z,rt_strfn__lr2         ; very large: the whole length
        LD      HL,(rt_strfn__cnt)
        OR      A
        SBC     HL,BC
        JR      C,rt_strfn__lr3
rt_strfn__lr2:
        LD      (rt_strfn__cnt),BC
rt_strfn__lr3:
        OR      A
        RET

; DE = s, BC -> from (1-based, 0 counts as 1), rt_argp -> count: MID$
rt_str_mid:
        LD      (rt_strfn__src),DE
        LD      H,B
        LD      L,C
        CALL    rt_strfn__int
        CP      1
        JP      Z,rt_strfn__err10
        LD      (rt_strfn__off),HL
        LD      (rt_strfn__fa),A
        LD      HL,(rt_argp)
        CALL    rt_strfn__int
        CP      1
        JP      Z,rt_strfn__err10
        CP      2
        JR      NZ,rt_strfn__m1
        LD      HL,32767
rt_strfn__m1:
        LD      (rt_strfn__cnt),HL
        LD      HL,(rt_strfn__src)
        CALL    rt_strfn__get           ; BC = length
        LD      A,(rt_strfn__fa)
        CP      2
        JR      Z,rt_strfn__mnone       ; from very large
        LD      HL,(rt_strfn__off)
        LD      A,H
        OR      L
        JR      NZ,rt_strfn__m2
        INC     HL                      ; 0 counts as 1
rt_strfn__m2:
        PUSH    HL                      ; from > length: ""
        OR      A
        SBC     HL,BC
        POP     HL
        JR      Z,rt_strfn__m3
        JR      NC,rt_strfn__mnone
rt_strfn__m3:
        DEC     HL                      ; offset
        LD      (rt_strfn__off),HL
        PUSH    HL                      ; characters left = length - offset
        LD      H,B
        LD      L,C
        POP     BC
        OR      A
        SBC     HL,BC
        LD      DE,(rt_strfn__cnt)      ; count = min(count, left)
        PUSH    HL
        OR      A
        SBC     HL,DE
        POP     HL
        JR      NC,rt_strfn__m4
        LD      (rt_strfn__cnt),HL
rt_strfn__m4:
        LD      BC,(rt_strfn__off)
        JP      rt_strfn__sub
rt_strfn__mnone:
        LD      HL,0
        LD      (rt_strfn__cnt),HL
        LD      BC,0
        JP      rt_strfn__sub

; DE -> n: SPACE$
rt_str_space:
        EX      DE,HL
        CALL    rt_strfn__int
        OR      A
        JP      NZ,rt_strfn__err10
        LD      B,H
        LD      C,L
        PUSH    BC
        CALL    rt_str_newtemp
        POP     BC
        LD      A,B
        OR      C
        RET     Z
        PUSH    HL
        EX      DE,HL
rt_strfn__sp:
        LD      (HL),' '
        INC     HL
        DEC     BC
        LD      A,B
        OR      C
        JR      NZ,rt_strfn__sp
        POP     HL
        RET

; DE -> n, BC = unit: STRING$(n, unit$)
rt_str_string:
        LD      (rt_strfn__src),BC
        EX      DE,HL
        CALL    rt_strfn__int
        OR      A
        JP      NZ,rt_strfn__err10
        LD      (rt_strfn__cnt),HL      ; n
        LD      HL,(rt_strfn__src)
        CALL    rt_strfn__get
        LD      (rt_strfn__len),BC      ; unit length; n * length <= 32767
        LD      HL,0
        LD      DE,(rt_strfn__cnt)
rt_strfn__sm:
        LD      A,D
        OR      E
        JR      Z,rt_strfn__sm2
        ADD     HL,BC
        JP      C,rt_strfn__err10
        BIT     7,H
        JP      NZ,rt_strfn__err10
        DEC     DE
        JR      rt_strfn__sm
rt_strfn__sm2:
        LD      B,H
        LD      C,L
        CALL    rt_str_newtemp
        PUSH    HL
rt_strfn__sr:
        LD      HL,(rt_strfn__cnt)
        LD      A,H
        OR      L
        JR      Z,rt_strfn__s9b
        DEC     HL
        LD      (rt_strfn__cnt),HL
        PUSH    DE
        LD      HL,(rt_strfn__src)
        CALL    rt_strfn__get
        EX      DE,HL
        POP     DE
        LD      A,B
        OR      C
        JR      Z,rt_strfn__sr
        LDIR
        JR      rt_strfn__sr
rt_strfn__s9b:
        POP     HL
        RET

; HL = result, DE = a, BC = b, rt_argp -> from: INSTR (0: not found; empty b as D12)
rt_str_instr:
        LD      (rt_strfn__res),HL
        LD      (rt_strfn__src),DE
        LD      (rt_strfn__src2),BC
        LD      HL,(rt_argp)
        CALL    rt_strfn__int
        CP      1
        JR      NZ,rt_strfn__i0
        LD      HL,(rt_strfn__res)
        LD      BC,0
        CALL    rt_strfn__w16
        LD      A,10
        JP      rt_error
rt_strfn__i0:
        CP      2
        JR      NZ,rt_strfn__i1
        LD      HL,32767
rt_strfn__i1:
        LD      A,H
        OR      L
        JR      NZ,rt_strfn__i2
        INC     HL                      ; 0 counts as 1
rt_strfn__i2:
        LD      (rt_strfn__off),HL      ; from
        LD      HL,(rt_strfn__src)
        CALL    rt_strfn__get
        LD      (rt_strfn__len),BC      ; len(a)
        LD      (rt_strfn__pa),DE
        LD      HL,(rt_strfn__src2)
        CALL    rt_strfn__get
        LD      (rt_strfn__cnt),BC      ; len(b)
        LD      (rt_strfn__pb),DE
        LD      A,B
        OR      C
        JR      NZ,rt_strfn__isrch
        LD      HL,(rt_strfn__len)      ; empty b: from if from <= len(a)+1 (1 for empty a), else 0
        INC     HL
        LD      DE,(rt_strfn__off)
        OR      A
        SBC     HL,DE
        JR      C,rt_strfn__inone
        LD      BC,(rt_strfn__off)
        LD      HL,(rt_strfn__len)
        LD      A,H
        OR      L
        JR      NZ,rt_strfn__iout
        LD      BC,1
        JR      rt_strfn__iout
rt_strfn__isrch:
        LD      HL,(rt_strfn__len)      ; from > len(a): 0
        LD      DE,(rt_strfn__off)
        OR      A
        SBC     HL,DE
        JR      C,rt_strfn__inone
rt_strfn__itry:                         ; enough left: len(a) - pos + 1 >= len(b)
        LD      HL,(rt_strfn__len)
        LD      DE,(rt_strfn__off)
        OR      A
        SBC     HL,DE
        INC     HL
        LD      DE,(rt_strfn__cnt)
        OR      A
        SBC     HL,DE
        JR      C,rt_strfn__inone
        LD      HL,(rt_strfn__pa)       ; compare at a + pos - 1
        LD      DE,(rt_strfn__off)
        ADD     HL,DE
        DEC     HL
        LD      DE,(rt_strfn__pb)
        LD      BC,(rt_strfn__cnt)
rt_strfn__ic:
        LD      A,(DE)
        CP      (HL)
        JR      NZ,rt_strfn__ino
        INC     HL
        INC     DE
        DEC     BC
        LD      A,B
        OR      C
        JR      NZ,rt_strfn__ic
        LD      BC,(rt_strfn__off)
        JR      rt_strfn__iout
rt_strfn__ino:
        LD      HL,(rt_strfn__off)
        INC     HL
        LD      (rt_strfn__off),HL
        JR      rt_strfn__itry
rt_strfn__inone:
        LD      BC,0
rt_strfn__iout:
        LD      HL,(rt_strfn__res)
        JP      rt_strfn__w16

; DE = s: UPPER$ (a-z and the Latin-1 letters &E0-&FE except &F7)
rt_str_upper:
        LD      (rt_strfn__src),DE
        EX      DE,HL
        CALL    rt_strfn__get
        LD      (rt_strfn__cnt),BC
        LD      BC,0
        CALL    rt_strfn__sub
        PUSH    HL
        CALL    rt_strfn__get           ; the copy: DE chars, BC length
        EX      DE,HL
rt_strfn__u1:
        LD      A,B
        OR      C
        JR      Z,rt_strfn__u9
        LD      A,(HL)
        CP      'a'
        JR      C,rt_strfn__u2
        CP      'z'+1
        JR      C,rt_strfn__up
        CP      &E0
        JR      C,rt_strfn__u2
        CP      &F7
        JR      Z,rt_strfn__u2
        CP      &FF
        JR      Z,rt_strfn__u2
rt_strfn__up:
        SUB     32
        LD      (HL),A
rt_strfn__u2:
        INC     HL
        DEC     BC
        JR      rt_strfn__u1
rt_strfn__u9:
        POP     HL
        RET

; DE = s: TRIM$ (spaces at both ends removed)
rt_str_trim:
        LD      (rt_strfn__src),DE
        EX      DE,HL
        CALL    rt_strfn__get           ; DE chars, BC length
        LD      HL,0                    ; HL = offset of the first non-space
rt_strfn__t1:
        LD      A,B
        OR      C
        JR      Z,rt_strfn__t3
        LD      A,(DE)
        CP      ' '
        JR      NZ,rt_strfn__t2
        INC     HL
        INC     DE
        DEC     BC
        JR      rt_strfn__t1
rt_strfn__t2:                           ; BC = rest: drop spaces at the end
        PUSH    HL
        LD      H,D
        LD      L,E
        ADD     HL,BC
rt_strfn__t4:
        DEC     HL
        LD      A,(HL)
        CP      ' '
        JR      NZ,rt_strfn__t5
        DEC     BC
        JR      rt_strfn__t4
rt_strfn__t5:
        POP     HL
rt_strfn__t3:
        LD      (rt_strfn__cnt),BC
        LD      B,H
        LD      C,L
        JP      rt_strfn__sub

; DE -> binary64: STR$ with DEFNUM digits
rt_str_str:
        LD      HL,rt_strfn__buf
        LD      A,(rt_defnum)
        CALL    rt_fmt_flt              ; BC = length
        PUSH    BC
        CALL    rt_str_newtemp
        POP     BC
        LD      A,B
        OR      C
        RET     Z
        PUSH    HL
        LD      HL,rt_strfn__buf
        LDIR
        POP     HL
        RET

;! section bss
rt_strfn__src:
        DS      2
rt_strfn__src2:
        DS      2
rt_strfn__off:
        DS      2
rt_strfn__cnt:
        DS      2
rt_strfn__len:
        DS      2
rt_strfn__res:
        DS      2
rt_strfn__pa:
        DS      2
rt_strfn__pb:
        DS      2
rt_strfn__fa:
        DS      1
rt_strfn__ch:
        DS      1
rt_strfn__buf:
        DS      32
