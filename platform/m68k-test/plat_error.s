;! module    plat_error
;! provides  plat_error
;! requires  rt_print_str rt_print_int rt_print_nl plat_exit
;! group     core
;! doc       m68k-test: an unhandled runtime error (D0) prints "runtime error <n>" and ends the program.

plat_error:
        MOVE.L  D0,-(SP)
        JSR     rt_print_nl
        LEA     plat_error__msg,A0
        MOVEQ   #14,D0
        JSR     rt_print_str
        MOVE.L  (SP)+,D0
        JSR     rt_print_int
        JSR     rt_print_nl
        JMP     plat_exit

;! section data
plat_error__msg:
        DC.B    'runtime error '
