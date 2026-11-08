;! module    rt_gfx
;! provides  rt_gfx_arg rt_gfx_str rt_gfx_cmd rt_gfx_point
;! requires  plat_gfx_plot plat_gfx_move plat_gfx_draw plat_gfx_test plat_gfx_pen plat_gfx_paper plat_gfx_mode
;! requires  plat_gfx_ink plat_gfx_text plat_gfx_clear plat_gfx_lmask zi_add zi_sub zi_mulv
;! group     graphics
;! clobbers  AF BC DE HL
;! doc       Graphics commands (D25): collects the arguments, draws lines, boxes, ellipses through the platform.

; The arguments are kept as 16-bit values (limited to -32768..32767), in GFA coordinates; the
; platform maps them to the screen (plat_gfx_*: DE = x, HL = y). One pen for COLOR and DEFFILL
; (probe23 T3); BOUNDARY draws the outline of PBOX/PCIRCLE/PELLIPSE in the outline pen (COLOR's
; third value). POLYLINE/POLYFILL take up to 127 points. Ellipses: Kennedy's integer algorithm (32-bit cells, radii limited to 1000);
; CIRCLE x,y,r has ry = 4r/9 (HiRes, D25). Windows and screens do not exist: OPENW, CLOSEW, TITLEW,
; OPENS, ... do nothing, CLEARW clears the graphics. DEFLINE: 0..6 the predefined patterns, else
; the 16 bits (D25); filled shapes are solid.

rt_gfx__MAX EQU 254                     ; POLYLINE/POLYFILL: up to 127 points

; DE -> next numeric argument (32 bits); its low 16 bits also unlimited in rt_gfx__raw (DEFLINE)
rt_gfx_arg:
        LD      A,(DE)
        LD      (rt_gfx__raw),A
        INC     DE
        LD      A,(DE)
        LD      (rt_gfx__raw+1),A
        DEC     DE
        LD      A,(rt_gfx__n)
        CP      rt_gfx__MAX
        RET     NC
        INC     A
        LD      (rt_gfx__n),A
        DEC     A
        LD      L,A
        LD      H,0
        ADD     HL,HL
        LD      BC,rt_gfx__args
        ADD     HL,BC
        PUSH    HL
        CALL    rt_gfx__clamp           ; DE = value
        POP     HL
        LD      (HL),E
        INC     HL
        LD      (HL),D
        RET

; DE -> 32-bit value -> DE = it limited to -32768..32767
rt_gfx__clamp:
        EX      DE,HL
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        INC     HL
        LD      A,(HL)
        INC     HL
        LD      H,(HL)
        LD      L,A                     ; HL = high word
        LD      A,D
        RLCA
        SBC     A,A                     ; A = the sign of DE as a byte
        CP      L
        JR      NZ,rt_gfx__cl
        CP      H
        RET     Z                       ; high word = sign extension: fits
rt_gfx__cl:
        BIT     7,H
        LD      DE,32767
        RET     Z
        LD      DE,-32768
        RET

; DE = descriptor of the string argument (TEXT)
rt_gfx_str:
        LD      (rt_gfx__s),DE
        RET

; A = k -> DE = argument k (0 if it was not given)
rt_gfx__a:
        LD      HL,rt_gfx__n
        CP      (HL)
        LD      DE,0
        RET     NC
        LD      L,A
        LD      H,0
        ADD     HL,HL
        LD      DE,rt_gfx__args
        ADD     HL,DE
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        RET

; DE -> command code (brt::kGfxCommands), BC -> mode (arguments given; DRAW: 1 = DRAW TO)
rt_gfx_cmd:
        LD      A,(BC)
        LD      (rt_gfx__mode),A
        LD      A,(DE)
        CP      rt_gfx__NCMD
        JR      NC,rt_gfx__end
        ADD     A,A
        LD      L,A
        LD      H,0
        LD      DE,rt_gfx__tab
        ADD     HL,DE
        LD      A,(HL)
        INC     HL
        LD      H,(HL)
        LD      L,A
        CALL    rt_gfx__jp
rt_gfx__end:
        XOR     A
        LD      (rt_gfx__n),A
        RET
rt_gfx__jp:
        JP      (HL)

rt_gfx__tab:                            ; in the order of brt::kGfxCommands
        DW      rt_gfx__nop             ; OPENS
        DW      rt_gfx__nop             ; CLOSES
        DW      rt_gfx__nop             ; OPENW
        DW      rt_gfx__nop             ; CLOSEW
        DW      plat_gfx_clear          ; CLEARW
        DW      rt_gfx__nop             ; TITLEW
        DW      rt_gfx__nop             ; TITLES
        DW      rt_gfx__nop             ; FRONTS
        DW      rt_gfx__nop             ; BACKS
        DW      rt_gfx__nop             ; FRONTW
        DW      rt_gfx__nop             ; BACKW
        DW      rt_gfx__color           ; COLOR
        DW      rt_gfx__setcolor        ; SETCOLOR
        DW      rt_gfx__graphmode       ; GRAPHMODE
        DW      rt_gfx__deffill         ; DEFFILL
        DW      rt_gfx__boundary        ; BOUNDARY
        DW      rt_gfx__defline         ; DEFLINE
        DW      rt_gfx__plot            ; PLOT
        DW      rt_gfx__line            ; LINE
        DW      rt_gfx__drawc           ; DRAW
        DW      rt_gfx__box             ; BOX
        DW      rt_gfx__pbox            ; PBOX
        DW      rt_gfx__circle          ; CIRCLE
        DW      rt_gfx__pcircle         ; PCIRCLE
        DW      rt_gfx__ellipse         ; ELLIPSE
        DW      rt_gfx__pellipse        ; PELLIPSE
        DW      rt_gfx__text            ; TEXT
        DW      plat_gfx_clear          ; CLS
        DW      rt_gfx__polyline        ; POLYLINE
        DW      rt_gfx__polyfill        ; POLYFILL
