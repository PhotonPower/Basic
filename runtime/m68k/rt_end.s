;! module    rt_end
;! provides  rt_end rt_no_return
;! requires  plat_exit rt_print_str rt_print_nl
;! group     core
;! doc       END, STOP, QUIT: end of the program; rt_no_return: a FUNCTION ended without RETURN.

rt_end:
        JMP     plat_exit

rt_no_return:
        LEA     rt_end__msg,A0
        MOVEQ   #rt_end__len,D0
        JSR     rt_print_str
        JSR     rt_print_nl
        JMP     plat_exit

;! section data
rt_end__msg:
        DC.B    'runtime error: FUNCTION ended without RETURN'
rt_end__len EQU 44
