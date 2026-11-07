;! module    plat_gui
;! provides  plat_gui
;! requires  plat_gfx_window plat_exit_gui
;! group     gui
;! clobbers  D0-D3/A0-A1
;! doc       amiga: requesters and menus (D26 stage 2b) with intuition.library on the screen of the current window.

; ALERT: a window of its own with the text lines and up to 3 buttons (AutoRequest knows only 2);
; Return chooses the default button. FILESELECT and FORM INPUT: a window with a string gadget and
; the buttons OK/Cancel (FILESELECT without a list of the directory yet). MENU m$(): a menu strip of
; Intuition at the current window; MENU x,y sets the flags of the MenuItem (16 ITEMENABLED, 256
; CHECKED, 64 HIGHCOMP; the reference values 80 and 256 are these flags); ON MENU looks for one
; message (MENUPICK) and waits for the next frame if there is none.
; The symbol of ALERT is not shown yet.

; D0 = code (brt::kGuiCommands), D1 = number of numbers, A0 = numbers, D2 = number of strings,
; A1 = descriptor addresses -> D0 = number; A0 = characters and D1 = length of the string result

plat_gui__OpenWindow     EQU -204
plat_gui__CloseWindow    EQU -72
plat_gui__SetMenuStrip   EQU -264
plat_gui__ClearMenuStrip EQU -54
plat_gui__ModifyIDCMP    EQU -150
plat_gui__ActivateGadget EQU -462
plat_gui__GetMsg         EQU -372
plat_gui__ReplyMsg       EQU -378
plat_gui__WaitPort       EQU -384
plat_gui__Move           EQU -240
plat_gui__Text           EQU -60
plat_gui__SetAPen        EQU -342
plat_gui__WaitTOF        EQU -270

plat_gui__MENUS  EQU 16                 ; menus of a strip
plat_gui__MITEMS EQU 32                 ; items per menu (ITEMNUM has 6 bits)
plat_gui__ITEMS  EQU 128                ; items of all menus
plat_gui__ENTRIES EQU 256               ; elements of m$() with an item
plat_gui__POOL   EQU 4096               ; texts of the menu strip
plat_gui__LINES  EQU 8                  ; text lines of ALERT

plat_gui:
        MOVEM.L D2-D7/A2-A6,-(SP)
        MOVE.L  D0,D7                   ; D7 = code
        MOVE.L  D1,D4                   ; D4 = number of numbers
        MOVE.L  D2,D5                   ; D5 = number of strings
        MOVE.L  A0,A2                   ; A2 = numbers
        MOVE.L  A1,A3                   ; A3 = descriptor addresses
        CLR.L   plat_gui__res
        CLR.L   plat_gui__olen
        JSR     plat_gfx_window
        MOVE.L  D0,plat_gui__win
        BEQ     .out                    ; no intuition: nothing happens
        MOVE.L  D1,plat_gui__ib
        MOVE.L  D2,plat_gui__gb
        CMP.L   #8,D7
        BHI     .out
        MOVE.L  D7,D0
        ADD.L   D0,D0
        LEA     plat_gui__table,A0
        MOVE.W  0(A0,D0.L),D0
        JSR     0(A0,D0.W)
.out:   MOVE.L  plat_gui__res,D0
        LEA     plat_gui__obuf,A0
        MOVE.L  plat_gui__olen,D1
        MOVEM.L (SP)+,D2-D7/A2-A6
        RTS

plat_gui__table:
        DC.W    plat_gui__alert-plat_gui__table, plat_gui__fsel-plat_gui__table
        DC.W    plat_gui__form-plat_gui__table, plat_gui__menu-plat_gui__table
        DC.W    plat_gui__mflags-plat_gui__table, plat_gui__mtext-plat_gui__table
        DC.W    plat_gui__mkill-plat_gui__table, plat_gui__onmenu-plat_gui__table
        DC.W    plat_gui__mvalue-plat_gui__table

; ---- arguments ------------------------------------------------------------------------------------

; D0 = k -> D0 = number k (0 if missing)
plat_gui__num:
        CMP.L   D4,D0
        BCC     .z
        LSL.L   #2,D0
        MOVE.L  0(A2,D0.L),D0
        RTS
.z:     MOVEQ   #0,D0
        RTS

; D0 = k -> A0 = characters, D0 = length of string k ("" if missing)
plat_gui__str:
        CMP.L   D5,D0
        BCC     .z
        LSL.L   #2,D0
        MOVE.L  0(A3,D0.L),A0
        MOVEQ   #0,D0
        MOVE.W  4(A0),D0
        MOVE.L  (A0),A0
        RTS
.z:     MOVEQ   #0,D0
        LEA     plat_gui__nul,A0
        RTS

; A0 = characters, D0 = length, A1 = table (8 bytes per part: address, length), D1 = most parts ->
; D0 = number of parts, separated by "|"
plat_gui__split:
        MOVEM.L D2-D3,-(SP)
        MOVEQ   #0,D2
        MOVEQ   #0,D3
        MOVE.L  A0,(A1)