rt_gfx__NCMD EQU 30

rt_gfx__nop:
        RET

; ---- pens and modes ---------------------------------------------------------------------------------

; COLOR front,back,outline (each one only if given)
rt_gfx__color:
        LD      A,(rt_gfx__mode)
        BIT     0,A
        JR      Z,rt_gfx__c1
        XOR     A
        CALL    rt_gfx__a
        LD      A,E
        CALL    rt_gfx__setpen
rt_gfx__c1:
        LD      A,(rt_gfx__mode)
        BIT     1,A
        JR      Z,rt_gfx__c2
        LD      A,1
        CALL    rt_gfx__a
        LD      A,E
        LD      (rt_gfx__bpen),A
        CALL    plat_gfx_paper
rt_gfx__c2:
        LD      A,(rt_gfx__mode)
        BIT     2,A
        RET     Z
        LD      A,2
        CALL    rt_gfx__a
        LD      A,E
        LD      (rt_gfx__opn),A
        RET

; DEFFILL colour: the same pen as COLOR (probe23 T3); style and pattern are not supported
rt_gfx__deffill:
        LD      A,(rt_gfx__mode)
        BIT     0,A
        RET     Z
        XOR     A
        CALL    rt_gfx__a
        LD      A,E
rt_gfx__setpen:
        LD      (rt_gfx__apen),A
        JP      plat_gfx_pen

; SETCOLOR reg,r,g,b or reg,rgb (0RGB, 4 bits each)
rt_gfx__setcolor:
        LD      A,(rt_gfx__n)
        CP      4
        JR      NC,rt_gfx__sc4
        LD      A,1
        CALL    rt_gfx__a
        EX      DE,HL                   ; HL = rgb
        JR      rt_gfx__sc
rt_gfx__sc4:
        LD      A,1                     ; r
        CALL    rt_gfx__a
        LD      A,E
        AND     15
        LD      H,A
        LD      A,2                     ; g
        CALL    rt_gfx__a
        LD      A,E
        AND     15
        RLCA
        RLCA
        RLCA
        RLCA
        LD      L,A
        LD      A,3                     ; b
        CALL    rt_gfx__a
        LD      A,E
        AND     15
        OR      L
        LD      L,A
rt_gfx__sc:
        PUSH    HL
        XOR     A
        CALL    rt_gfx__a
        POP     HL
        LD      A,E
        JP      plat_gfx_ink

rt_gfx__graphmode:
        XOR     A
        CALL    rt_gfx__a
        LD      A,E
        JP      plat_gfx_mode

; DEFLINE n: the pattern for lines (the low 16 bits of n)
rt_gfx__defline:
        LD      DE,(rt_gfx__raw)
        LD      A,D
        OR      A
        JR      NZ,rt_gfx__dl1
        LD      A,E
        CP      7
        JR      NC,rt_gfx__dl1
        ADD     A,A                     ; 0..6: predefined
        LD      L,A
        LD      H,0
        LD      DE,rt_gfx__pre
        ADD     HL,DE
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
rt_gfx__dl1:
        LD      (rt_gfx__pat),DE
        EX      DE,HL
        JP      plat_gfx_lmask
rt_gfx__pre:                            ; as brt::Gfx::defline (1 checked, the others assumed)
        DW      &0000,&FFFF,&CCCC,&AAAA,&FF18,&FF00,&FE10

rt_gfx__boundary:
        XOR     A
        CALL    rt_gfx__a
        LD      A,D
        OR      E
        LD      (rt_gfx__bnd),A
        RET

; ---- points and lines -------------------------------------------------------------------------------

; argument A (x) and A+1 (y) -> DE = x, HL = y
rt_gfx__xy:
        PUSH    AF
        INC     A
        CALL    rt_gfx__a
        POP     AF
        PUSH    DE
        CALL    rt_gfx__a
        POP     HL
        RET

; PLOT x,y
rt_gfx__plot:
        XOR     A
        CALL    rt_gfx__xy
rt_gfx__pl:                             ; DE = x, HL = y: a point, the pen there
        LD      (rt_gfx__px),DE
        LD      (rt_gfx__py),HL
        JP      plat_gfx_plot

; LINE x1,y1,x2,y2
rt_gfx__line:
        XOR     A
        CALL    rt_gfx__xy
        CALL    plat_gfx_move
        LD      A,2
        CALL    rt_gfx__xy
rt_gfx__to:                             ; DE = x, HL = y: a line from the current point, the pen there
        LD      (rt_gfx__px),DE
        LD      (rt_gfx__py),HL
        JP      plat_gfx_draw

