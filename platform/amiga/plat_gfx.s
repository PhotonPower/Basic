;! module    plat_gfx
;! provides  plat_gfx plat_gfx_point plat_gfx_window plat_gfx_putc plat_gfx_win plat_gfx_scr plat_gfx_current
;! requires  plat_exit_gfx plat_putc_gfx
;! group     graphics
;! clobbers  D0-D3/A0-A1
;! doc       amiga: graphics (D25 stage 2) with intuition.library and graphics.library, as the original.

; The first graphics command opens screen 0 (640 x 256 HiRes, 2 bitplanes, palette of Kickstart 1.x)
; and window 0 on it (GIMMEZEROZERO: coordinates from the inner area, clipped there). Drawing goes
; through the ROM: SetAPen/SetBPen/SetDrMd, LinePtrn, WritePixel, Move/Draw, RectFill, DrawEllipse,
; AreaEllipse (with TmpRas and AreaInfo), Text, ReadPixel, SetRGB4, SetRast. The pens are kept here
; and given to every window that becomes the current one. At the end of the program
; (plat_exit_gfx) windows, screens, the TmpRas and the libraries are given back.
; Not yet: other screens than 0 with own sizes are opened, but windows always go to screen 0 unless
; the window command names an open screen; IDCMP and window flags of OPENW are ignored.

plat_gfx__NWIN  EQU 16
plat_gfx__NSCR  EQU 8
plat_gfx__NVEC  EQU 132                 ; area vectors: POLYFILL with up to 128 points

; exec, intuition, graphics
plat_gfx__OpenLibrary   EQU -552
plat_gfx__CloseLibrary  EQU -414
plat_gfx__OpenScreen    EQU -198
plat_gfx__CloseScreen   EQU -66
plat_gfx__OpenWindow    EQU -204
plat_gfx__CloseWindow   EQU -72
plat_gfx__SetWTitles    EQU -276
plat_gfx__ScrToFront    EQU -252
plat_gfx__ScrToBack     EQU -246
plat_gfx__WinToFront    EQU -312
plat_gfx__WinToBack     EQU -306
plat_gfx__Text          EQU -60
plat_gfx__DrawEllipse   EQU -180
plat_gfx__AreaEllipse   EQU -186
plat_gfx__LoadRGB4      EQU -192
plat_gfx__SetRast       EQU -234
plat_gfx__Draw          EQU -246
plat_gfx__Move          EQU -240
plat_gfx__ScrollRaster  EQU -396
plat_gfx__AreaMove      EQU -252
plat_gfx__AreaDraw      EQU -258
plat_gfx__AreaEnd       EQU -264
plat_gfx__InitArea      EQU -282
plat_gfx__SetRGB4       EQU -288
plat_gfx__RectFill      EQU -306
plat_gfx__ReadPixel     EQU -318
plat_gfx__WritePixel    EQU -324
plat_gfx__SetAPen       EQU -342
plat_gfx__SetBPen       EQU -348
plat_gfx__SetDrMd       EQU -354
plat_gfx__InitTmpRas    EQU -468
plat_gfx__AllocRaster   EQU -492
plat_gfx__FreeRaster    EQU -498

; ---- entry points ---------------------------------------------------------------------------------

; D0 = code (brt::kGfxCommands), D1 = mode (arguments given; DRAW: 1 = DRAW TO), D2 = number of
; arguments, A0 = arguments (longs), A1 = string descriptor or 0
plat_gfx:
        MOVEM.L D2-D7/A2-A6,-(SP)
        MOVE.L  D0,D7                   ; D7 = code
        MOVE.L  D1,D6                   ; D6 = mode
        MOVE.L  D2,D5                   ; D5 = number of arguments
        MOVE.L  A0,A2                   ; A2 = arguments
        MOVE.L  A1,A3                   ; A3 = string
        CMP.L   #27,D7                  ; CLS: only if graphics are open
        BNE     .open
        TST.L   plat_gfx__gb
        BEQ     .out
        BSR     plat_gfx__rp
        MOVEQ   #0,D0
        JSR     plat_gfx__SetRast(A6)
        BRA     .out
.open:  BSR     plat_gfx__init
        TST.L   plat_gfx__gb
        BEQ     .out
        MOVE.L  D7,D0                   ; jump table
        ADD.L   D0,D0
        LEA     plat_gfx__table,A0
        MOVE.W  0(A0,D0.L),D0
        JSR     0(A0,D0.W)
.out:   MOVEM.L (SP)+,D2-D7/A2-A6
        RTS

; D0 = x, D1 = y -> D0 = POINT(x,y): ReadPixel, -1 outside
plat_gfx_point:
        MOVEM.L D2-D7/A2-A6,-(SP)
        MOVE.L  D0,D4
        MOVE.L  D1,D5
        BSR     plat_gfx__init
        MOVEQ   #-1,D0
        TST.L   plat_gfx__gb
        BEQ     .out
        BSR     plat_gfx__rp
        MOVE.L  D4,D0
        MOVE.L  D5,D1
        JSR     plat_gfx__ReadPixel(A6)
.out:   MOVEM.L (SP)+,D2-D7/A2-A6
        RTS

; -> D0 = the current window (screen 0 and window 0 are opened first; 0 if that failed), D1 = base of
; intuition.library, D2 = base of graphics.library (for plat_gui)
plat_gfx_window:
        MOVEM.L D3-D7/A2-A6,-(SP)
        BSR     plat_gfx__init
        MOVEQ   #0,D0
        TST.L   plat_gfx__gb
        BEQ     .out
        MOVE.L  plat_gfx__cur,D0
        LSL.L   #2,D0
        LEA     plat_gfx__win,A1
        MOVE.L  0(A1,D0.L),D0
        MOVE.L  plat_gfx__ib,D1
        MOVE.L  plat_gfx__gb,D2
.out:   MOVEM.L (SP)+,D3-D7/A2-A6
        RTS

