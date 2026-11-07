;! module    plat_exit
;! provides  plat_exit
;! requires  plat_flush plat_free_all plat_dosbase plat_sp plat_rc
;! group     core
;! doc       amiga: ends the program from any depth: flush, free memory, close dos.library, return to DOS.

plat_exit:
        JSR     plat_flush
        MOVE.L  plat_sp,SP              ; back to the shell's stack before the own one is freed
        JSR     plat_free_all
        MOVE.L  4,A6
        MOVE.L  plat_dosbase,A1
        JSR     -414(A6)                ; CloseLibrary
        MOVE.L  plat_rc,D0
        MOVEM.L (SP)+,D2-D7/A2-A6
        RTS