.l:     SUBQ.L  #1,D0
        BMI     .end
        CMP.B   #$7C,(A0)+
        BNE     .c
        MOVE.L  D3,4(A1)
        ADDQ.L  #1,D2
        CMP.L   D1,D2
        BGE     .full
        ADDQ.L  #8,A1
        MOVE.L  A0,(A1)
        MOVEQ   #0,D3
        BRA     .l
.c:     ADDQ.L  #1,D3
        BRA     .l
.end:   MOVE.L  D3,4(A1)
        ADDQ.L  #1,D2
.full:  MOVE.L  D2,D0
        MOVEM.L (SP)+,D2-D3
        RTS

; A0 = characters, D0 = length, A1 = destination, D1 = most characters: a C string
plat_gui__cstr:
        CMP.L   D1,D0
        BLE     .n
        MOVE.L  D1,D0
        BRA     .n
.c:     MOVE.B  (A0)+,(A1)+
.n:     SUBQ.L  #1,D0
        BPL     .c
        CLR.B   (A1)
        RTS

; ---- windows of the requesters --------------------------------------------------------------------

; D0 = width, D1 = height, A0 = first gadget -> D0 = window (0 if it could not open), centred on the
; screen of the current window
plat_gui__open:
        LEA     plat_gui__nw,A1
        MOVE.W  D0,4(A1)
        MOVE.W  D1,6(A1)
        MOVE.L  A0,18(A1)
        MOVE.L  plat_gui__win,A0
        MOVE.L  46(A0),A0               ; Window->WScreen
        MOVE.L  A0,30(A1)
        MOVE.W  12(A0),D2               ; centred
        SUB.W   D0,D2
        BPL     .x
        MOVEQ   #0,D2
.x:     LSR.W   #1,D2
        MOVE.W  D2,(A1)
        MOVE.W  14(A0),D2
        SUB.W   D1,D2
        BPL     .y
        MOVEQ   #0,D2
.y:     LSR.W   #1,D2
        MOVE.W  D2,2(A1)
        MOVE.L  A1,A0
        MOVE.L  plat_gui__ib,A6
        JMP     plat_gui__OpenWindow(A6)

; A4 = window -> D0 = class, D1 = code, A0 = IAddress of the next message (replied)
plat_gui__wait:
.l:     MOVE.L  4,A6
        MOVE.L  86(A4),A0               ; Window->UserPort
        JSR     plat_gui__GetMsg(A6)
        TST.L   D0
        BNE     .got
        MOVE.L  86(A4),A0
        JSR     plat_gui__WaitPort(A6)
        BRA     .l
.got:   MOVE.L  D0,A1
        MOVE.L  20(A1),plat_gui__cls
        MOVEQ   #0,D0
        MOVE.W  24(A1),D0
        MOVE.L  D0,plat_gui__code
        MOVE.L  28(A1),plat_gui__iadr
        JSR     plat_gui__ReplyMsg(A6)
        MOVE.L  plat_gui__cls,D0
        MOVE.L  plat_gui__code,D1
        MOVE.L  plat_gui__iadr,A0
        RTS

; A4 = window, A0 = characters, D0 = length, D1 = y: one line of text, centred
plat_gui__line:
        MOVEM.L D2-D3/A2,-(SP)
        MOVE.L  A0,A2
        MOVE.L  D0,D3
        MOVE.L  D1,D2
        MOVE.L  50(A4),A1               ; Window->RPort
        MOVE.L  plat_gui__gb,A6
        MOVEQ   #1,D0
        JSR     plat_gui__SetAPen(A6)
        MOVE.L  50(A4),A1
        MOVEQ   #0,D0
        MOVE.W  8(A4),D0                ; Window->Width
        MOVE.L  D3,D1
        LSL.L   #3,D1
        SUB.L   D1,D0
        BPL     .x
        MOVEQ   #0,D0
.x:     LSR.L   #1,D0
        MOVE.L  D2,D1
        JSR     plat_gui__Move(A6)
        MOVE.L  50(A4),A1
        MOVE.L  A2,A0
        MOVE.L  D3,D0
        JSR     plat_gui__Text(A6)
        MOVEM.L (SP)+,D2-D3/A2
        RTS

; D0 = button 0..3, D1 = x, D2 = y, D3 = width, A0 = characters, D6 = length -> A0 = its gadget
; (boolean, RELVERIFY, GadgetID = button + 1; framed, the text centred)
plat_gui__button:
        MOVEM.L D4-D5/A2-A3,-(SP)
        MOVE.L  D0,D4
        MOVE.L  A0,A3
        MOVE.L  D4,D0                   ; the text as a C string
        MULU    #40,D0
        LEA     plat_gui__btxt,A1
        ADD.L   D0,A1
        MOVE.L  A1,D5
        MOVE.L  D1,-(SP)                ; x
        MOVE.L  A3,A0
        MOVE.L  D6,D0
        MOVEQ   #39,D1
        BSR     plat_gui__cstr
        MOVE.L  (SP)+,D1
        MOVE.L  D4,D0                   ; frame: 5 points
        MULU    #20,D0
        LEA     plat_gui__xy,A1
        ADD.L   D0,A1
        MOVE.L  A1,A3
        CLR.L   (A1)+
        MOVE.W  D3,D0
        SUBQ.W  #1,D0
        MOVE.W  D0,(A1)+
        CLR.W   (A1)+
        MOVE.W  D0,(A1)+
        MOVE.W  #13,(A1)+
        CLR.W   (A1)+
        MOVE.W  #13,(A1)+
        CLR.L   (A1)
        MOVE.L  D4,D0                   ; border
        LSL.L   #4,D0
        LEA     plat_gui__bord,A1
        ADD.L   D0,A1
        CLR.L   (A1)
        MOVE.L  #$01000005,4(A1)        ; FrontPen 1, BackPen 0, JAM1, 5 points
        MOVE.L  A3,8(A1)
        CLR.L   12(A1)
        MOVE.L  A1,A3
        MOVE.L  D4,D0                   ; IntuiText
        MULU    #20,D0
        LEA     plat_gui__it,A1
        ADD.L   D0,A1
        MOVE.L  #$01000000,(A1)         ; FrontPen 1, BackPen 0, JAM1
        MOVE.L  D6,D0
        CMP.L   #39,D0
        BLE     .w
        MOVEQ   #39,D0
