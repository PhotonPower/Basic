;! module    rt_str_set
;! provides  rt_str_lset rt_str_rset rt_str_midset
;! requires  rt_argp
;! group     strings
;! clobbers  AF BC DE HL
;! doc       LSET, RSET and MID$(a$,from,count)=b$: overwrite a$ in place, its length stays.

; DE = target descriptor, BC = source descriptor: rt_str_set__t/tl = characters and length of the
; target, HL = characters of the source, BC = characters taken from it (at most the target length)
rt_str_set__get:
        EX      DE,HL
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        INC     HL
        LD      (rt_str_set__t),DE
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        LD      (rt_str_set__tl),DE
        LD      H,B
        LD      L,C
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        INC     HL
        LD      C,(HL)
        INC     HL
        LD      B,(HL)
        EX      DE,HL                   ; HL = source characters, BC = source length
        PUSH    HL
        LD      HL,(rt_str_set__tl)
        OR      A
        SBC     HL,BC
        POP     HL
        RET     NC
        LD      BC,(rt_str_set__tl)
        RET

; HL = count: that many spaces at DE
rt_str_set__sp:
        LD      A,H
        OR      L
        RET     Z
        LD      A,' '
        LD      (DE),A
        INC     DE
        DEC     HL
        JR      rt_str_set__sp

; LSET a$=b$: b$ cut or filled with spaces at the end
rt_str_lset:
        CALL    rt_str_set__get
        LD      DE,(rt_str_set__t)
        PUSH    BC
        LD      A,B
        OR      C
        JR      Z,rt_str_set__l1
        LDIR
rt_str_set__l1:
        POP     BC
        LD      HL,(rt_str_set__tl)
        OR      A
        SBC     HL,BC
        JR      rt_str_set__sp

; RSET a$=b$: spaces in front
rt_str_rset:
        CALL    rt_str_set__get
        PUSH    HL
        PUSH    BC
        LD      HL,(rt_str_set__tl)
        OR      A
        SBC     HL,BC
        LD      DE,(rt_str_set__t)
        CALL    rt_str_set__sp
        POP     BC
        POP     HL
        LD      A,B
        OR      C
        RET     Z
        LDIR
        RET

; rt_argp -> from (< 1 counts as 1), -> count: MID$(a$,from,count)=b$ (probe4 T46)
rt_str_midset:
        CALL    rt_str_set__get         ; BC = min(len b$, len a$)
        LD      (rt_str_set__s),HL
        LD      (rt_str_set__n),BC
        LD      HL,(rt_argp)            ; from: 1..len a$, else nothing to do (< 1 is 1)
        CALL    rt_str_set__i16         ; HL = from (clamped to 1..32767)
        LD      A,H
        OR      L
        JR      NZ,rt_str_set__m1
        INC     HL
rt_str_set__m1:
        EX      DE,HL                   ; DE = from
        LD      HL,(rt_str_set__tl)     ; left = len a$ - from + 1
        OR      A
        SBC     HL,DE
        RET     C                       ; from > len + 1
        INC     HL
        LD      A,H
        OR      L
        RET     Z
        DEC     DE
        PUSH    DE                      ; offset = from - 1
        LD      BC,(rt_str_set__n)      ; n = min(left, count, len b$)
        CALL    rt_str_set__min
        PUSH    HL
        LD      HL,(rt_argp+2)
        CALL    rt_str_set__i16
        LD      B,H
        LD      C,L
        POP     HL
        CALL    rt_str_set__min         ; HL = n
        LD      B,H
        LD      C,L
        POP     DE
        LD      HL,(rt_str_set__t)
        ADD     HL,DE
        EX      DE,HL
        LD      HL,(rt_str_set__s)
        LD      A,B
        OR      C
        RET     Z
        LDIR
        RET

; HL, BC (unsigned) -> HL = the smaller
rt_str_set__min:
        PUSH    HL
        OR      A
        SBC     HL,BC
        POP     HL
        RET     C
        LD      H,B
        LD      L,C
        RET

; HL -> 32-bit value -> HL = 0 if negative or 0, 32767 if larger, else the value
rt_str_set__i16:
        INC     HL
        INC     HL
        INC     HL
        BIT     7,(HL)
        JR      NZ,rt_str_set__i0
        LD      A,(HL)
        DEC     HL
        OR      (HL)
        DEC     HL
        LD      B,(HL)
        DEC     HL
        LD      L,(HL)
        LD      H,B
        JR      NZ,rt_str_set__ib
        BIT     7,H
        RET     Z
rt_str_set__ib:
        LD      HL,32767
        RET
rt_str_set__i0:
        LD      HL,0
        RET

;! section bss
rt_str_set__t:
        DS      2
rt_str_set__tl:
        DS      2
rt_str_set__s:
        DS      2
rt_str_set__n:
        DS      2