; DRAW x,y [TO x,y ...] or DRAW TO x,y ...: from the last point
rt_gfx__drawc:
        LD      C,0
        LD      A,(rt_gfx__mode)
        CP      1
        JR      Z,rt_gfx__dt
        XOR     A
        CALL    rt_gfx__xy
        CALL    rt_gfx__pl
        LD      C,2
rt_gfx__dt:
        LD      DE,(rt_gfx__px)
        LD      HL,(rt_gfx__py)
        PUSH    BC
        CALL    plat_gfx_move
        POP     BC
rt_gfx__dl:
        LD      A,(rt_gfx__n)
        DEC     A
        CP      C
        RET     C                       ; no pair left
        RET     Z
        LD      A,C
        PUSH    BC
        CALL    rt_gfx__xy
        CALL    rt_gfx__to
        POP     BC
        INC     C
        INC     C
        JR      rt_gfx__dl

; ---- boxes ------------------------------------------------------------------------------------------

; the corners -> rt_gfx__x1/y1 the smaller, x2/y2 the larger values
rt_gfx__corners:
        XOR     A
        CALL    rt_gfx__xy
        LD      (rt_gfx__x1),DE
        LD      (rt_gfx__y1),HL
        LD      A,2
        CALL    rt_gfx__xy
        LD      (rt_gfx__x2),DE
        LD      (rt_gfx__y2),HL
        LD      HL,rt_gfx__x1
        CALL    rt_gfx__order
        LD      HL,rt_gfx__y1
rt_gfx__order:                          ; HL -> a, a+4 = b (16 bit): a <= b afterwards (signed)
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        INC     HL
        INC     HL
        INC     HL
        LD      C,(HL)
        INC     HL
        LD      B,(HL)                  ; DE = a, BC = b
        PUSH    HL
        LD      H,D
        LD      L,E
        OR      A
        SBC     HL,BC
        POP     HL
        JP      PE,rt_gfx__ov
        RET     M                       ; a < b
        JR      rt_gfx__sw
rt_gfx__ov:
        RET     P
rt_gfx__sw:
        LD      (HL),D                  ; swap
        DEC     HL
        LD      (HL),E
        DEC     HL
        DEC     HL
        DEC     HL
        LD      (HL),B
        DEC     HL
        LD      (HL),C
        RET

; the outline of the box rt_gfx__x1..y2
rt_gfx__frame:
        LD      DE,(rt_gfx__x1)
        LD      HL,(rt_gfx__y1)
        CALL    plat_gfx_move
        LD      DE,(rt_gfx__x2)
        LD      HL,(rt_gfx__y1)
        CALL    plat_gfx_draw
        LD      DE,(rt_gfx__x2)
        LD      HL,(rt_gfx__y2)
        CALL    plat_gfx_draw
        LD      DE,(rt_gfx__x1)
        LD      HL,(rt_gfx__y2)
        CALL    plat_gfx_draw
        LD      DE,(rt_gfx__x1)
        LD      HL,(rt_gfx__y1)
        JP      plat_gfx_draw

rt_gfx__box:
        CALL    rt_gfx__corners
        JR      rt_gfx__frame

rt_gfx__pbox:
        CALL    rt_gfx__corners
        LD      HL,(rt_gfx__y1)
rt_gfx__pb:
        PUSH    HL
        LD      DE,(rt_gfx__x1)
        LD      BC,(rt_gfx__x2)
        CALL    rt_gfx__span
        POP     HL
        LD      DE,(rt_gfx__y2)
        OR      A
        PUSH    HL
        SBC     HL,DE
        POP     HL
        INC     HL
        JR      NZ,rt_gfx__pb
        CALL    rt_gfx__outpen
        RET     Z
        CALL    rt_gfx__frame
        JR      rt_gfx__back

; DE = x1, BC = x2, HL = y: a horizontal line of a filled shape (solid, whatever DEFLINE says)
rt_gfx__span:
        PUSH    BC
        PUSH    HL
        PUSH    DE
        LD      HL,(rt_gfx__pat)
        INC     HL
        LD      A,H
        OR      L
        JR      Z,rt_gfx__sp1           ; solid already
        LD      HL,&FFFF
        CALL    plat_gfx_lmask
rt_gfx__sp1:
        POP     DE
        POP     HL
        PUSH    HL
        CALL    plat_gfx_move
        POP     HL
        POP     DE
        CALL    plat_gfx_draw
        LD      HL,(rt_gfx__pat)
        INC     HL
        LD      A,H
        OR      L
        RET     Z
        DEC     HL
        JP      plat_gfx_lmask

; BOUNDARY on: the outline pen (NZ); off: Z
rt_gfx__outpen:
        LD      A,(rt_gfx__bnd)
        OR      A
        RET     Z
        LD      A,(rt_gfx__opn)
        CALL    plat_gfx_pen
        OR      1
        RET
; back to the drawing pen
rt_gfx__back:
        LD      A,(rt_gfx__apen)
        JP      plat_gfx_pen

; ---- ellipses ---------------------------------------------------------------------------------------

rt_gfx__circle:
        XOR     A
        JR      rt_gfx__ci
rt_gfx__pcircle:
        LD      A,1
