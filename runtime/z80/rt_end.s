;! module    rt_end
;! provides  rt_end rt_no_return
;! requires  plat_exit rt_print_str rt_print_nl
;! group     core
;! clobbers  AF BC DE HL
;! doc       END, STOP, QUIT: end of the program; rt_no_return: a FUNCTION ended without RETURN.

rt_end:
        JP      plat_exit

rt_no_return:
        LD      DE,rt_end__msg
        LD      BC,44
        CALL    rt_print_str
        CALL    rt_print_nl
        JP      plat_exit

;! section data
rt_end__msg:
        DB      "runtime error: FUNCTION ended without RETURN"