.w:     LSL.L   #3,D0
        NEG.L   D0
        ADD.L   D3,D0
        ASR.L   #1,D0
        MOVE.W  D0,4(A1)
        MOVE.W  #3,6(A1)
        CLR.L   8(A1)
        MOVE.L  D5,12(A1)
        CLR.L   16(A1)
        MOVE.L  A1,D5
        MOVE.L  D4,D0                   ; gadget
        MULU    #44,D0
        LEA     plat_gui__gad,A0
        ADD.L   D0,A0
        CLR.L   (A0)
        MOVE.W  D1,4(A0)
        MOVE.W  D2,6(A0)
        MOVE.W  D3,8(A0)
        MOVE.W  #14,10(A0)
        CLR.W   12(A0)                  ; GADGHCOMP
        MOVE.W  #1,14(A0)               ; RELVERIFY
        MOVE.W  #1,16(A0)               ; BOOLGADGET
        MOVE.L  A3,18(A0)
        CLR.L   22(A0)
        MOVE.L  D5,26(A0)
        CLR.L   30(A0)
        CLR.L   34(A0)
        MOVE.W  D4,D0
        ADDQ.W  #1,D0
        MOVE.W  D0,38(A0)
        CLR.L   40(A0)
        MOVEM.L (SP)+,D4-D5/A2-A3
        RTS

; ---- ALERT symbol,text$,default,buttons$ ----------------------------------------------------------
plat_gui__alert:
        MOVEQ   #1,D0
        BSR     plat_gui__num
        MOVE.L  D0,plat_gui__def
        MOVEQ   #0,D0                   ; the text lines
        BSR     plat_gui__str
        LEA     plat_gui__lines,A1
        MOVEQ   #plat_gui__LINES,D1
        BSR     plat_gui__split
        MOVE.L  D0,plat_gui__nl
        MOVEQ   #1,D0                   ; the buttons
        BSR     plat_gui__str
        LEA     plat_gui__btns,A1
        MOVEQ   #3,D1
        BSR     plat_gui__split
        MOVE.L  D0,plat_gui__nb
        MOVE.L  plat_gui__def,D1        ; Return: the default button, 0: none
        CMP.L   D0,D1
        BLS     .d
        CLR.L   plat_gui__def
.d:     LEA     plat_gui__lines,A0      ; D2 = longest line, D3 = longest button
        MOVE.L  plat_gui__nl,D0
        BSR     plat_gui__longest
        MOVE.L  D1,D2
        LEA     plat_gui__btns,A0
        MOVE.L  plat_gui__nb,D0
        BSR     plat_gui__longest
        MOVE.L  D1,D3
        LSL.L   #3,D3                   ; button width: text + 16, at least 64
        ADD.L   #16,D3
        CMP.L   #64,D3
        BGE     .bw
        MOVEQ   #64,D3
.bw:    MOVE.L  D3,plat_gui__bw
        LSL.L   #3,D2                   ; window width: the lines + 40, the buttons + 24
        ADD.L   #40,D2
        MOVE.L  D3,D0
        ADD.L   #16,D0
        MULU    plat_gui__nb+2,D0
        ADD.L   #24,D0
        CMP.L   D0,D2
        BGE     .ww
        MOVE.L  D0,D2
.ww:    CMP.L   #620,D2
        BLE     .wm
        MOVE.L  #620,D2
.wm:    MOVE.L  D2,plat_gui__w
        MOVE.L  plat_gui__nl,D0         ; height: lines + buttons
        MULU    #10,D0
        ADD.L   #48,D0
        MOVE.L  D0,plat_gui__h
        MOVE.L  plat_gui__bw,D0         ; the buttons side by side, centred
        ADD.L   #16,D0
        MULU    plat_gui__nb+2,D0
        SUB.L   #16,D0
        MOVE.L  plat_gui__w,D1
        SUB.L   D0,D1
        ASR.L   #1,D1
        MOVE.L  D1,plat_gui__x
        MOVEQ   #0,D7                   ; D7 = button
        SUB.L   A5,A5                   ; A5 = previous gadget
