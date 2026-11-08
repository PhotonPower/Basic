;! module    rt_str_n
;! provides  rt_str_n
;! requires  rt_fmt_flt rt_str_newtemp
;! group     strings
;! clobbers  AF BC DE HL
;! doc       STR$(x,n): left-padded to n, or the first n characters (probe4 T33, probe5 T19).

; DE -> x, BC -> n -> HL = temporary
rt_str_n:
        PUSH    BC
        LD      HL,rt_str_n__buf
        LD      A,13
        CALL    rt_fmt_flt              ; BC = len
        LD      (rt_str_n__len),BC
        POP     HL                      ; n: <= 0 gives "", above 32767 is 32767
        INC     HL
        INC     HL
        INC     HL
        LD      BC,0
        BIT     7,(HL)
        JR      NZ,rt_str_n__n
        LD      A,(HL)
        DEC     HL
        OR      (HL)
        DEC     HL
        LD      B,(HL)
        DEC     HL
        LD      C,(HL)
        JR      NZ,rt_str_n__big
        BIT     7,B
        JR      Z,rt_str_n__n
rt_str_n__big:
        LD      BC,32767
rt_str_n__n:
        PUSH    BC
        CALL    rt_str_newtemp          ; HL = descriptor, DE = characters
        POP     BC
        PUSH    HL
        LD      HL,(rt_str_n__len)      ; spaces in front: n - len (if > 0)
        EX      DE,HL                   ; HL = characters, DE = len
        PUSH    HL
        LD      H,B
        LD      L,C
        OR      A
        SBC     HL,DE                   ; n - len
        LD      A,H
        POP     DE                      ; DE = characters
        JR      C,rt_str_n__cut
        JR      Z,rt_str_n__cut
        BIT     7,A
        JR      NZ,rt_str_n__cut
        LD      B,H
        LD      C,L
rt_str_n__sp:
        LD      A,' '
        LD      (DE),A
        INC     DE
        DEC     BC
        LD      A,B
        OR      C
        JR      NZ,rt_str_n__sp
        LD      BC,(rt_str_n__len)
        JR      rt_str_n__cp
rt_str_n__cut:                          ; the first n characters (n <= len)
rt_str_n__cp:
        LD      A,B
        OR      C
        JR      Z,rt_str_n__end
        LD      HL,rt_str_n__buf
        LDIR
rt_str_n__end:
        POP     HL
        RET

;! section bss
rt_str_n__len:
        DS      2
rt_str_n__buf:
        DS      24
