;! module    rt_str_int
;! provides  rt_str_int
;! requires  rt_str_dig rt_defnum rt_str_str fp_from_i32
;! group     string
;! clobbers  AF BC DE HL
;! doc       STR$ of an integer: its digits with DEFNUM 13, else as STR$ of the float (rounded to DEFNUM digits).

; DE -> 32-bit integer -> HL = temporary string
rt_str_int:
        LD      A,(rt_defnum)
        CP      13
        JP      NC,rt_str_dig
        LD      HL,rt_str_int__fv
        CALL    fp_from_i32
        LD      DE,rt_str_int__fv
        JP      rt_str_str

;! section bss
rt_str_int__fv:
        DS      8