rt_gfx__ci:
        LD      (rt_gfx__fill),A
        LD      A,2                     ; ry = 4r/9
        CALL    rt_gfx__a
        CALL    rt_gfx__rad
        LD      (rt_gfx__rx),DE
        EX      DE,HL
        ADD     HL,HL
        ADD     HL,HL
        LD      C,9
        CALL    rt_gfx__div
        LD      (rt_gfx__ry),HL
        JR      rt_gfx__el

rt_gfx__ellipse:
        XOR     A
        JR      rt_gfx__e0
rt_gfx__pellipse:
        LD      A,1
rt_gfx__e0:
        LD      (rt_gfx__fill),A
        LD      A,2
        CALL    rt_gfx__a
        CALL    rt_gfx__rad
        LD      (rt_gfx__rx),DE
        LD      A,3
        CALL    rt_gfx__a
        CALL    rt_gfx__rad
        LD      (rt_gfx__ry),DE
rt_gfx__el:
        XOR     A
        CALL    rt_gfx__xy
        LD      (rt_gfx__cx),DE
        LD      (rt_gfx__cy),HL
        CALL    rt_gfx__kennedy
        LD      A,(rt_gfx__fill)
        OR      A
        RET     Z
        CALL    rt_gfx__outpen          ; filled with BOUNDARY: the outline on top
        RET     Z
        XOR     A
        LD      (rt_gfx__fill),A
        CALL    rt_gfx__kennedy
        JR      rt_gfx__back

; DE = radius -> DE = |radius| limited to 1000
rt_gfx__rad:
        BIT     7,D
        JR      Z,rt_gfx__r1
        XOR     A
        SUB     E
        LD      E,A
        SBC     A,A
        SUB     D
        LD      D,A
rt_gfx__r1:
        LD      HL,1000
        OR      A
        SBC     HL,DE
        RET     NC
        LD      DE,1000
        RET

; HL / C -> HL (unsigned, C > 0)
rt_gfx__div:
        XOR     A
        LD      B,16
rt_gfx__dv:
        ADD     HL,HL
        RLA
        CP      C
        JR      C,rt_gfx__dn
        SUB     C
        INC     L
rt_gfx__dn:
        DJNZ    rt_gfx__dv
        RET

; HL -> cell, DE = 16-bit value: the cell = DE sign-extended
rt_gfx__set:
        LD      (HL),E
        INC     HL
        LD      (HL),D
        INC     HL
        LD      A,D
        RLCA
        SBC     A,A
        LD      (HL),A
        INC     HL
        LD      (HL),A
        RET

; HL -> dst, BC -> src: dst += src (resp. -=)
rt_gfx__add:
        LD      D,H
        LD      E,L
        JP      zi_add
rt_gfx__sub:
        LD      D,H
        LD      E,L
        JP      zi_sub

; HL -> cell -> CF if it is negative
rt_gfx__neg:
        INC     HL
        INC     HL
        INC     HL
        LD      A,(HL)
        RLCA
        RET

; HL -> cell -> NZ and CF clear if it is > 0
rt_gfx__pos:
        PUSH    HL
        CALL    rt_gfx__neg
        POP     HL
        JR      C,rt_gfx__p0
        LD      A,(HL)
        INC     HL
        OR      (HL)
        INC     HL
        OR      (HL)
        INC     HL
        OR      (HL)
        RET
rt_gfx__p0:
        XOR     A                       ; Z: not > 0
        RET

; the ellipse (cx, cy, rx, ry), outline or filled (rt_gfx__fill), by Kennedy's algorithm
rt_gfx__kennedy:
        LD      HL,(rt_gfx__rx)         ; rx = 0 or ry = 0: a line
        LD      A,H
        OR      L
        JR      Z,rt_gfx__kl
        LD      HL,(rt_gfx__ry)
        LD      A,H
        OR      L
        JR      NZ,rt_gfx__k0
rt_gfx__kl:
        LD      HL,(rt_gfx__cx)
        LD      DE,(rt_gfx__rx)
        OR      A
        SBC     HL,DE
        PUSH    HL
        LD      HL,(rt_gfx__cy)
        LD      DE,(rt_gfx__ry)
        OR      A
        SBC     HL,DE
        POP     DE
        CALL    plat_gfx_move
        LD      HL,(rt_gfx__cx)
        LD      DE,(rt_gfx__rx)
        ADD     HL,DE
        PUSH    HL
        LD      HL,(rt_gfx__cy)
        LD      DE,(rt_gfx__ry)
        ADD     HL,DE
        POP     DE
        JP      plat_gfx_draw