.b:     CMP.L   plat_gui__nb,D7
        BGE     .bo
        LEA     plat_gui__btns,A0
        MOVE.L  D7,D0
        LSL.L   #3,D0
        MOVE.L  4(A0,D0.L),D6
        MOVE.L  0(A0,D0.L),A0
        MOVE.L  D7,D0
        MOVE.L  plat_gui__x,D1
        MOVE.L  plat_gui__h,D2
        SUB.L   #24,D2
        MOVE.L  plat_gui__bw,D3
        BSR     plat_gui__button
        MOVE.L  A5,D0
        BEQ     .first
        MOVE.L  A0,(A5)
.first: MOVE.L  A0,A5
        MOVE.L  plat_gui__bw,D0
        ADD.L   #16,D0
        ADD.L   D0,plat_gui__x
        ADDQ.L  #1,D7
        BRA     .b
.bo:    MOVE.L  plat_gui__w,D0
        MOVE.L  plat_gui__h,D1
        LEA     plat_gui__gad,A0
        BSR     plat_gui__open
        MOVE.L  D0,A4
        TST.L   D0
        BEQ     .def
        MOVEQ   #0,D7                   ; the lines
.t:     CMP.L   plat_gui__nl,D7
        BGE     .ev
        LEA     plat_gui__lines,A0
        MOVE.L  D7,D0
        LSL.L   #3,D0
        MOVE.L  4(A0,D0.L),D1
        MOVE.L  0(A0,D0.L),A0
        MOVE.L  D1,D0
        MOVE.L  D7,D1
        MULU    #10,D1
        ADD.L   #16,D1
        BSR     plat_gui__line
        ADDQ.L  #1,D7
        BRA     .t
.ev:    BSR     plat_gui__wait
        CMP.L   #$40,D0                 ; GADGETUP: GadgetID = button
        BNE     .key
        MOVEQ   #0,D0
        MOVE.W  38(A0),D0
        BRA     .done
.key:   CMP.L   #$400,D0                ; RAWKEY Return / Enter: the default button
        BNE     .ev
        CMP.L   #$44,D1
        BEQ     .ret
        CMP.L   #$43,D1
        BNE     .ev
.ret:   MOVE.L  plat_gui__def,D0
        BEQ     .ev
.done:  MOVE.L  D0,plat_gui__res
        MOVE.L  A4,A0
        MOVE.L  plat_gui__ib,A6
        JMP     plat_gui__CloseWindow(A6)
.def:   MOVE.L  plat_gui__def,D0        ; no window: the default button, else 1
        BNE     .dd
        MOVEQ   #1,D0
.dd:    MOVE.L  D0,plat_gui__res
        RTS

; A0 = table of parts, D0 = their number -> D1 = the longest length
plat_gui__longest:
        MOVEQ   #0,D1
        BRA     .n
.l:     CMP.L   4(A0),D1
        BGE     .s
        MOVE.L  4(A0),D1
.s:     ADDQ.L  #8,A0
.n:     SUBQ.L  #1,D0
        BPL     .l
        RTS

; ---- FILESELECT title$,ok$,path$ / FORM INPUT n,old$ ----------------------------------------------
plat_gui__fsel:
        MOVEQ   #0,D0                   ; title
        BSR     plat_gui__str
        MOVE.L  A0,plat_gui__tptr
        MOVE.L  D0,plat_gui__tlen
        MOVEQ   #1,D0                   ; text of the OK button
        BSR     plat_gui__str
        MOVE.L  A0,plat_gui__okp
        MOVE.L  D0,plat_gui__okl
        BNE     .ok
        LEA     plat_gui__oktxt,A0
        MOVE.L  A0,plat_gui__okp
        MOVEQ   #2,D0
        MOVE.L  D0,plat_gui__okl
.ok:    MOVEQ   #2,D0                   ; the path is the start
        BSR     plat_gui__str
        MOVE.L  #255,D1
        BSR     plat_gui__ask
        TST.L   D0
        BNE     .take
        CLR.L   plat_gui__olen          ; cancelled: ""
        RTS
.take:  BRA     plat_gui__result

plat_gui__form:
        MOVEQ   #0,D0
        BSR     plat_gui__num
        MOVE.L  D0,D6
        CMP.L   #1,D6
        BGE     .lo
        MOVEQ   #1,D6
.lo:    CMP.L   #255,D6
        BLE     .hi
        MOVE.L  #255,D6
.hi:    CLR.L   plat_gui__tlen
        LEA     plat_gui__oktxt,A0
        MOVE.L  A0,plat_gui__okp
        MOVEQ   #2,D0
        MOVE.L  D0,plat_gui__okl
        MOVEQ   #0,D0                   ; the old text
        BSR     plat_gui__str
        MOVE.L  D6,D1
        BSR     plat_gui__ask
        TST.L   D0
        BNE     .take
        MOVEQ   #0,D0                   ; cancelled: the old text stays
        BSR     plat_gui__str
        LEA     plat_gui__obuf,A1
        MOVE.L  D0,plat_gui__olen
        BRA     .n
.c:     MOVE.B  (A0)+,(A1)+
.n:     SUBQ.L  #1,D0
        BPL     .c
        RTS
.take:  BRA     plat_gui__result

; the text of the string gadget as the result
plat_gui__result:
        LEA     plat_gui__sbuf,A0
        LEA     plat_gui__obuf,A1
        MOVEQ   #0,D0
