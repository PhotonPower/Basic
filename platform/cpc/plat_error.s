;! module    plat_error
;! provides  plat_error
;! requires  rt_print_str rt_print_int rt_print_nl plat_exit
;! group     core
;! clobbers  -
;! doc       cpc: "runtime error n" on the screen, then back to the caller.

plat_error:
        LD      (plat_error__n),A
        CP      255                     ; -1 (RESUME outside the error routine)
        JR      NZ,plat_error__go
        LD      HL,&FFFF
        LD      (plat_error__n),HL
        LD      (plat_error__n+2),HL
plat_error__go:
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