rt_gfx__k0:
        LD      HL,rt_gfx__t            ; a2 = rx*rx, b2 = ry*ry, ta2 = 2a2, tb2 = 2b2
        LD      DE,(rt_gfx__rx)
        CALL    rt_gfx__set
        LD      HL,rt_gfx__a2
        LD      DE,rt_gfx__t
        LD      BC,rt_gfx__t
        CALL    zi_mulv
        LD      HL,rt_gfx__t
        LD      DE,(rt_gfx__ry)
        CALL    rt_gfx__set
        LD      HL,rt_gfx__b2
        LD      DE,rt_gfx__t
        LD      BC,rt_gfx__t
        CALL    zi_mulv
        LD      HL,rt_gfx__ta2
        LD      DE,rt_gfx__a2
        LD      BC,rt_gfx__a2
        CALL    zi_add
        LD      HL,rt_gfx__tb2
        LD      DE,rt_gfx__b2
        LD      BC,rt_gfx__b2
        CALL    zi_add
        ; region 1: from (rx,0) upwards while the slope is steep
        LD      HL,(rt_gfx__rx)
        LD      (rt_gfx__x),HL
        LD      HL,0
        LD      (rt_gfx__y),HL
        LD      HL,rt_gfx__t            ; xch = b2 - rx*tb2
        LD      DE,(rt_gfx__rx)
        CALL    rt_gfx__set
        LD      HL,rt_gfx__sx           ; sx = tb2*rx
        LD      DE,rt_gfx__t
        LD      BC,rt_gfx__tb2
        CALL    zi_mulv
        LD      HL,rt_gfx__xch
        LD      DE,rt_gfx__b2
        LD      BC,rt_gfx__sx
        CALL    zi_sub
        LD      HL,rt_gfx__ych          ; ych = a2
        LD      DE,rt_gfx__a2
        CALL    rt_gfx__cpy
        LD      HL,rt_gfx__err
        LD      DE,0
        CALL    rt_gfx__set
        LD      HL,rt_gfx__sy
        LD      DE,0
        CALL    rt_gfx__set
rt_gfx__r1l:
        LD      HL,rt_gfx__t            ; while sx >= sy
        LD      DE,rt_gfx__sx
        LD      BC,rt_gfx__sy
        CALL    zi_sub
        LD      HL,rt_gfx__t
        CALL    rt_gfx__neg
        JR      C,rt_gfx__r2
        CALL    rt_gfx__emit
        LD      HL,(rt_gfx__y)
        INC     HL
        LD      (rt_gfx__y),HL
        LD      HL,rt_gfx__sy
        LD      BC,rt_gfx__ta2
        CALL    rt_gfx__add
        LD      HL,rt_gfx__err
        LD      BC,rt_gfx__ych
        CALL    rt_gfx__add
        LD      HL,rt_gfx__ych
        LD      BC,rt_gfx__ta2
        CALL    rt_gfx__add
        CALL    rt_gfx__test            ; 2*err + xch > 0?
        LD      BC,rt_gfx__xch
        CALL    rt_gfx__t2
        JR      Z,rt_gfx__r1l
        LD      HL,(rt_gfx__x)
        DEC     HL
        LD      (rt_gfx__x),HL
        LD      HL,rt_gfx__sx
        LD      BC,rt_gfx__tb2
        CALL    rt_gfx__sub
        LD      HL,rt_gfx__err
        LD      BC,rt_gfx__xch
        CALL    rt_gfx__add
        LD      HL,rt_gfx__xch
        LD      BC,rt_gfx__tb2
        CALL    rt_gfx__add
        JR      rt_gfx__r1l
rt_gfx__r2:                             ; region 2: from (0,ry) to the side while the slope is flat
        LD      HL,0
        LD      (rt_gfx__x),HL
        LD      HL,(rt_gfx__ry)
        LD      (rt_gfx__y),HL
        LD      HL,rt_gfx__xch          ; xch = b2
        LD      DE,rt_gfx__b2
        CALL    rt_gfx__cpy
        LD      HL,rt_gfx__t            ; sy = ta2*ry, ych = a2 - sy
        LD      DE,(rt_gfx__ry)
        CALL    rt_gfx__set
        LD      HL,rt_gfx__sy
        LD      DE,rt_gfx__t
        LD      BC,rt_gfx__ta2
        CALL    zi_mulv
        LD      HL,rt_gfx__ych
        LD      DE,rt_gfx__a2
        LD      BC,rt_gfx__sy
        CALL    zi_sub
        LD      HL,rt_gfx__err
        LD      DE,0
        CALL    rt_gfx__set
        LD      HL,rt_gfx__sx
        LD      DE,0
        CALL    rt_gfx__set
rt_gfx__r2l:
        LD      HL,rt_gfx__t            ; while sx <= sy
        LD      DE,rt_gfx__sy
        LD      BC,rt_gfx__sx
        CALL    zi_sub
        LD      HL,rt_gfx__t
        CALL    rt_gfx__neg
        RET     C
        CALL    rt_gfx__emit
        LD      HL,(rt_gfx__x)
        INC     HL
        LD      (rt_gfx__x),HL
        LD      HL,rt_gfx__sx
        LD      BC,rt_gfx__tb2
        CALL    rt_gfx__add
        LD      HL,rt_gfx__err
        LD      BC,rt_gfx__xch
        CALL    rt_gfx__add
        LD      HL,rt_gfx__xch
        LD      BC,rt_gfx__tb2
        CALL    rt_gfx__add
        CALL    rt_gfx__test            ; 2*err + ych > 0?
        LD      BC,rt_gfx__ych
        CALL    rt_gfx__t2
        JR      Z,rt_gfx__r2l
        LD      HL,(rt_gfx__y)
        DEC     HL
        LD      (rt_gfx__y),HL
        LD      HL,rt_gfx__sy
        LD      BC,rt_gfx__ta2
        CALL    rt_gfx__sub
        LD      HL,rt_gfx__err
        LD      BC,rt_gfx__ych
        CALL    rt_gfx__add
        LD      HL,rt_gfx__ych
        LD      BC,rt_gfx__ta2
        CALL    rt_gfx__add
        JR      rt_gfx__r2l