.c:     MOVE.B  (A0)+,D1
        BEQ     .e
        MOVE.B  D1,(A1)+
        ADDQ.L  #1,D0
        BRA     .c
.e:     MOVE.L  D0,plat_gui__olen
        RTS

; A0 = characters, D0 = length of the start text, D1 = most characters; plat_gui__tptr/tlen = title
; line, plat_gui__okp/okl = text of the OK button -> D0 = 1 OK (text in plat_gui__sbuf), 0 cancelled
plat_gui__ask:
        MOVE.L  D1,D6
        LEA     plat_gui__sbuf,A1       ; empty first: the gadget shows nothing of an earlier text
        MOVEQ   #63,D1
.clr:   CLR.L   (A1)+
        DBRA    D1,.clr
        MOVE.L  D6,D1
        LEA     plat_gui__sbuf,A1       ; start text
        BSR     plat_gui__cstr
        MOVE.L  A1,D0                   ; the cursor after it
        SUB.L   #plat_gui__sbuf,D0
        CLR.B   plat_gui__ubuf
        LEA     plat_gui__si,A1         ; StringInfo
        MOVE.L  #plat_gui__sbuf,(A1)
        MOVE.L  #plat_gui__ubuf,4(A1)
        MOVE.W  D0,8(A1)                ; BufferPos
        MOVE.W  D6,D0
        ADDQ.W  #1,D0
        MOVE.W  D0,10(A1)               ; MaxChars with the 0 byte
        CLR.L   12(A1)
        MOVE.W  8(A1),16(A1)            ; NumChars
        CLR.W   18(A1)
        CLR.L   20(A1)
        CLR.L   24(A1)
        CLR.L   28(A1)
        CLR.L   32(A1)
        MOVE.L  #$FFFEFFFE,plat_gui__xy+60     ; frame of the string gadget: (-2,-2) .. (361,9)
        MOVE.W  #361,plat_gui__xy+64
        MOVE.W  #-2,plat_gui__xy+66
        MOVE.W  #361,plat_gui__xy+68
        MOVE.W  #9,plat_gui__xy+70
        MOVE.W  #-2,plat_gui__xy+72
        MOVE.W  #9,plat_gui__xy+74
        MOVE.L  #$FFFEFFFE,plat_gui__xy+76
        LEA     plat_gui__bord+48,A1
        CLR.L   (A1)
        MOVE.L  #$01000005,4(A1)
        MOVE.L  #plat_gui__xy+60,8(A1)
        CLR.L   12(A1)
        LEA     plat_gui__gad+132,A0    ; gadget 3: the string
        CLR.L   (A0)
        MOVE.W  #20,4(A0)
        MOVE.W  #30,6(A0)
        MOVE.W  #360,8(A0)
        MOVE.W  #8,10(A0)
        CLR.W   12(A0)
        MOVE.W  #1,14(A0)               ; RELVERIFY: Return ends it
        MOVE.W  #4,16(A0)               ; STRGADGET
        MOVE.L  A1,18(A0)
        CLR.L   22(A0)
        CLR.L   26(A0)
        CLR.L   30(A0)
        MOVE.L  #plat_gui__si,34(A0)
        MOVE.W  #10,38(A0)
        CLR.L   40(A0)
        MOVEQ   #0,D0                   ; OK
        MOVEQ   #20,D1
        MOVEQ   #50,D2
        MOVEQ   #96,D3
        MOVE.L  plat_gui__okp,A0
        MOVE.L  D6,-(SP)
        MOVE.L  plat_gui__okl,D6
        BSR     plat_gui__button
        MOVE.L  A0,A5
        MOVEQ   #1,D0                   ; Cancel
        MOVE.L  #284,D1
        MOVEQ   #50,D2
        MOVEQ   #96,D3
        LEA     plat_gui__cantxt,A0
        MOVEQ   #6,D6
        BSR     plat_gui__button
        MOVE.L  (SP)+,D6
        MOVE.L  A0,(A5)
        LEA     plat_gui__gad+132,A1
        MOVE.L  A1,(A0)
        MOVE.L  #400,D0
        MOVEQ   #74,D1
        LEA     plat_gui__gad,A0
        BSR     plat_gui__open
        MOVE.L  D0,A4
        TST.L   D0
        BEQ     .no
        MOVE.L  plat_gui__tptr,A0       ; the title line
        MOVE.L  plat_gui__tlen,D0
        BEQ     .act
        MOVEQ   #18,D1
        BSR     plat_gui__line
.act:   LEA     plat_gui__gad+132,A0    ; the cursor into the string gadget
        MOVE.L  A4,A1
        MOVE.L  A2,-(SP)                ; A2 = the numbers of the command
        SUB.L   A2,A2
        MOVE.L  plat_gui__ib,A6
        JSR     plat_gui__ActivateGadget(A6)
        MOVE.L  (SP)+,A2
.ev:    BSR     plat_gui__wait
        CMP.L   #$40,D0
        BNE     .ev
        MOVE.W  38(A0),D0
        MOVEQ   #1,D7                   ; OK (1) and Return in the field (10)
        CMP.W   #2,D0
        BNE     .cl
        MOVEQ   #0,D7
