;! module    plat_gfx
;! provides  plat_gfx plat_gfx_point
;! requires  plat_exit_gfx
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
.def:   MOVE.L  plat_gfx__ib,A6
        JSR     plat_gfx__OpenScreen(A6)
        LEA     plat_gfx__scr,A0
        MOVE.L  D4,D1
        LSL.L   #2,D1
        MOVE.L  D0,0(A0,D1.L)
        BEQ     .out
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
        MOVEQ   #0,D3                   ; D3 = screen number
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
plat_gfx__ry:
        BSR     plat_gfx__rp
        MOVE.L  (A2),D0
        MOVE.L  4(A2),D1
        MOVE.L  8(A2),D2
        MOVE.L  D2,D3
        LSL.L   #2,D3
        DIVS    #9,D3
        EXT.L   D3
        RTS

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
