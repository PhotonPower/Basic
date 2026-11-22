;! module    rt_str_dig
;! provides  rt_str_dig
;! requires  rt_fmt_int rt_str_newtemp
;! group     string
;! clobbers  AF BC DE HL
;! doc       STR$ of an integer as its digits (the program has no DEFNUM; rt_str_int decides at run time and needs the float formatting).

; DE -> 32-bit integer -> HL = temporary string
rt_str_dig:
        LD      HL,rt_str_dig__buf
        CALL    rt_fmt_int              ; BC = length (at least 1)
        PUSH    BC
        CALL    rt_str_newtemp          ; HL = descriptor, DE = characters
        POP     BC
        PUSH    HL
        LD      HL,rt_str_dig__buf
        LDIR
        POP     HL
        RET

;! section bss
rt_str_dig__buf:
        DS      11