.cl:    MOVE.L  A4,A0
        MOVE.L  plat_gui__ib,A6
        JSR     plat_gui__CloseWindow(A6)
        MOVE.L  D7,D0
        RTS
.no:    MOVEQ   #0,D0
        RTS

; ---- menus --------------------------------------------------------------------------------------------

; ClearMenuStrip if a strip is shown
plat_gui__unset:
        MOVE.L  plat_gui__mwin,D0
        BEQ     .out
        MOVE.L  D0,A0
        MOVE.L  plat_gui__ib,A6
        JSR     plat_gui__ClearMenuStrip(A6)
.out:   RTS

; SetMenuStrip again
plat_gui__reset:
        MOVE.L  plat_gui__mwin,D0
        BEQ     .out
        MOVE.L  D0,A0
        LEA     plat_gui__menus,A1
        MOVE.L  plat_gui__ib,A6
        JSR     plat_gui__SetMenuStrip(A6)
.out:   RTS

; A0 = characters, D0 = length -> A1 = the text as a C string in the pool (an empty one if it is full)
plat_gui__pooled:
        MOVE.L  plat_gui__pptr,A1
        MOVE.L  A1,D1
        ADD.L   D0,D1
        ADDQ.L  #1,D1
        CMP.L   #plat_gui__pool+plat_gui__POOL,D1
        BLS     .ok
        LEA     plat_gui__nul,A1
        RTS
.ok:    MOVE.L  D1,plat_gui__pptr
        MOVE.L  A1,-(SP)
        MOVE.L  #255,D1
        BSR     plat_gui__cstr
        MOVE.L  (SP)+,A1
        RTS

; MENU m$(): per menu a title, its entries, ""; one more "" ends it (as brt::Gui::menu)
plat_gui__menu:
        BSR     plat_gui__unset
        CLR.L   plat_gui__mwin
        MOVE.L  #-1,plat_gui__chosen
        MOVE.L  #plat_gui__pool,plat_gui__pptr
        LEA     plat_gui__item,A0       ; no items, no map
        MOVE.W  #plat_gui__ENTRIES-1,D0
.ci:    CLR.L   (A0)+
        DBRA    D0,.ci
        LEA     plat_gui__map,A0
        MOVE.W  #plat_gui__MENUS*plat_gui__MITEMS-1,D0
.cm:    CLR.W   (A0)+
        DBRA    D0,.cm
        MOVEQ   #-1,D2                  ; D2 = menu (-1: none yet)
        MOVEQ   #0,D3                   ; D3 = items in all menus
        MOVEQ   #1,D6                   ; D6 = a title comes next
        MOVEQ   #0,D7                   ; D7 = element k
        SUB.L   A4,A4                   ; A4 = current menu
        SUB.L   A5,A5                   ; A5 = its last item
        CLR.L   plat_gui__mx
.k:     CMP.L   D5,D7
        BGE     .end
        MOVE.L  D7,D0
        BSR     plat_gui__str
        TST.L   D0
        BNE     .text
        TST.L   D6                      ; "" where a title belongs: the end
        BNE     .end
        BSR     plat_gui__width
        MOVEQ   #1,D6
        BRA     .next
.text:  TST.L   D6
        BEQ     .it
        CMP.L   #plat_gui__MENUS-1,D2   ; a new menu
        BGE     .end
        ADDQ.L  #1,D2
        MOVE.L  D0,D1
        MOVE.L  D2,D0
        MULU    #30,D0
        LEA     plat_gui__menus,A1
        ADD.L   D0,A1
        MOVE.L  A4,D0
        BEQ     .m1
        MOVE.L  A1,(A4)                 ; NextMenu of the previous one
.m1:    MOVE.L  A1,A4
        CLR.L   (A4)
        MOVE.L  plat_gui__mx,D0
        MOVE.W  D0,4(A4)
        CLR.W   6(A4)
        MOVE.L  D1,D0                   ; width: text + 16
        LSL.L   #3,D0
        ADD.L   #16,D0
        MOVE.W  D0,8(A4)
        ADD.L   D0,plat_gui__mx
        MOVE.W  #10,10(A4)
        MOVE.W  #1,12(A4)               ; MENUENABLED
        MOVE.L  D1,D0
        BSR     plat_gui__pooled
        MOVE.L  A1,14(A4)
        CLR.L   18(A4)
        CLR.L   22(A4)
        CLR.L   26(A4)
        SUB.L   A5,A5
        CLR.L   plat_gui__mi
        CLR.L   plat_gui__ml
        MOVEQ   #0,D6
        BRA     .next
.it:    CMP.L   #plat_gui__ITEMS,D3     ; an item
        BGE     .next
        MOVE.L  plat_gui__mi,D1
        CMP.L   #plat_gui__MITEMS,D1
        BGE     .next
        CMP.L   plat_gui__ml,D0
        BLE     .ml
        MOVE.L  D0,plat_gui__ml
.ml:    MOVE.L  A0,-(SP)
        MOVE.L  D0,-(SP)
        MOVE.L  D3,D0
        MULU    #34,D0
        LEA     plat_gui__items,A1
        ADD.L   D0,A1
        MOVE.L  A5,D0
        BEQ     .i1
        MOVE.L  A1,(A5)                 ; NextItem of the previous one
        BRA     .i2
