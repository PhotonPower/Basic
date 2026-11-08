;! module    plat_error
;! provides  plat_error
;! requires  rt_print_str rt_print_int rt_print_nl plat_exit
;! group     core
;! clobbers  -
;! doc       z80-test: an unhandled runtime error (A) prints "runtime error <n>" and ends the program.

plat_error:
        LD      (plat_error__n),A
        CALL    rt_print_nl
        LD      DE,plat_error__msg
        LD      BC,14
        CALL    rt_print_str
        LD      DE,plat_error__n
        CALL    rt_print_int
        CALL    rt_print_nl
        JP      plat_exit

;! section data
plat_error__msg:
        DB      "runtime error "
plat_error__n:
        DB      0,0,0,0
