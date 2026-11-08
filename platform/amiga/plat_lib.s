;! module    plat_lib
;! provides  plat_libcall plat_window plat_screen
;! requires  plat_dosbase plat_gfx_win plat_gfx_scr plat_exit_lib
;! group     amiga-os
;! clobbers  D0-D3/A0-A1
;! doc       amiga: library functions through their base (D28); intuition and graphics are opened on demand.

; D0 = library ('e', 'd', 'i', 'g'), D1 = offset, A0 = 14 registers d0..d7, a0..a5 -> D0 = result
plat_libcall:
        MOVEM.L D2-D7/A2-A6,-(SP)
        MOVE.L  A0,A2
        MOVE.L  D1,D7
        CMP.B   #'e',D0
        BNE     .dos
        MOVE.L  4,D0
        BRA     .base
.dos:   CMP.B   #'d',D0
        BNE     .int
        MOVE.L  plat_dosbase,D0
        BRA     .base
.int:   LEA     plat_lib__ib,A3
        LEA     plat_lib__iname,A1
        CMP.B   #'i',D0
        BEQ     .open
        LEA     plat_lib__gb,A3
        LEA     plat_lib__gname,A1
.open:  MOVE.L  (A3),D0                 ; opened on the first call, closed at the end
        BNE     .base
        MOVE.L  4,A6
        MOVEQ   #0,D0
        JSR     -552(A6)                ; OpenLibrary
        MOVE.L  D0,(A3)
        BEQ     .out
        MOVE.L  #plat_lib__close,plat_exit_lib
.base:  MOVE.L  D0,plat_lib__base
        BEQ     .out
        ADD.L   D7,D0
        LEA     .ret,A1                 ; return address, then the function: RTS jumps there
        MOVE.L  A1,-(SP)
        MOVE.L  D0,-(SP)
        MOVEM.L (A2),D0-D7/A0-A5
        MOVE.L  plat_lib__base,A6
        RTS
.ret:
.out:   MOVEM.L (SP)+,D2-D7/A2-A6
        RTS

plat_lib__close:
        MOVE.L  A6,-(SP)
        MOVE.L  4,A6
        MOVE.L  plat_lib__ib,D0
        BEQ     .g
        MOVE.L  D0,A1
        JSR     -414(A6)
        CLR.L   plat_lib__ib
.g:     MOVE.L  plat_lib__gb,D0
        BEQ     .out
        MOVE.L  D0,A1
        JSR     -414(A6)
        CLR.L   plat_lib__gb
.out:   MOVE.L  (SP)+,A6
        RTS

; D0 = number -> D0 = the Window / Screen structure of GFA-BASIC's window / screen n
plat_window:
        JMP     plat_gfx_win
plat_screen:
        JMP     plat_gfx_scr

;! section data
plat_lib__iname:
        DC.B    'intuition.library',0
plat_lib__gname:
        DC.B    'graphics.library',0
        EVEN

;! section bss
plat_lib__base:
        DS.L    1
plat_lib__ib:
        DS.L    1
plat_lib__gb:
        DS.L    1
