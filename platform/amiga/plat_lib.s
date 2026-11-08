;! module    plat_lib
;! provides  plat_libcall plat_window plat_screen plat_mouse plat_stick plat_strig
;! requires  plat_dosbase plat_gfx_win plat_gfx_scr plat_gfx_current plat_exit_lib
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

; D0 = 0 x, 1 y, 2 buttons -> D0 (D29). The position is that of the current window (Intuition
; keeps it in the Window structure; inner coordinates for GIMMEZEROZERO), 0 without graphics. The
; buttons come from the hardware: left CIA-A PRA bit 6, right and middle POTGOR bits 10 and 8 (0 =
; pressed).
plat_mouse:
        CMP.L   #2,D0
        BEQ     .keys
        MOVE.L  D0,D1
        JSR     plat_gfx_current        ; -> D0 = window or 0
        TST.L   D0
        BEQ     .out
        MOVE.L  D0,A0
        BTST    #2,25(A0)               ; GIMMEZEROZERO
        BEQ     .raw
        LEA     108(A0),A0              ; GZZMouseX, GZZMouseY
        BRA     .pos
.raw:   LEA     12(A0),A0               ; MouseY, MouseX
        TST.L   D1
        BNE     .ry
        MOVE.W  2(A0),D0                ; MouseX
        EXT.L   D0
        RTS
.ry:    MOVE.W  (A0),D0                 ; MouseY
        EXT.L   D0
        RTS
.pos:   TST.L   D1
        BNE     .py
        MOVE.W  (A0),D0
        EXT.L   D0
        RTS
.py:    MOVE.W  2(A0),D0
        EXT.L   D0
        RTS
.keys:  MOVEQ   #0,D0
        BTST    #6,$BFE001
        BNE     .r
        BSET    #0,D0
.r:     MOVE.W  $DFF016,D1              ; POTGOR
        BTST    #10,D1
        BNE     .m
        BSET    #1,D0
.m:     BTST    #8,D1
        BNE     .out
        BSET    #2,D0
.out:   RTS

; D0 = port (0 mouse port, 1 joystick port) -> D0 = 1 up, 2 down, 4 left, 8 right from JOYxDAT
plat_stick:
        LEA     $DFF00A,A0              ; JOY0DAT
        TST.L   D0
        BEQ     .p
        LEA     $DFF00C,A0              ; JOY1DAT
.p:     MOVE.W  (A0),D1
        MOVEQ   #0,D0
        BTST    #1,D1                   ; right
        BEQ     .nr
        BSET    #3,D0
.nr:    BTST    #9,D1                   ; left
        BEQ     .nl
        BSET    #2,D0
.nl:    MOVE.W  D1,D2                   ; down: bit 1 XOR bit 0
        LSR.W   #1,D2
        EOR.W   D1,D2
        BTST    #0,D2
        BEQ     .nd
        BSET    #1,D0
.nd:    MOVE.W  D1,D2                   ; up: bit 9 XOR bit 8
        LSR.W   #1,D2
        EOR.W   D1,D2
        BTST    #8,D2
        BEQ     .nu
        BSET    #0,D0
.nu:    RTS

; D0 = port -> D0 = -1 if its fire button is pressed (CIA-A PRA bit 6 port 0, bit 7 port 1)
plat_strig:
        MOVEQ   #6,D1
        TST.L   D0
        BEQ     .b
        MOVEQ   #7,D1
.b:     MOVEQ   #0,D0
        BTST    D1,$BFE001
        BNE     .out
        MOVEQ   #-1,D0
.out:   RTS

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