.i1:    MOVE.L  A1,18(A4)               ; FirstItem
.i2:    MOVE.L  A1,A5
        CLR.L   (A5)
        CLR.W   4(A5)
        MOVE.L  plat_gui__mi,D0
        MULU    #10,D0
        MOVE.W  D0,6(A5)
        MOVE.W  #10,10(A5)
        MOVE.W  #83,12(A5)              ; CHECKIT ITEMTEXT ITEMENABLED HIGHCOMP
        MOVE.L  4(SP),A0
        CMP.B   #$2D,(A0)               ; "-": not selectable
        BNE     .en
        MOVE.W  #67,12(A5)
.en:    CLR.L   14(A5)
        MOVE.L  D3,D0
        MULU    #20,D0
        LEA     plat_gui__mits,A1
        ADD.L   D0,A1
        MOVE.L  A1,18(A5)               ; ItemFill
        CLR.L   22(A5)
        CLR.W   26(A5)
        CLR.L   28(A5)
        CLR.W   32(A5)
        MOVE.L  #$00010000,(A1)         ; FrontPen 0, BackPen 1, JAM1
        MOVE.W  #20,4(A1)               ; after the check mark
        MOVE.W  #1,6(A1)
        CLR.L   8(A1)
        CLR.L   16(A1)
        MOVE.L  A1,-(SP)
        MOVE.L  8(SP),A0
        MOVE.L  4(SP),D0
        BSR     plat_gui__pooled
        MOVE.L  (SP)+,A0
        MOVE.L  A1,12(A0)               ; IText
        ADDQ.L  #8,SP
        MOVE.L  D2,D0                   ; map: menu, item -> k + 1; item of k
        LSL.L   #5,D0
        ADD.L   plat_gui__mi,D0
        ADD.L   D0,D0
        LEA     plat_gui__map,A0
        MOVE.W  D7,D1
        ADDQ.W  #1,D1
        MOVE.W  D1,0(A0,D0.L)
        CMP.L   #plat_gui__ENTRIES,D7
        BGE     .ni
        MOVE.L  D7,D0
        LSL.L   #2,D0
        LEA     plat_gui__item,A0
        MOVE.L  A5,0(A0,D0.L)
.ni:    ADDQ.L  #1,plat_gui__mi
        ADDQ.L  #1,D3
.next:  ADDQ.L  #1,D7
        BRA     .k
.end:   TST.L   D6
        BNE     .set
        BSR     plat_gui__width
.set:   TST.L   D2
        BMI     .out
        MOVE.L  plat_gui__win,plat_gui__mwin
        BSR     plat_gui__reset
        MOVE.L  plat_gui__mwin,A0       ; the window reports MENUPICK
        MOVE.L  #$100,D0
        MOVE.L  plat_gui__ib,A6
        JSR     plat_gui__ModifyIDCMP(A6)
        MOVE.L  #plat_gui__close,plat_exit_gui
.out:   RTS

; the items of the current menu (A4) get the width of the longest text
plat_gui__width:
        MOVE.L  A4,D0
        BEQ     .out
        MOVE.L  plat_gui__ml,D1
        LSL.L   #3,D1
        ADD.L   #28,D1
        MOVE.L  18(A4),D0
.l:     TST.L   D0
        BEQ     .out
        MOVE.L  D0,A0
        MOVE.W  D1,8(A0)
        MOVE.L  (A0),D0
        BRA     .l
.out:   RTS

; D0 = k -> A0 = its MenuItem, Z set if there is none
plat_gui__entry:
        CMP.L   #plat_gui__ENTRIES,D0
        BCC     .none
        LSL.L   #2,D0
        LEA     plat_gui__item,A0
        MOVE.L  0(A0,D0.L),D0
        MOVE.L  D0,A0
        RTS
.none:  MOVEQ   #0,D0
        RTS

; MENU x,flags: the flags of the item (with ITEMTEXT and CHECKIT)
plat_gui__mflags:
        MOVEQ   #0,D0
        BSR     plat_gui__num
        BSR     plat_gui__entry
        BEQ     .out
        MOVE.L  A0,A4
        BSR     plat_gui__unset
        MOVEQ   #1,D0
        BSR     plat_gui__num
        OR.W    #3,D0
        MOVE.W  D0,12(A4)
        BRA     plat_gui__reset
.out:   RTS

; MENU x,text$: the new text, not longer than the old one
plat_gui__mtext:
        MOVEQ   #0,D0
        BSR     plat_gui__num
        BSR     plat_gui__entry
        BEQ     .out
        MOVE.L  18(A0),A0               ; ItemFill -> IText
        MOVE.L  12(A0),A4
        BSR     plat_gui__unset
        MOVEQ   #-1,D1                  ; D1 = length of the old text
        MOVE.L  A4,A0
.len:   ADDQ.L  #1,D1
        TST.B   (A0)+
        BNE     .len
        MOVEQ   #1,D0
        BSR     plat_gui__str
        MOVE.L  A4,A1
        BSR     plat_gui__cstr
        BRA     plat_gui__reset
.out:   RTS

plat_gui__mkill:
        BSR     plat_gui__unset
        CLR.L   plat_gui__mwin
        MOVE.L  #-1,plat_gui__chosen
        RTS