; t = 2*err
rt_gfx__test:
        LD      HL,rt_gfx__t
        LD      DE,rt_gfx__err
        LD      BC,rt_gfx__err
        JP      zi_add
; t += (BC) -> NZ if t > 0
rt_gfx__t2:
        LD      HL,rt_gfx__t
        CALL    rt_gfx__add
        LD      HL,rt_gfx__t
        JP      rt_gfx__pos

; HL -> dst, DE -> src: copy a cell
rt_gfx__cpy:
        EX      DE,HL
        LD      BC,4
        LDIR
        RET

; the points (cx +- x, cy +- y), or the lines between them (filled)
rt_gfx__emit:
        LD      A,(rt_gfx__fill)
        OR      A
        JR      NZ,rt_gfx__ef
        LD      HL,(rt_gfx__cy)         ; outline: four points
        LD      DE,(rt_gfx__y)
        ADD     HL,DE
        CALL    rt_gfx__e2
        LD      HL,(rt_gfx__cy)
        LD      DE,(rt_gfx__y)
        OR      A
        SBC     HL,DE
rt_gfx__e2:                             ; HL = y: the points cx + x and cx - x
        PUSH    HL
        LD      HL,(rt_gfx__cx)
        LD      DE,(rt_gfx__x)
        ADD     HL,DE
        EX      DE,HL
        POP     HL
        PUSH    HL
        CALL    plat_gfx_plot
        LD      HL,(rt_gfx__cx)
        LD      DE,(rt_gfx__x)
        OR      A
        SBC     HL,DE
        EX      DE,HL
        POP     HL
        JP      plat_gfx_plot
rt_gfx__ef:                             ; filled: the lines in the rows cy + y and cy - y
        LD      HL,(rt_gfx__cy)
        LD      DE,(rt_gfx__y)
        ADD     HL,DE
        CALL    rt_gfx__e3
        LD      HL,(rt_gfx__y)
        LD      A,H
        OR      L
        RET     Z                       ; the middle row only once
        LD      HL,(rt_gfx__cy)
        LD      DE,(rt_gfx__y)
        OR      A
        SBC     HL,DE
rt_gfx__e3:                             ; HL = y: the line from cx - x to cx + x
        PUSH    HL
        LD      HL,(rt_gfx__cx)
        LD      DE,(rt_gfx__x)
        ADD     HL,DE
        LD      B,H
        LD      C,L
        LD      HL,(rt_gfx__cx)
        OR      A
        SBC     HL,DE
        EX      DE,HL
        POP     HL
        JP      rt_gfx__span

; ---- polygons (probe25 T8) ----------------------------------------------------------------------------

; the points are the arguments x0,y0,x1,y1,...; A = k -> DE = x, HL = y of point k
rt_gfx__pt:
        ADD     A,A
        JP      rt_gfx__xy

; POLYLINE: the open polyline (one point: a point)
rt_gfx__polyline:
        LD      A,(rt_gfx__n)
        SRL     A
        RET     Z
        LD      (rt_gfx__m),A
        CP      1
        LD      A,0
        JR      NZ,rt_gfx__pl1
        CALL    rt_gfx__pt
        JP      rt_gfx__pl
rt_gfx__pl1:
        CALL    rt_gfx__pt
        CALL    plat_gfx_move
        LD      A,1
rt_gfx__pl2:
        PUSH    AF
        CALL    rt_gfx__pt
        CALL    rt_gfx__to
        POP     AF
        INC     A
        LD      HL,rt_gfx__m
        CP      (HL)
        JR      C,rt_gfx__pl2
        RET

; POLYFILL: every row from the leftmost to the rightmost point of the closed outline in it (as
; the original fills, D25), the outline with BOUNDARY on top. Rows 0..199 (those on the screen).
rt_gfx__polyfill:
        LD      A,(rt_gfx__n)
        SRL     A
        RET     Z
        LD      (rt_gfx__m),A
        LD      HL,rt_gfx__rows         ; all rows empty: min = 32767, max = -32768
        LD      B,200
rt_gfx__pf0:
        LD      (HL),&FF
        INC     HL
        LD      (HL),&7F
        INC     HL
        LD      (HL),&00
        INC     HL
        LD      (HL),&80
        INC     HL
        DJNZ    rt_gfx__pf0
        XOR     A                       ; the edges k -> k+1, the last one back to point 0
rt_gfx__pf1:
        LD      (rt_gfx__k),A
        INC     A
        LD      HL,rt_gfx__m
        CP      (HL)
        JR      C,rt_gfx__pf2
        XOR     A
rt_gfx__pf2:
        CALL    rt_gfx__pt              ; the end of the edge
        CALL    rt_gfx__c4k
        LD      (rt_gfx__ex),DE
        LD      (rt_gfx__ey),HL
        LD      A,(rt_gfx__k)
        CALL    rt_gfx__pt
        CALL    rt_gfx__c4k
        CALL    rt_gfx__edge
        LD      A,(rt_gfx__k)
        INC     A
        LD      HL,rt_gfx__m
        CP      (HL)
        JR      C,rt_gfx__pf1
        LD      HL,0                    ; the rows
