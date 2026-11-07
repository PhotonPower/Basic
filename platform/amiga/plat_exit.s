;! module    plat_exit
;! provides  plat_exit plat_exit_hook plat_exit_gfx plat_exit_gui
;! requires  plat_flush plat_free_all plat_dosbase plat_sp plat_rc
;! group     core
;! doc       amiga: ends the program from any depth: flush, free memory, close dos.library, return to DOS.

plat_exit:
        TST.L   plat_exit_gui           ; the menu strip of plat_gui (before its window closes)
        BEQ     .nogui
        MOVE.L  plat_exit_gui,A0
        CLR.L   plat_exit_gui
        JSR     (A0)
.nogui: TST.L   plat_exit_gfx           ; windows and screens of plat_gfx
        BEQ     .nogfx
        MOVE.L  plat_exit_gfx,A0
        CLR.L   plat_exit_gfx
        JSR     (A0)
.nogfx: TST.L   plat_exit_hook          ; e.g. close the open files (rt_file)
        BEQ     .nohook
        MOVE.L  plat_exit_hook,A0
        CLR.L   plat_exit_hook
        JSR     (A0)
.nohook:
        JSR     plat_flush
        MOVE.L  plat_sp,SP              ; back to the shell's stack before the own one is freed
        JSR     plat_free_all
        MOVE.L  4,A6
        MOVE.L  plat_dosbase,A1
        JSR     -414(A6)                ; CloseLibrary
        MOVE.L  plat_rc,D0
        MOVEM.L (SP)+,D2-D7/A2-A6
        RTS

;! section data
plat_exit_hook:
        DC.L    0
plat_exit_gfx:
        DC.L    0
plat_exit_gui:
        DC.L    0
