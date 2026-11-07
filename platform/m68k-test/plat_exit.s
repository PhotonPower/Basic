;! module    plat_exit
;! provides  plat_exit plat_exit_hook
;! group     core
;! doc       m68k-test: ends the program (STOP).

plat_exit:
        TST.L   plat_exit_hook          ; e.g. close the open files (rt_file)
        BEQ     .nohook
        MOVE.L  plat_exit_hook,A0
        CLR.L   plat_exit_hook
        JSR     (A0)
.nohook:
        STOP    #$2700
        BRA     plat_exit

;! section data
plat_exit_hook:
        DC.L    0
