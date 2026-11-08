;! module    rt_str_int
;! provides  rt_str_int
;! requires  rt_fmt_int rt_defnum rt_str_str rt_str_newtemp fp_from_i32
;! group     string
;! clobbers  AF BC DE HL
;! doc       STR$ of an integer: its digits with DEFNUM 13, else as STR$ of the float (rounded to DEFNUM digits).

; DE -> 32-bit integer -> HL = temporary string
rt_str_int:
        LD      A,(rt_defnum)
        CP      13
        JR      NC,rt_str_int__dig
        LD      HL,rt_str_int__fv
        CALL    fp_from_i32
        LD      DE,rt_str_int__fv
        JP      rt_str_str
rt_str_int__dig:
        LD      HL,rt_str_int__buf
        CALL    rt_fmt_int              ; BC = length (at least 1)
        PUSH    BC
        CALL    rt_str_newtemp          ; HL = descriptor, DE = characters
        POP     BC
        PUSH    HL
        LD      HL,rt_str_int__buf
        LDIR
        POP     HL
        RET

;! section bss
rt_str_int__fv:
        DS      8
rt_str_int__buf:
        DS      11
