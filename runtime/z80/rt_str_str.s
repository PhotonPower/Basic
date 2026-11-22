;! module    rt_str_str
;! provides  rt_str_str
;! requires  rt_fmt_flt rt_defnum rt_str_newtemp
;! group     string
;! clobbers  AF BC DE HL
;! doc       STR$ of a float with DEFNUM digits (own module: STR$ of integers and the other string functions do not need the float formatting).

; DE -> binary64: STR$ with DEFNUM digits
rt_str_str:
        LD      HL,rt_str_str__buf
        LD      A,(rt_defnum)
        CALL    rt_fmt_flt              ; BC = length
        PUSH    BC
        CALL    rt_str_newtemp
        POP     BC
        LD      A,B
        OR      C
        RET     Z
        PUSH    HL
        LD      HL,rt_str_str__buf
        LDIR
        POP     HL
        RET

;! section bss
rt_str_str__buf:
        DS      32