; D0.B = character of PRINT (all registers are kept): also into the current window, in cells of
; 8 x 8 pixels (topaz 8) at its text cursor, JAM2 with pen 1 on 0 (probe26 T3, probe27). ESC [ line ; column H moves
; the cursor (LOCATE, PRINT AT, CLS from rt_print), LF begins the next line and scrolls at the
; bottom, CR goes to column 0, BS one back.
plat_gfx_putc:
        MOVEM.L D0-D4/A0-A1/A6,-(SP)
        TST.L   plat_gfx__gb
        BEQ     .out
        MOVE.L  plat_gfx__cur,D4
        MOVE.L  D4,D1
        LSL.L   #2,D1
        LEA     plat_gfx__win,A0
        MOVE.L  0(A0,D1.L),D1
        BEQ     .out
        MOVE.L  D1,A0                   ; A0 = window
        ADD.L   D4,D4                   ; D4 = offset of its cursor
        AND.L   #$FF,D0
        MOVE.W  plat_gfx__es,D1         ; inside an escape sequence?
        BEQ     .plain
        CMP.W   #1,D1
        BNE     .csi
        CLR.W   plat_gfx__es
        CMP.B   #'[',D0
        BNE     .out
        MOVE.W  #2,plat_gfx__es
        CLR.L   plat_gfx__ep
        CLR.L   plat_gfx__ep+4
        CLR.W   plat_gfx__en
        BRA     .out
.csi:   CMP.B   #'0',D0
        BCS     .nd
        CMP.B   #'9',D0
        BHI     .nd
        SUB.B   #'0',D0
        LEA     plat_gfx__ep,A1
        MOVE.W  plat_gfx__en,D1
        LSL.W   #2,D1
        MOVE.L  0(A1,D1.W),D2
        MULU    #10,D2
        ADD.L   D0,D2
        MOVE.L  D2,0(A1,D1.W)
        BRA     .out
.nd:    CMP.B   #';',D0
        BNE     .fin
        MOVE.W  #1,plat_gfx__en
        BRA     .out
.fin:   CLR.W   plat_gfx__es            ; the end of the sequence
        CMP.B   #'H',D0
        BNE     .out
        MOVE.L  plat_gfx__ep,D0         ; line, column from 1 (0 counts as 1)
        BEQ     .l0
        SUBQ.L  #1,D0
.l0:    LEA     plat_gfx__crow,A1
        MOVE.W  D0,0(A1,D4.L)
        MOVE.L  plat_gfx__ep+4,D0
        BEQ     .c0
        SUBQ.L  #1,D0
.c0:    LEA     plat_gfx__ccol,A1
        MOVE.W  D0,0(A1,D4.L)
        BRA     .out
.plain: CMP.B   #27,D0
        BNE     .nesc
        MOVE.W  #1,plat_gfx__es
        BRA     .out
.nesc:  CMP.B   #10,D0
        BEQ     .lf
        CMP.B   #13,D0
        BNE     .nbs
        LEA     plat_gfx__ccol,A1
        CLR.W   0(A1,D4.L)
        BRA     .out
.nbs:   CMP.B   #8,D0
        BNE     .ch
        LEA     plat_gfx__ccol,A1
        TST.W   0(A1,D4.L)
        BEQ     .out
        SUBQ.W  #1,0(A1,D4.L)
        BRA     .out
.ch:    CMP.B   #32,D0
        BCS     .out
        MOVE.B  D0,plat_gfx__ch
        MOVE.L  A0,D3                   ; D3 = window
        MOVE.L  50(A0),A1               ; its RastPort
        MOVE.L  plat_gfx__gb,A6
        MOVEQ   #0,D0                   ; x = column * 8, base line = line * 8 + 6, from the inner
        LEA     plat_gfx__ccol,A0       ; edge (a window without GIMMEZEROZERO: its borders, probe26 T2)
        MOVE.W  0(A0,D4.L),D0
        LSL.L   #3,D0
        MOVEQ   #0,D1
        LEA     plat_gfx__crow,A0
        MOVE.W  0(A0,D4.L),D1
        LSL.L   #3,D1
        ADDQ.L  #6,D1
        MOVE.L  D3,A0
        BTST    #2,25(A0)               ; Window->Flags: GIMMEZEROZERO ($400)
        BNE     .gzz
        MOVEQ   #0,D2
        MOVE.B  54(A0),D2               ; BorderLeft
        ADD.L   D2,D0
        MOVE.B  55(A0),D2               ; BorderTop
        ADD.L   D2,D1
.gzz:   MOVE.L  A1,-(SP)
        JSR     plat_gfx__Move(A6)
        MOVE.L  (SP),A1
        MOVEQ   #0,D0                   ; JAM2 for the text (the background too, probe26 T3), XOR
        MOVE.B  28(A1),D0               ; stays; then the mode of GRAPHMODE again
        MOVE.L  D0,-(SP)
        BTST    #1,D0
        BNE     .cmp
        MOVEQ   #1,D0
.cmp:   JSR     plat_gfx__SetDrMd(A6)
        MOVE.L  4(SP),A1                ; pen 1 on 0, whatever COLOR says (probe27)
        MOVEQ   #0,D0
        MOVE.B  25(A1),D0               ; RastPort FgPen, BgPen
        MOVE.L  D0,-(SP)
        MOVE.B  26(A1),D0
        MOVE.L  D0,-(SP)
        MOVEQ   #1,D0
        JSR     plat_gfx__SetAPen(A6)
        MOVE.L  12(SP),A1
        MOVEQ   #0,D0
        JSR     plat_gfx__SetBPen(A6)
        MOVE.L  12(SP),A1
        LEA     plat_gfx__ch,A0
        MOVEQ   #1,D0
        JSR     plat_gfx__Text(A6)
        MOVE.L  12(SP),A1               ; the pens and the mode back
        MOVE.L  (SP)+,D0
        JSR     plat_gfx__SetBPen(A6)
        MOVE.L  8(SP),A1
        MOVE.L  (SP)+,D0
        JSR     plat_gfx__SetAPen(A6)
        MOVE.L  (SP)+,D0
        MOVE.L  (SP)+,A1
        JSR     plat_gfx__SetDrMd(A6)
        LEA     plat_gfx__ccol,A0       ; next column; at the right edge the next line
        ADDQ.W  #1,0(A0,D4.L)
        MOVE.L  D3,A0
        MOVE.W  112(A0),D0              ; Window->GZZWidth
        LSR.W   #3,D0
        LEA     plat_gfx__ccol,A1
        CMP.W   0(A1,D4.L),D0
        BHI     .out
.lf:    LEA     plat_gfx__ccol,A1
        CLR.W   0(A1,D4.L)
        LEA     plat_gfx__crow,A1
        ADDQ.W  #1,0(A1,D4.L)
        MOVEQ   #0,D0                   ; below the last whole line: scroll up by one
        MOVE.W  0(A1,D4.L),D0
        ADDQ.L  #1,D0
        LSL.L   #3,D0
        MOVEQ   #0,D1
        MOVE.W  114(A0),D1              ; Window->GZZHeight
        CMP.L   D1,D0
        BLE     .out
        SUBQ.W  #1,0(A1,D4.L)
        MOVE.L  50(A0),A1
        MOVEQ   #0,D2
        MOVE.W  112(A0),D2
        SUBQ.L  #1,D2
        MOVEQ   #0,D3
        MOVE.W  114(A0),D3
        SUBQ.L  #1,D3
        MOVEM.L D4-D5,-(SP)
        MOVE.L  D2,D4                   ; ScrollRaster(rp, 0, 8, 0, 0, w-1, h-1)
        MOVE.L  D3,D5
        MOVEQ   #0,D0
        MOVEQ   #8,D1
        MOVEQ   #0,D2
        MOVEQ   #0,D3
        MOVE.L  plat_gfx__gb,A6
        JSR     plat_gfx__ScrollRaster(A6)
        MOVEM.L (SP)+,D4-D5
.out:   MOVEM.L (SP)+,D0-D4/A0-A1/A6
        RTS

; -> D0 = the current window, 0 if the graphics are not open (nothing is opened here)
plat_gfx_current:
        MOVEQ   #0,D0
        TST.L   plat_gfx__gb
        BEQ     .out
        MOVE.L  plat_gfx__cur,D0
        LSL.L   #2,D0
        LEA     plat_gfx__win,A0
        MOVE.L  0(A0,D0.L),D0
.out:   RTS

; D0 = n -> D0 = the Window (plat_gfx_win) / Screen (plat_gfx_scr) structure n, 0 if not open
; (WINDOW(n), SCREEN(n); screen 0 and window 0 are opened first)
plat_gfx_win:
        LEA     plat_gfx__win,A0
        MOVEQ   #plat_gfx__NWIN,D1
        BRA     plat_gfx__struct
plat_gfx_scr:
        LEA     plat_gfx__scr,A0
        MOVEQ   #plat_gfx__NSCR,D1
plat_gfx__struct:
        MOVEM.L D2-D7/A2-A6,-(SP)
        MOVEM.L D0-D1/A0,-(SP)          ; the first opening uses D2-D7/A2
        BSR     plat_gfx__init
        MOVEM.L (SP)+,D2-D3/A2
        MOVEQ   #0,D0
        CMP.L   D3,D2
        BCC     .out
        LSL.L   #2,D2
        MOVE.L  0(A2,D2.L),D0
.out:   MOVEM.L (SP)+,D2-D7/A2-A6
        RTS

plat_gfx__table:
        DC.W    plat_gfx__opens-plat_gfx__table, plat_gfx__closes-plat_gfx__table
        DC.W    plat_gfx__openw-plat_gfx__table, plat_gfx__closew-plat_gfx__table
        DC.W    plat_gfx__clearw-plat_gfx__table, plat_gfx__titlew-plat_gfx__table
        DC.W    plat_gfx__titles-plat_gfx__table, plat_gfx__fronts-plat_gfx__table
        DC.W    plat_gfx__backs-plat_gfx__table, plat_gfx__frontw-plat_gfx__table
        DC.W    plat_gfx__backw-plat_gfx__table, plat_gfx__color-plat_gfx__table
        DC.W    plat_gfx__setcolor-plat_gfx__table, plat_gfx__graphmode-plat_gfx__table
        DC.W    plat_gfx__deffill-plat_gfx__table, plat_gfx__boundary-plat_gfx__table
        DC.W    plat_gfx__defline-plat_gfx__table, plat_gfx__plot-plat_gfx__table
        DC.W    plat_gfx__line-plat_gfx__table, plat_gfx__draw-plat_gfx__table
        DC.W    plat_gfx__box-plat_gfx__table, plat_gfx__pbox-plat_gfx__table
        DC.W    plat_gfx__circle-plat_gfx__table, plat_gfx__pcircle-plat_gfx__table
        DC.W    plat_gfx__ellipse-plat_gfx__table, plat_gfx__pellipse-plat_gfx__table
        DC.W    plat_gfx__text-plat_gfx__table, plat_gfx__none-plat_gfx__table
        DC.W    plat_gfx__polyline-plat_gfx__table, plat_gfx__polyfill-plat_gfx__table

; ---- helpers ----------------------------------------------------------------------------------------

; -> A1 = RastPort of the current window, A6 = GfxBase
plat_gfx__rp:
        MOVE.L  plat_gfx__cur,D0
        LSL.L   #2,D0
        LEA     plat_gfx__win,A1
        MOVE.L  0(A1,D0.L),A1
        MOVE.L  50(A1),A1               ; Window->RPort (the inner one: GIMMEZEROZERO)
        MOVE.L  plat_gfx__gb,A6
        RTS

; D0 = k -> D0 = argument k (A2 = arguments)
plat_gfx__arg:
        LSL.L   #2,D0
        MOVE.L  0(A2,D0.L),D0
        RTS

plat_gfx__none:
        RTS

; first use: libraries, screen 0, window 0, TmpRas and AreaInfo
plat_gfx__init:
        TST.L   plat_gfx__gb
        BNE     .out
        TST.L   plat_gfx__failed
        BNE     .out
        MOVE.L  D5,-(SP)                ; the argument count of the running command
        MOVEQ   #0,D5                   ; screen 0 and window 0 in their standard form
        BSR     .first
        MOVE.L  (SP)+,D5
.out:   RTS
.first: MOVE.L  #plat_gfx__close,plat_exit_gfx
        MOVE.L  #plat_gfx_putc,plat_putc_gfx
        MOVE.L  4,A6
        LEA     plat_gfx__intname,A1
        MOVEQ   #0,D0
        JSR     plat_gfx__OpenLibrary(A6)
        MOVE.L  D0,plat_gfx__ib
        BEQ     .fail
        LEA     plat_gfx__gfxname,A1
        MOVEQ   #0,D0
        JSR     plat_gfx__OpenLibrary(A6)
        MOVE.L  D0,plat_gfx__gb
        BEQ     .fail
        MOVE.L  D0,A6                   ; the TmpRas for AreaEllipse: one bitplane of the screen
        MOVE.L  #640,D0
        MOVE.L  #256,D1
        JSR     plat_gfx__AllocRaster(A6)
        MOVE.L  D0,plat_gfx__ras
        BEQ     .fail
        LEA     plat_gfx__tmpras,A0
        MOVE.L  D0,A1
        MOVE.L  #256*80,D0
        JSR     plat_gfx__InitTmpRas(A6)
        LEA     plat_gfx__area,A0
        LEA     plat_gfx__vbuf,A1
        MOVE.L  #plat_gfx__NVEC,D0
        JSR     plat_gfx__InitArea(A6)
        MOVEQ   #0,D0                   ; screen 0
        BSR     plat_gfx__openscreen
        TST.L   plat_gfx__scr
        BEQ     .fail
        MOVEQ   #0,D0                   ; window 0
        BSR     plat_gfx__openwindow
        TST.L   plat_gfx__win
        BEQ     .fail
        RTS
.fail:  MOVE.L  #1,plat_gfx__failed
        BSR     plat_gfx__close
        RTS

; D0 = screen number, D5 = number of arguments, A2: OPENS n,x,y,w,h,depth[,mode] (without: 640 x 256 x 2)
plat_gfx__openscreen:
        MOVE.L  D0,D4
        LEA     plat_gfx__ns,A0
        MOVE.L  #$02800100,4(A0)        ; Width 640, Height 256
        MOVE.W  #2,8(A0)                ; Depth
        MOVE.W  #$8000,12(A0)           ; HIRES
        CMP.L   #6,D5
        BLT     .def
        MOVEQ   #3,D0
        BSR     plat_gfx__arg
        MOVE.W  D0,4(A0)
        MOVEQ   #4,D0
        BSR     plat_gfx__arg
        MOVE.W  D0,6(A0)
        MOVEQ   #5,D0
        BSR     plat_gfx__arg
        MOVE.W  D0,8(A0)
        CLR.W   12(A0)                  ; view modes: the 7th argument (&H8000 HIRES, &H4 LACE),
        CMP.L   #7,D5                   ; else HIRES above 320 pixels
        BLT     .vm
        MOVEQ   #6,D0
        BSR     plat_gfx__arg
        MOVE.W  D0,12(A0)
        BRA     .def
.vm:    CMP.W   #320,4(A0)
        BLE     .def
        MOVE.W  #$8000,12(A0)
.def:   MOVE.L  plat_gfx__ib,A6
        JSR     plat_gfx__OpenScreen(A6)
        LEA     plat_gfx__scr,A0
        MOVE.L  D4,D1
        LSL.L   #2,D1
        MOVE.L  D0,0(A0,D1.L)
        BEQ     .out
        MOVE.L  D4,plat_gfx__lscr       ; OPENW without a screen goes there
        MOVE.L  D0,A0                   ; the palette of Kickstart 1.x
        LEA     44(A0),A0               ; Screen->ViewPort
        LEA     plat_gfx__palette,A1
        MOVEQ   #4,D0
        MOVE.L  plat_gfx__gb,A6
        JSR     plat_gfx__LoadRGB4(A6)
.out:   RTS

; D0 = window number, D5 = number of arguments, A2: OPENW n,x,y,w,h[,idcmp,flags[,scr]]
plat_gfx__openwindow:
        MOVE.L  D0,D4                   ; D4 = n
        LEA     plat_gfx__nw,A0
        MOVE.L  #$00001402,14(A0)       ; WINDOWDRAG, GIMMEZEROZERO, ACTIVATE
        CMP.L   #7,D5                   ; the flags of OPENW
        BLT     .fl
        MOVEQ   #6,D0                   ; as given: no GIMMEZEROZERO, the coordinates count from the
        BSR     plat_gfx__arg           ; edge of the window (probe26 T2, T3)
        MOVE.L  D0,14(A0)
.fl:    MOVE.L  plat_gfx__lscr,D3       ; D3 = screen number: the last opened one
        CMP.L   #8,D5
        BLT     .scr0
        MOVEQ   #7,D0
        BSR     plat_gfx__arg
        TST.L   D0
        BMI     .scr0
        MOVE.L  D0,D3
.scr0:  LSL.L   #2,D3
        LEA     plat_gfx__scr,A1
        MOVE.L  0(A1,D3.L),D0
        BNE     .scrok
        MOVE.L  plat_gfx__scr,D0        ; not open: screen 0
.scrok: MOVE.L  D0,30(A0)               ; NewWindow.Screen
        CMP.L   #5,D5
        BLT     .std
        MOVEQ   #1,D0
        BSR     plat_gfx__arg
        MOVE.W  D0,0(A0)
        MOVEQ   #2,D0
        BSR     plat_gfx__arg
        MOVE.W  D0,2(A0)
        MOVEQ   #3,D0
        BSR     plat_gfx__arg
        MOVE.W  D0,4(A0)
        MOVEQ   #4,D0
        BSR     plat_gfx__arg
        MOVE.W  D0,6(A0)
        BRA     .open
.std:   MOVE.L  D4,D0                   ; 0 the whole screen below its title, 1..4 quarters, 5.. centred
        LSL.L   #3,D0
        LEA     plat_gfx__geom,A1
        MOVE.L  0(A1,D0.L),0(A0)        ; LeftEdge, TopEdge
        MOVE.L  4(A1,D0.L),4(A0)        ; Width, Height
.open:  MOVE.L  plat_gfx__ib,A6
        JSR     plat_gfx__OpenWindow(A6)
        LEA     plat_gfx__win,A0
        MOVE.L  D4,D1
        LSL.L   #2,D1
        MOVE.L  D0,0(A0,D1.L)
        BEQ     .out
        MOVE.L  D4,plat_gfx__cur
        MOVE.L  D4,D1                   ; the text cursor at the top left
        ADD.L   D1,D1
        LEA     plat_gfx__ccol,A0
        CLR.W   0(A0,D1.L)
        LEA     plat_gfx__crow,A0
        CLR.W   0(A0,D1.L)
        BSR     plat_gfx__state         ; area buffers and pens for its RastPort
.out:   RTS

; the RastPort of the current window gets the TmpRas, the AreaInfo and the pens
plat_gfx__state:
        BSR     plat_gfx__rp
        MOVE.L  #plat_gfx__tmpras,12(A1)
        MOVE.L  #plat_gfx__area,16(A1)
        MOVE.B  plat_gfx__opn+3,27(A1)  ; AOlPen
        MOVE.W  plat_gfx__pat,34(A1)    ; LinePtrn
        MOVE.B  #15,30(A1)
        BCLR    #3,33(A1)               ; AREAOUTLINE by BOUNDARY
        TST.L   plat_gfx__bnd
        BEQ     .nb
        BSET    #3,33(A1)
.nb:    BSET    #0,33(A1)               ; FRST_DOT
        MOVE.L  A1,-(SP)
        MOVE.L  plat_gfx__apen,D0
        JSR     plat_gfx__SetAPen(A6)
        MOVE.L  (SP),A1
        MOVE.L  plat_gfx__bpen,D0
        JSR     plat_gfx__SetBPen(A6)
        MOVE.L  (SP)+,A1
        MOVE.L  plat_gfx__mode,D0
        JMP     plat_gfx__SetDrMd(A6)

; at the end of the program: everything back
plat_gfx__close:
        CLR.L   plat_putc_gfx
        MOVEM.L D2/A2/A6,-(SP)
        MOVE.L  plat_gfx__ib,D0
        BEQ     .libs
        MOVE.L  D0,A6
        MOVEQ   #plat_gfx__NWIN-1,D2
        LEA     plat_gfx__win,A2
.w:     MOVE.L  (A2),D0
        BEQ     .wn
        CLR.L   (A2)
        MOVE.L  D0,A0
        JSR     plat_gfx__CloseWindow(A6)
.wn:    ADDQ.L  #4,A2
        DBRA    D2,.w
        MOVEQ   #plat_gfx__NSCR-1,D2
        LEA     plat_gfx__scr,A2
.s:     MOVE.L  (A2),D0
        BEQ     .sn
        CLR.L   (A2)
        MOVE.L  D0,A0
        JSR     plat_gfx__CloseScreen(A6)
.sn:    ADDQ.L  #4,A2
        DBRA    D2,.s
.libs:  MOVE.L  plat_gfx__ras,D0
        BEQ     .nr
        CLR.L   plat_gfx__ras
        MOVE.L  D0,A0
        MOVE.L  #640,D0
        MOVE.L  #256,D1
        MOVE.L  plat_gfx__gb,A6
        JSR     plat_gfx__FreeRaster(A6)
.nr:    MOVE.L  4,A6
        MOVE.L  plat_gfx__gb,D0
        BEQ     .ng
        CLR.L   plat_gfx__gb
        MOVE.L  D0,A1
        JSR     plat_gfx__CloseLibrary(A6)
.ng:    MOVE.L  plat_gfx__ib,D0
        BEQ     .ni
        CLR.L   plat_gfx__ib
        MOVE.L  D0,A1
        JSR     plat_gfx__CloseLibrary(A6)
.ni:    MOVEM.L (SP)+,D2/A2/A6
        RTS

; ---- commands (A2 = arguments, D5 = number, D6 = mode, A3 = string) ---------------------------------

plat_gfx__opens:
        MOVEQ   #0,D0
        BSR     plat_gfx__arg
        CMP.L   #plat_gfx__NSCR,D0
        BCC     .out
        MOVE.L  D0,D1
        LSL.L   #2,D1
        LEA     plat_gfx__scr,A0
        TST.L   0(A0,D1.L)
        BNE     .out                    ; open already: nothing more
        BRA     plat_gfx__openscreen
.out:   RTS

plat_gfx__closes:
        MOVEQ   #0,D0
        BSR     plat_gfx__arg
        CMP.L   #plat_gfx__NSCR,D0
        BCC     .out
        LSL.L   #2,D0
        LEA     plat_gfx__scr,A0
        MOVE.L  0(A0,D0.L),D1
        BEQ     .out
        CLR.L   0(A0,D0.L)
        MOVE.L  D1,D4                   ; first its windows
        MOVEQ   #plat_gfx__NWIN-1,D3
        LEA     plat_gfx__win,A4
.w:     MOVE.L  (A4),D0
        BEQ     .wn
        MOVE.L  D0,A0
        CMP.L   46(A0),D4               ; Window->WScreen
        BNE     .wn
        CLR.L   (A4)
        MOVE.L  plat_gfx__ib,A6
        JSR     plat_gfx__CloseWindow(A6)
.wn:    ADDQ.L  #4,A4
        DBRA    D3,.w
        MOVE.L  D4,A0
        MOVE.L  plat_gfx__ib,A6
        JMP     plat_gfx__CloseScreen(A6)
.out:   RTS

plat_gfx__openw:
        MOVEQ   #0,D0
        BSR     plat_gfx__arg
        CMP.L   #plat_gfx__NWIN,D0
        BCC     .out
        MOVE.L  D0,D1
        LSL.L   #2,D1
        LEA     plat_gfx__win,A0
        TST.L   0(A0,D1.L)
        BEQ     plat_gfx__openwindow
        MOVE.L  D0,plat_gfx__cur        ; open: it becomes the current window
        BRA     plat_gfx__state
.out:   RTS

plat_gfx__closew:
        MOVEQ   #0,D0
        BSR     plat_gfx__arg
        CMP.L   #plat_gfx__NWIN,D0
        BCC     .out
        MOVE.L  D0,D1
        LSL.L   #2,D1
        LEA     plat_gfx__win,A0
        MOVE.L  0(A0,D1.L),D2
        BEQ     .out
        CLR.L   0(A0,D1.L)
        CMP.L   plat_gfx__cur,D0        ; the current one: back to window 0 (if open)
        BNE     .cl
        CLR.L   plat_gfx__cur
.cl:    MOVE.L  D2,A0
        MOVE.L  plat_gfx__ib,A6
        JMP     plat_gfx__CloseWindow(A6)
.out:   RTS

plat_gfx__clearw:
        MOVEQ   #0,D0
        BSR     plat_gfx__arg
        CMP.L   #plat_gfx__NWIN,D0
        BCC     .out
        LSL.L   #2,D0
        LEA     plat_gfx__win,A0
        MOVE.L  0(A0,D0.L),D0
        BEQ     .out
        MOVE.L  D0,A1
        MOVE.L  50(A1),A1
        MOVE.L  plat_gfx__gb,A6
        MOVEQ   #0,D0
        JMP     plat_gfx__SetRast(A6)
.out:   RTS

; the string argument as a C string in buf (at most 79 characters) -> A1
plat_gfx__cstr:
        LEA     plat_gfx__buf,A1
        MOVE.L  A1,-(SP)
        MOVE.L  A3,D0
        BEQ     .end
        MOVEQ   #0,D1
        MOVE.W  4(A3),D1
        CMP.L   #79,D1
        BLS     .l
        MOVEQ   #79,D1
.l:     MOVE.L  (A3),A0
        BRA     .n
.c:     MOVE.B  (A0)+,(A1)+
.n:     SUBQ.L  #1,D1
        BPL     .c
.end:   CLR.B   (A1)
        MOVE.L  (SP)+,A1
        RTS

; D0 = window number -> A0 = window or 0 (Z set)
plat_gfx__window:
        CMP.L   #plat_gfx__NWIN,D0
        BCC     .no
        LSL.L   #2,D0
        LEA     plat_gfx__win,A0
        MOVE.L  0(A0,D0.L),D0
        MOVE.L  D0,A0
        RTS
.no:    SUB.L   A0,A0
        MOVEQ   #0,D0
        RTS

plat_gfx__titlew:
        BSR     plat_gfx__cstr
        MOVE.L  A1,-(SP)
        MOVEQ   #0,D0
        BSR     plat_gfx__arg
        BSR     plat_gfx__window
        MOVE.L  (SP)+,A1
        BEQ     .out
        LEA     plat_gfx__wtitle,A2     ; keep the title (Intuition does not copy it)
        MOVE.L  A2,-(SP)
.c:     MOVE.B  (A1)+,(A2)+
        BNE     .c
        MOVE.L  (SP)+,A1
        MOVE.L  #-1,A2                  ; the screen title stays
        MOVE.L  plat_gfx__ib,A6
        JMP     plat_gfx__SetWTitles(A6)
.out:   RTS

plat_gfx__titles:
        BSR     plat_gfx__cstr
        LEA     plat_gfx__stitle,A2
        MOVE.L  A2,-(SP)
.c:     MOVE.B  (A1)+,(A2)+
        BNE     .c
        MOVE.L  (SP)+,A2                ; shown while the current window is active
        MOVE.L  plat_gfx__cur,D0
        BSR     plat_gfx__window
        BEQ     .out
        MOVE.L  #-1,A1
        MOVE.L  plat_gfx__ib,A6
        JMP     plat_gfx__SetWTitles(A6)
.out:   RTS

plat_gfx__fronts:
        MOVE.L  #plat_gfx__ScrToFront,D3
        BRA     plat_gfx__scrop
plat_gfx__backs:
        MOVE.L  #plat_gfx__ScrToBack,D3
plat_gfx__scrop:
        MOVEQ   #0,D0
        BSR     plat_gfx__arg
        CMP.L   #plat_gfx__NSCR,D0
        BCC     .out
        LSL.L   #2,D0
        LEA     plat_gfx__scr,A0
        MOVE.L  0(A0,D0.L),D0
        BEQ     .out
        MOVE.L  D0,A0
        MOVE.L  plat_gfx__ib,A6
        JMP     0(A6,D3.L)
.out:   RTS

plat_gfx__frontw:
        MOVE.L  #plat_gfx__WinToFront,D3
        BRA     plat_gfx__winop
plat_gfx__backw:
        MOVE.L  #plat_gfx__WinToBack,D3
plat_gfx__winop:
        MOVEQ   #0,D0
        BSR     plat_gfx__arg
        BSR     plat_gfx__window
        BEQ     .out
        MOVE.L  plat_gfx__ib,A6
        JMP     0(A6,D3.L)
.out:   RTS

; COLOR front,back,outline (only the given ones)
plat_gfx__color:
        BTST    #0,D6
        BEQ     .b
        MOVEQ   #0,D0
        BSR     plat_gfx__arg
        AND.L   #63,D0
        MOVE.L  D0,plat_gfx__apen
.b:     BTST    #1,D6
        BEQ     .o
        MOVEQ   #1,D0
        BSR     plat_gfx__arg
        AND.L   #63,D0
        MOVE.L  D0,plat_gfx__bpen
.o:     BTST    #2,D6
        BEQ     .set
        MOVEQ   #2,D0
        BSR     plat_gfx__arg
        AND.L   #63,D0
        MOVE.L  D0,plat_gfx__opn
.set:   BRA     plat_gfx__state

; DEFFILL colour,style,pattern: the colour is the drawing pen too (probe23 T3)
plat_gfx__deffill:
        BTST    #0,D6
        BEQ     .out
        MOVEQ   #0,D0
        BSR     plat_gfx__arg
        AND.L   #63,D0
        MOVE.L  D0,plat_gfx__apen
        BRA     plat_gfx__state
.out:   RTS

; SETCOLOR reg,r,g,b / reg,rgb
plat_gfx__setcolor:
        MOVEQ   #0,D0
        BSR     plat_gfx__arg
        AND.L   #63,D0
        MOVE.L  D0,D4                   ; register
        CMP.L   #4,D5
        BLT     .mix
        MOVEQ   #1,D0
        BSR     plat_gfx__arg
        MOVE.L  D0,D1
        MOVEQ   #2,D0
        BSR     plat_gfx__arg
        MOVE.L  D0,D2
        MOVEQ   #3,D0
        BSR     plat_gfx__arg
        MOVE.L  D0,D3
        BRA     .set
.mix:   MOVEQ   #1,D0
        BSR     plat_gfx__arg
        MOVE.L  D0,D3
        AND.L   #15,D3
        MOVE.L  D0,D2
        LSR.L   #4,D2
        AND.L   #15,D2
        MOVE.L  D0,D1
        LSR.L   #8,D1
.set:   AND.L   #15,D1
        AND.L   #15,D2
        AND.L   #15,D3
        MOVE.L  plat_gfx__cur,D0        ; the screen of the current window
        BSR     plat_gfx__window
        BEQ     .out
        MOVE.L  46(A0),A0               ; Window->WScreen
        LEA     44(A0),A0               ; ->ViewPort
        MOVE.L  D4,D0
        MOVE.L  plat_gfx__gb,A6
        JMP     plat_gfx__SetRGB4(A6)
.out:   RTS

plat_gfx__graphmode:
        MOVEQ   #0,D0
        BSR     plat_gfx__arg
        MOVE.L  D0,plat_gfx__mode
        BRA     plat_gfx__state         ; SetDrMd, FRST_DOT

plat_gfx__boundary:
        MOVEQ   #0,D0
        BSR     plat_gfx__arg
        MOVE.L  D0,plat_gfx__bnd
        BRA     plat_gfx__state

; DEFLINE: 0..6 predefined (as brt::Gfx::defline), else the 16-bit pattern
plat_gfx__defline:
        MOVEQ   #0,D0
        BSR     plat_gfx__arg
        CMP.L   #6,D0
        BHI     .own
        ADD.L   D0,D0
        LEA     plat_gfx__lines,A0
        MOVE.W  0(A0,D0.L),D0
.own:   MOVE.W  D0,plat_gfx__pat
        BRA     plat_gfx__state

plat_gfx__plot:
        BSR     plat_gfx__rp
        MOVEQ   #0,D0
        BSR     plat_gfx__arg
        MOVE.L  D0,D4
        MOVEQ   #1,D0
        BSR     plat_gfx__arg
        MOVE.L  D0,D1
        MOVE.L  D4,D0
        MOVEM.L D0-D1/A1,-(SP)
        JSR     plat_gfx__WritePixel(A6)
        MOVEM.L (SP)+,D0-D1/A1
        MOVE.W  D0,36(A1)               ; the pen position directly (cp_x, cp_y): Move would set
        MOVE.W  D1,38(A1)               ; FRST_DOT, the original keeps it (gfx1 on Kickstart 1.2)
        RTS

; D3 = index of the x argument: the pen position (cp_x, cp_y, not Move: that sets FRST_DOT, the
; original does not, probe23 T4) / Draw(rp, x, y)
plat_gfx__moveto:
        MOVE.L  D3,D0
        BSR     plat_gfx__arg
        MOVE.W  D0,36(A1)
        MOVE.L  D3,D0
        ADDQ.L  #1,D0
        BSR     plat_gfx__arg
        MOVE.W  D0,38(A1)
        RTS
plat_gfx__drawto:
        MOVE.L  #plat_gfx__Draw,D2
plat_gfx__xy:
        MOVE.L  D3,D0
        BSR     plat_gfx__arg
        MOVE.L  D0,D4
        MOVE.L  D3,D0
        ADDQ.L  #1,D0
        BSR     plat_gfx__arg
        MOVE.L  D0,D1
        MOVE.L  D4,D0
        MOVE.L  A1,-(SP)
        JSR     0(A6,D2.L)
        MOVE.L  (SP)+,A1
        RTS

plat_gfx__line:
        BSR     plat_gfx__rp
        MOVEQ   #0,D3
        BSR     plat_gfx__moveto
        MOVEQ   #2,D3
        BRA     plat_gfx__drawto

; DRAW x,y TO ... (mode 0: the first point is a PLOT) / DRAW TO x,y ... (mode 1)
plat_gfx__draw:
        MOVEQ   #0,D3
        TST.L   D6
        BNE     .to
        BSR     plat_gfx__plot
        MOVEQ   #2,D3
.to:    BSR     plat_gfx__rp
.l:     MOVE.L  D3,D0
        ADDQ.L  #1,D0
        CMP.L   D5,D0
        BGE     .out
        BSR     plat_gfx__drawto
        ADDQ.L  #2,D3
        BRA     .l
.out:   RTS

; BOX: Move/Draw around (corners in any order)
plat_gfx__box:
        BSR     plat_gfx__rp
        LEA     plat_gfx__quad,A0       ; x1,y1 x2,y1 x2,y2 x1,y2 x1,y1 from the arguments
        MOVE.L  (A2),D0
        MOVE.L  4(A2),D1
        MOVE.L  8(A2),D2
        MOVE.L  12(A2),D3
        MOVEM.L D0-D1,(A0)
        MOVE.L  D2,8(A0)
        MOVE.L  D1,12(A0)
        MOVEM.L D2-D3,16(A0)
        MOVE.L  D0,24(A0)
        MOVE.L  D3,28(A0)
        MOVEM.L D0-D1,32(A0)
        MOVE.L  A2,-(SP)
        LEA     plat_gfx__quad,A2
        MOVEQ   #0,D3
        BSR     plat_gfx__moveto
        MOVEQ   #2,D3
.l:     BSR     plat_gfx__drawto
        ADDQ.L  #2,D3
        CMP.L   #10,D3
        BLT     .l
        MOVE.L  (SP)+,A2
        RTS

; PBOX: RectFill with the corners ordered (the outline by AREAOUTLINE, BOUNDARY)
plat_gfx__pbox:
        BSR     plat_gfx__rp
        MOVE.L  (A2),D0
        MOVE.L  4(A2),D1
        MOVE.L  8(A2),D2
        MOVE.L  12(A2),D3
        CMP.L   D0,D2
        BGE     .x
        EXG     D0,D2
.x:     CMP.L   D1,D3
        BGE     .y
        EXG     D1,D3
.y:     JMP     plat_gfx__RectFill(A6)

; CIRCLE x,y,r: DrawEllipse with ry = 4r/9 (HiRes, probe23)
plat_gfx__circle:
        BSR     plat_gfx__ry
        JMP     plat_gfx__DrawEllipse(A6)
plat_gfx__pcircle:
        BSR     plat_gfx__ry
        BRA     plat_gfx__fill
; ry by the mode of the screen (probe26): LowRes r, HiRes 4r/9, Interlace twice that
plat_gfx__ry:
        BSR     plat_gfx__rp
        MOVE.L  (A2),D0
        MOVE.L  4(A2),D1
        MOVE.L  8(A2),D2
        MOVE.L  D2,D3
        MOVE.L  plat_gfx__cur,D4
        LSL.L   #2,D4
        LEA     plat_gfx__win,A0
        MOVE.L  0(A0,D4.L),A0
        MOVE.L  46(A0),A0               ; Window->WScreen
        MOVE.W  76(A0),D4               ; Screen->ViewPort.Modes
        BTST    #15,D4                  ; HIRES
        BEQ     .lo
        LSL.L   #2,D3
        DIVS    #9,D3
        EXT.L   D3
.lo:    BTST    #2,D4                   ; LACE
        BEQ     .out
        ADD.L   D3,D3
.out:   RTS

plat_gfx__ellipse:
        BSR     plat_gfx__radii
        JMP     plat_gfx__DrawEllipse(A6)
plat_gfx__pellipse:
        BSR     plat_gfx__radii
plat_gfx__fill:
        MOVE.L  A1,-(SP)
        JSR     plat_gfx__AreaEllipse(A6)
        MOVE.L  (SP)+,A1
        JMP     plat_gfx__AreaEnd(A6)
plat_gfx__radii:
        BSR     plat_gfx__rp
        MOVEM.L (A2),D0-D3
        RTS

; POLYLINE: the open line through the points (n-1 lines, probe25 T8); one point: PLOT
plat_gfx__polyline:
        CMP.L   #4,D5
        BLT     .one
        BSR     plat_gfx__rp
        MOVEQ   #0,D3
        BSR     plat_gfx__moveto
        MOVEQ   #2,D3
.l:     MOVE.L  D3,D0
        ADDQ.L  #1,D0
        CMP.L   D5,D0
        BGE     .out
        BSR     plat_gfx__drawto
        ADDQ.L  #2,D3
        BRA     .l
.one:   CMP.L   #2,D5
        BGE     plat_gfx__plot
.out:   RTS

; POLYFILL: AreaMove, AreaDraw, AreaEnd closes and fills (BOUNDARY: AREAOUTLINE)
plat_gfx__polyfill:
        CMP.L   #2,D5
        BLT     .out
        BSR     plat_gfx__rp
        MOVEQ   #0,D3
        MOVE.L  #plat_gfx__AreaMove,D2
.l:     MOVE.L  D3,D0
        ADDQ.L  #1,D0
        CMP.L   D5,D0
        BGE     .end
        MOVE.L  D3,D0
        LSL.L   #2,D0
        MOVE.L  4(A2,D0.L),D1
        MOVE.L  0(A2,D0.L),D0
        MOVE.L  A1,-(SP)
        JSR     0(A6,D2.L)
        MOVE.L  (SP)+,A1
        MOVE.L  #plat_gfx__AreaDraw,D2
        ADDQ.L  #2,D3
        BRA     .l
.end:   JMP     plat_gfx__AreaEnd(A6)
.out:   RTS

; TEXT x,y,s: Move to the base line, Text
plat_gfx__text:
        BSR     plat_gfx__rp
        MOVEQ   #0,D3
        BSR     plat_gfx__moveto
        MOVE.L  A3,D0
        BEQ     .out
        MOVEQ   #0,D0
        MOVE.W  4(A3),D0
        MOVE.L  (A3),A0
        JMP     plat_gfx__Text(A6)
.out:   RTS

;! section data
plat_gfx__intname:
        DC.B    'intuition.library',0
plat_gfx__gfxname:
        DC.B    'graphics.library',0
plat_gfx__title:
        DC.B    'GFA-BASIC',0
        EVEN
plat_gfx__palette:                      ; Kickstart 1.x
        DC.W    $05A,$FFF,$002,$F80
plat_gfx__lines:                        ; DEFLINE 0..6 (as brt::Gfx::defline)
        DC.W    $0000,$FFFF,$CCCC,$AAAA,$FF18,$FF00,$FE10
plat_gfx__geom:                         ; standard windows: x, y, w, h (words) for 0..15
        DC.W    0,11,640,245
        DC.W    0,11,320,122, 320,11,320,122, 0,133,320,122, 320,133,320,122
        DC.W    160,72,320,122, 160,72,320,122, 160,72,320,122, 160,72,320,122, 160,72,320,122
        DC.W    160,72,320,122, 160,72,320,122, 160,72,320,122, 160,72,320,122, 160,72,320,122
        DC.W    160,72,320,122
plat_gfx__ns:                           ; NewScreen
        DC.W    0,0,640,256,2
        DC.B    0,1
        DC.W    $8000,$000F             ; HIRES, CUSTOMSCREEN
        DC.L    0,plat_gfx__title,0,0
plat_gfx__nw:                           ; NewWindow
        DC.W    0,11,640,245
        DC.B    -1,-1
        DC.L    0                       ; IDCMP
        DC.L    $00001402               ; WINDOWDRAG, GIMMEZEROZERO, ACTIVATE
        DC.L    0,0,0,0,0
        DC.W    0,0,640,256,$000F       ; Min/Max, CUSTOMSCREEN
plat_gfx__apen:
        DC.L    1
plat_gfx__bpen:
        DC.L    0
plat_gfx__opn:
        DC.L    1
plat_gfx__mode:
        DC.L    0
plat_gfx__bnd:
        DC.L    0
plat_gfx__pat:
        DC.W    $FFFF
        EVEN

;! section bss
plat_gfx__lscr:
        DS.L    1
plat_gfx__es:
        DS.W    1
plat_gfx__en:
        DS.W    1
plat_gfx__ep:
        DS.L    2
plat_gfx__ccol:
        DS.W    plat_gfx__NWIN
plat_gfx__crow:
        DS.W    plat_gfx__NWIN
plat_gfx__ch:
        DS.W    1
plat_gfx__ib:
        DS.L    1
plat_gfx__gb:
        DS.L    1
plat_gfx__failed:
        DS.L    1
plat_gfx__ras:
        DS.L    1
plat_gfx__cur:
        DS.L    1
plat_gfx__win:
        DS.L    plat_gfx__NWIN
plat_gfx__scr:
        DS.L    plat_gfx__NSCR
plat_gfx__tmpras:
        DS.L    2
plat_gfx__area:
        DS.W    12
plat_gfx__vbuf:
        DS.B    5*plat_gfx__NVEC
plat_gfx__quad:
        DS.L    10
plat_gfx__buf:
        DS.B    80
plat_gfx__wtitle:
        DS.B    80
plat_gfx__stitle:
        DS.B    80
