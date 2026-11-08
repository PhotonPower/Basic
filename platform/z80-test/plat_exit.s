;! module    plat_exit
;! provides  plat_exit
;! group     core
;! clobbers  -
;! doc       z80-test: the end of the program (DI, HALT).

plat_exit:
        DI
        HALT