; ON MENU: one message of the menu window; none: wait for the next frame
plat_gui__onmenu:
        MOVE.L  plat_gui__mwin,D0
        BEQ     .tick
        MOVE.L  D0,A4
        MOVE.L  4,A6
        MOVE.L  86(A4),A0
        JSR     plat_gui__GetMsg(A6)
        TST.L   D0
        BEQ     .tick
        MOVE.L  D0,A1
        MOVE.L  20(A1),D2               ; class
        MOVEQ   #0,D3
        MOVE.W  24(A1),D3               ; code
        JSR     plat_gui__ReplyMsg(A6)
        CMP.L   #$100,D2                ; MENUPICK
        BNE     .out
        CMP.W   #$FFFF,D3               ; MENUNULL
        BEQ     .out
        MOVE.L  D3,D0                   ; menu (5 bits), item (6 bits)
        AND.L   #31,D0
        CMP.L   #plat_gui__MENUS,D0
        BCC     .out
        LSR.L   #5,D3
        AND.L   #63,D3
        CMP.L   #plat_gui__MITEMS,D3
        BCC     .out
        LSL.L   #5,D0
        ADD.L   D3,D0
        ADD.L   D0,D0
        LEA     plat_gui__map,A0
        MOVEQ   #0,D1
        MOVE.W  0(A0,D0.L),D1
        BEQ     .out
        SUBQ.L  #1,D1
        MOVE.L  D1,plat_gui__chosen
        MOVE.L  #256,plat_gui__mcls
        MOVE.L  #1,plat_gui__res
.out:   RTS
.tick:  MOVE.L  plat_gui__gb,A6
        JMP     plat_gui__WaitTOF(A6)

; MENU(x): 0 the chosen entry, 1 the class of the message
plat_gui__mvalue:
        MOVEQ   #0,D0
        BSR     plat_gui__num
        TST.L   D0
        BNE     .one
        MOVE.L  plat_gui__chosen,plat_gui__res
        RTS
.one:   CMP.L   #1,D0
        BNE     .out
        MOVE.L  plat_gui__mcls,plat_gui__res
.out:   RTS

; at the end of the program (plat_exit_gui): the strip away before the window closes
plat_gui__close:
        MOVE.L  A6,-(SP)
        BSR     plat_gui__unset
        CLR.L   plat_gui__mwin
        MOVE.L  (SP)+,A6
        RTS

;! section data
plat_gui__nw:                           ; NewWindow of the requesters
        DC.W    0,0,200,60
        DC.B    -1,-1
        DC.L    $00000440               ; GADGETUP, RAWKEY
        DC.L    $00031000               ; ACTIVATE, RMBTRAP, NOCAREREFRESH
        DC.L    0,0,0,0,0
        DC.W    0,0,640,256,$000F       ; CUSTOMSCREEN
plat_gui__chosen:
        DC.L    -1
plat_gui__nul:
        DC.B    0
plat_gui__oktxt:
        DC.B    'OK',0
plat_gui__cantxt:
        DC.B    'Cancel',0
        EVEN

;! section bss
plat_gui__res:
        DS.L    1
plat_gui__olen:
        DS.L    1
plat_gui__win:
        DS.L    1
plat_gui__ib:
        DS.L    1
plat_gui__gb:
        DS.L    1
plat_gui__cls:
        DS.L    1
plat_gui__code:
        DS.L    1
plat_gui__iadr:
        DS.L    1
plat_gui__def:
        DS.L    1
plat_gui__nl:
        DS.L    1
plat_gui__nb:
        DS.L    1
plat_gui__bw:
        DS.L    1
plat_gui__w:
        DS.L    1
plat_gui__h:
        DS.L    1
plat_gui__x:
        DS.L    1
plat_gui__tptr:
        DS.L    1
plat_gui__tlen:
        DS.L    1
plat_gui__okp:
        DS.L    1
plat_gui__okl:
        DS.L    1
plat_gui__mwin:
        DS.L    1
plat_gui__mcls:
        DS.L    1
plat_gui__mx:
        DS.L    1
plat_gui__mi:
        DS.L    1
plat_gui__ml:
        DS.L    1
plat_gui__pptr:
        DS.L    1
plat_gui__lines:
        DS.L    2*plat_gui__LINES
plat_gui__btns:
        DS.L    2*3
plat_gui__gad:
        DS.B    4*44
plat_gui__bord:
        DS.B    4*16
plat_gui__xy:
        DS.B    4*20
plat_gui__it:
        DS.B    4*20
plat_gui__btxt:
        DS.B    4*40
plat_gui__si:
        DS.B    36
plat_gui__sbuf:
        DS.B    256
plat_gui__ubuf:
        DS.B    256
plat_gui__obuf:
        DS.B    256
plat_gui__menus:
        DS.B    plat_gui__MENUS*30
plat_gui__items:
        DS.B    plat_gui__ITEMS*34
plat_gui__mits:
        DS.B    plat_gui__ITEMS*20
plat_gui__map:
        DS.W    plat_gui__MENUS*plat_gui__MITEMS
plat_gui__item:
        DS.L    plat_gui__ENTRIES
plat_gui__pool:
        DS.B    plat_gui__POOL
