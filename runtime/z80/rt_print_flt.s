;! module    rt_print_flt
;! provides  rt_print_flt rt_set_defnum
;! requires  rt_fmt_flt fp_from_i32 rt_print_str rt_defnum rt_print_low
;! group     print
;! clobbers  AF BC DE HL
;! doc       PRINT of a float and DEFNUM (own module: programs that print only strings and integers do not need the float formatting).

; DE -> binary64: as PRINT writes it (rt_defnum digits)
rt_print_flt:
        LD      HL,rt_print_flt__buf
        LD      A,(rt_defnum)
        CALL    rt_fmt_flt
        LD      DE,rt_print_flt__buf
        JP      rt_print_str

; DE -> 32-bit integer, DEFNUM below 13: through the float form (rt_print_int jumps here)
rt_print_flt__int:
        LD      HL,rt_print_flt__fv
        CALL    fp_from_i32
        LD      DE,rt_print_flt__fv
        JP      rt_print_flt

; DE -> n: DEFNUM n (2..13)
rt_set_defnum:
        LD      HL,rt_print_flt__int    ; from now on integers may need the float form
        LD      (rt_print_low),HL
        EX      DE,HL
        LD      A,(HL)
        INC     HL
        LD      C,(HL)
        INC     HL
        LD      B,(HL)
        INC     HL
        LD      D,(HL)
        BIT     7,D                     ; negative: 2
        JR      NZ,rt_print_flt__dlo
        LD      E,A
        LD      A,C
        OR      B
        OR      D
        LD      A,13
        JR      NZ,rt_print_flt__dset   ; large: 13
        LD      A,E
        CP      2
        JR      C,rt_print_flt__dlo
        CP      14
        JR      C,rt_print_flt__dset
        LD      A,13
        JR      rt_print_flt__dset
rt_print_flt__dlo:
        LD      A,2
rt_print_flt__dset:
        LD      (rt_defnum),A
        RET

;! section bss
rt_print_flt__fv:
        DS      8
rt_print_flt__buf:
        DS      32