rt_gfx__pf3:
        PUSH    HL
        ADD     HL,HL
        ADD     HL,HL
        LD      DE,rt_gfx__rows
        ADD     HL,DE
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        INC     HL
        LD      C,(HL)
        INC     HL
        LD      B,(HL)                  ; DE = min, BC = max
        LD      H,B
        LD      L,C
        CALL    rt_gfx__lt              ; max < min: empty
        POP     HL
        JR      C,rt_gfx__pf4
        PUSH    HL
        CALL    rt_gfx__span
        POP     HL
rt_gfx__pf4:
        INC     HL
        LD      A,L
        CP      200
        JR      C,rt_gfx__pf3
        CALL    rt_gfx__outpen
        RET     Z
        XOR     A                       ; the closed outline
        CALL    rt_gfx__pt
        PUSH    DE
        PUSH    HL
        CALL    plat_gfx_move
        LD      A,1
rt_gfx__pf5:
        LD      HL,rt_gfx__m
        CP      (HL)
        JR      NC,rt_gfx__pf6
        PUSH    AF
        CALL    rt_gfx__pt
        CALL    plat_gfx_draw
        POP     AF
        INC     A
        JR      rt_gfx__pf5
rt_gfx__pf6:
        POP     HL
        POP     DE
        CALL    plat_gfx_draw
        JP      rt_gfx__back

; DE = x, HL = y -> both limited to -4000..4000 (for the edges)
rt_gfx__c4k:
        PUSH    HL
        CALL    rt_gfx__l4k
        EX      (SP),HL
        EX      DE,HL                   ; DE = y, HL = x (limited)
        CALL    rt_gfx__l4k
        EX      DE,HL                   ; DE = x, HL = y (limited)
        POP     BC                      ; (the old x)
        RET
rt_gfx__l4k:                            ; DE -> DE limited (HL kept)
        PUSH    HL
        LD      HL,4000
        CALL    rt_gfx__lt              ; 4000 < DE?
        JR      NC,rt_gfx__l1
        EX      DE,HL
        JR      rt_gfx__l9
rt_gfx__l1:
        EX      DE,HL                   ; HL = value
        LD      DE,-4000
        CALL    rt_gfx__lt              ; value < -4000?
        JR      NC,rt_gfx__l2
        EX      DE,HL
rt_gfx__l2:
        EX      DE,HL
rt_gfx__l9:
        POP     HL
        RET

; HL, DE signed -> CF set if HL < DE (HL, DE kept)
rt_gfx__lt:
        PUSH    HL
        OR      A
        SBC     HL,DE
        LD      A,H
        POP     HL
        JP      PO,rt_gfx__lt1
        CPL                             ; overflow: the sign is inverted
rt_gfx__lt1:
        RLA
        RET

; the points of the edge from (DE, HL) to (rt_gfx__ex, rt_gfx__ey) into the row table (Bresenham)
rt_gfx__edge:
        LD      (rt_gfx__bx),DE
        LD      (rt_gfx__by),HL
        LD      HL,(rt_gfx__ex)         ; dx = |ex - x|, sx
        OR      A
        SBC     HL,DE
        LD      DE,1
        BIT     7,H
        JR      Z,rt_gfx__ed1
        CALL    rt_gfx__negh
        LD      DE,-1
rt_gfx__ed1:
        LD      (rt_gfx__dx),HL
        LD      (rt_gfx__sx2),DE
        LD      HL,(rt_gfx__ey)         ; dy = -|ey - y|, sy
        LD      DE,(rt_gfx__by)
        OR      A
        SBC     HL,DE
        LD      DE,1
        BIT     7,H
        JR      NZ,rt_gfx__ed2
        CALL    rt_gfx__negh
        JR      rt_gfx__ed3
rt_gfx__ed2:
        LD      DE,-1
rt_gfx__ed3:
        LD      (rt_gfx__dy),HL
        LD      (rt_gfx__sy2),DE
        LD      DE,(rt_gfx__dx)         ; err = dx + dy
        ADD     HL,DE
        LD      (rt_gfx__be),HL
rt_gfx__edl:
        LD      DE,(rt_gfx__bx)
        LD      HL,(rt_gfx__by)
        CALL    rt_gfx__mark
        LD      HL,(rt_gfx__bx)         ; at the end?
        LD      DE,(rt_gfx__ex)
        OR      A
        SBC     HL,DE
        JR      NZ,rt_gfx__ed4
        LD      HL,(rt_gfx__by)
        LD      DE,(rt_gfx__ey)
        OR      A
        SBC     HL,DE
        RET     Z
rt_gfx__ed4:
        LD      HL,(rt_gfx__be)         ; e2 = 2 err
        ADD     HL,HL
        LD      (rt_gfx__bq),HL
        LD      DE,(rt_gfx__dy)         ; e2 >= dy: err += dy, x += sx
        CALL    rt_gfx__lt
        JR      C,rt_gfx__ed5
        LD      HL,(rt_gfx__be)
        ADD     HL,DE
        LD      (rt_gfx__be),HL
        LD      HL,(rt_gfx__bx)
        LD      DE,(rt_gfx__sx2)
        ADD     HL,DE
        LD      (rt_gfx__bx),HL
