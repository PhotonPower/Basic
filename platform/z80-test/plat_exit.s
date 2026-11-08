;! module    plat_exit
;! provides  plat_exit plat_exit_hook
;! group     core
;! clobbers  -
;! doc       z80-test: the end of the program (the hook first, e.g. rt_file closes the files; DI, HALT).

plat_exit:
        LD      HL,(plat_exit_hook)
        LD      A,H
        OR      L
        JR      Z,plat_exit__end
        LD      DE,0
        LD      (plat_exit_hook),DE
        LD      DE,plat_exit
        PUSH    DE
        JP      (HL)
plat_exit__end:
        DI
        HALT

;! section data
plat_exit_hook:
        DW      0