rt_gfx__ed5:
        LD      HL,(rt_gfx__dx)         ; e2 <= dx: err += dx, y += sy
        LD      DE,(rt_gfx__bq)
        CALL    rt_gfx__lt
        JR      C,rt_gfx__edl
        LD      HL,(rt_gfx__be)
        LD      DE,(rt_gfx__dx)
        ADD     HL,DE
        LD      (rt_gfx__be),HL
        LD      HL,(rt_gfx__by)
        LD      DE,(rt_gfx__sy2)
        ADD     HL,DE
        LD      (rt_gfx__by),HL
        JR      rt_gfx__edl

; HL = -HL
rt_gfx__negh:
        XOR     A
        SUB     L
        LD      L,A
        SBC     A,A
        SUB     H
        LD      H,A
        RET

; DE = x, HL = y: the row y (0..199) reaches x
rt_gfx__mark:
        BIT     7,H
        RET     NZ
        LD      A,H
        OR      A
        RET     NZ
        LD      A,L
        CP      200
        RET     NC
        ADD     HL,HL
        ADD     HL,HL
        LD      BC,rt_gfx__rows
        ADD     HL,BC                   ; HL -> min, max
        PUSH    HL
        LD      C,(HL)
        INC     HL
        LD      B,(HL)
        PUSH    DE
        EX      DE,HL                   ; HL = x
        LD      D,B
        LD      E,C                     ; DE = min
        CALL    rt_gfx__lt              ; x < min?
        POP     DE
        POP     HL
        JR      NC,rt_gfx__mk1
        LD      (HL),E
        INC     HL
        LD      (HL),D
        DEC     HL
rt_gfx__mk1:
        INC     HL
        INC     HL
        LD      C,(HL)
        INC     HL
        LD      B,(HL)
        DEC     HL
        PUSH    HL
        LD      H,B
        LD      L,C                     ; HL = max
        CALL    rt_gfx__lt              ; max < x?
        POP     HL
        RET     NC
        LD      (HL),E
        INC     HL
        LD      (HL),D
        RET

; ---- text and POINT ---------------------------------------------------------------------------------

; TEXT x,y,s$: y is the base line
rt_gfx__text:
        XOR     A
        CALL    rt_gfx__xy
        LD      BC,(rt_gfx__s)
        JP      plat_gfx_text

; HL = result, DE -> x, BC -> y: POINT(x,y) (-1 outside the screen)
rt_gfx_point:
        PUSH    HL
        PUSH    BC
        CALL    rt_gfx__clamp           ; DE = x
        POP     HL                      ; HL -> y
        PUSH    DE
        EX      DE,HL
        CALL    rt_gfx__clamp
        EX      DE,HL                   ; HL = y
        POP     DE                      ; DE = x
        CALL    plat_gfx_test           ; A = pen, CF outside
        POP     HL
        JR      NC,rt_gfx__in
        LD      A,-1
rt_gfx__in:
        LD      (HL),A
        RLCA
        SBC     A,A                     ; -1 outside, else 0
        INC     HL
        LD      (HL),A
        INC     HL
        LD      (HL),A
        INC     HL
        LD      (HL),A
        RET

;! section data
rt_gfx__pat:                            ; the line pattern (DEFLINE)
        DW      &FFFF

;! section bss
rt_gfx__raw:
        DS      2
rt_gfx__rows:                           ; POLYFILL: per row 0..199 the leftmost and rightmost x
        DS      4*200
rt_gfx__m:
        DS      1
rt_gfx__k:
        DS      1
rt_gfx__ex:
        DS      2
rt_gfx__ey:
        DS      2
rt_gfx__bx:
        DS      2
rt_gfx__by:
        DS      2
rt_gfx__dx:
        DS      2
rt_gfx__dy:
        DS      2
rt_gfx__sx2:
        DS      2
rt_gfx__sy2:
        DS      2
rt_gfx__be:
        DS      2
rt_gfx__bq:
        DS      2
rt_gfx__n:
        DS      1
rt_gfx__mode:
        DS      1
rt_gfx__s:
        DS      2
rt_gfx__args:
        DS      2*rt_gfx__MAX
rt_gfx__px:
        DS      2
rt_gfx__py:
        DS      2
rt_gfx__x1:                             ; x1, y1, x2, y2 (the order matters for rt_gfx__order)
        DS      2
rt_gfx__y1:
        DS      2
rt_gfx__x2:
        DS      2
rt_gfx__y2:
        DS      2
rt_gfx__fill:
        DS      1
rt_gfx__bnd:
        DS      1
rt_gfx__apen:
        DS      1
rt_gfx__bpen:
        DS      1
rt_gfx__opn:
        DS      1
rt_gfx__cx:
        DS      2
rt_gfx__cy:
        DS      2
rt_gfx__rx:
        DS      2
rt_gfx__ry:
        DS      2
rt_gfx__x:
        DS      2
rt_gfx__y:
        DS      2
rt_gfx__a2:
        DS      4
rt_gfx__b2:
        DS      4
rt_gfx__ta2:
        DS      4
rt_gfx__tb2:
        DS      4
rt_gfx__xch:
        DS      4
rt_gfx__ych:
        DS      4
rt_gfx__err:
        DS      4
rt_gfx__sx:
        DS      4
rt_gfx__sy:
        DS      4
rt_gfx__t:
        DS      4
