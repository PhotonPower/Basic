;! module    plat_gfx
;! provides  plat_gfx_plot plat_gfx_move plat_gfx_draw plat_gfx_test plat_gfx_pen plat_gfx_paper plat_gfx_mode
;! provides  plat_gfx_ink plat_gfx_text plat_gfx_clear
;! group     graphics
;! clobbers  AF BC DE HL
;! doc       cpc: graphics through the firmware (GRA): GFA coordinates on the 640 x 400 logical screen.

; DE = x, HL = y in GFA coordinates (y downwards, one unit per screen row): the firmware gets
; X = x and Y = 398 - 2y (its origin is at the bottom left, 2 units per row and per pixel of mode
; 1). So x 0..639 and y 0..199 are visible; the firmware clips the rest.

; HL = y -> HL = 398 - 2y (y limited to -16000..16000)
plat_gfx__y:
        LD      A,H
        BIT     7,A
        JR      NZ,plat_gfx__yn
        CP      &3E                     ; 15872 and more: limit
        JR      C,plat_gfx__y2
        LD      HL,&3E80                ; 16000
        JR      plat_gfx__y2
plat_gfx__yn:
        CP      &C2
        JR      NC,plat_gfx__y2
        LD      HL,-16000
plat_gfx__y2:
        ADD     HL,HL
        PUSH    DE
        EX      DE,HL
        LD      HL,398
        OR      A
        SBC     HL,DE
        POP     DE
        RET

plat_gfx_plot:
        CALL    plat_gfx__y
        JP      &BBEA                   ; GRA PLOT ABSOLUTE
plat_gfx_move:
        CALL    plat_gfx__y
        JP      &BBC0                   ; GRA MOVE ABSOLUTE
plat_gfx_draw:
        CALL    plat_gfx__y
        JP      &BBF6                   ; GRA LINE ABSOLUTE

; DE = x, HL = y -> A = pen there; CF set outside the screen (x 0..639, y 0..199)
plat_gfx_test:
        BIT     7,D
        SCF
        RET     NZ
        BIT     7,H
        SCF
        RET     NZ
        PUSH    HL
        LD      HL,639
        OR      A
        SBC     HL,DE
        POP     HL
        RET     C
        PUSH    HL
        LD      BC,200
        OR      A
        SBC     HL,BC
        CCF
        POP     HL
        RET     C
        CALL    plat_gfx__y
        CALL    &BBF0                   ; GRA TEST ABSOLUTE
        OR      A
        RET

; A = pen (COLOR, DEFFILL); in XOR mode the firmware gets pen 3, so that XOR inverts all bits as
; COMPLEMENT on the Amiga does
plat_gfx_pen:
        LD      (plat_gfx__apen),A
plat_gfx__setpen:
        LD      A,(plat_gfx__xor)
        OR      A
        LD      A,3
        JP      NZ,&BBDE                ; GRA SET PEN
        LD      A,(plat_gfx__apen)
        JP      &BBDE
; A = background pen (COLOR's second value)
plat_gfx_paper:
        JP      &BBE4                   ; GRA SET PAPER

; A = GRAPHMODE: XOR (bit 1) through SCR ACCESS, else normal
plat_gfx_mode:
        AND     2
        RRCA
        LD      (plat_gfx__xor),A
        CALL    &BC59                   ; SCR ACCESS: 0 normal, 1 XOR
        JR      plat_gfx__setpen

; A = pen, HL = colour 0RGB (4 bits each): the nearest of the 27 colours (SETCOLOR)
plat_gfx_ink:
        LD      (plat_gfx__pen),A
        LD      A,H                     ; r
        CALL    plat_gfx__lvl
        LD      B,A
        ADD     A,A
        ADD     A,B
        LD      C,A                     ; 3r
        LD      A,L                     ; g
        RRCA
        RRCA
        RRCA
        RRCA
        CALL    plat_gfx__lvl
        LD      B,A
        ADD     A,A
        ADD     A,A
        ADD     A,A
        ADD     A,B                     ; 9g
        ADD     A,C
        LD      C,A
        LD      A,L                     ; b
        CALL    plat_gfx__lvl
        ADD     A,C
        LD      B,A
        LD      C,A
        LD      A,(plat_gfx__pen)
        JP      &BC32                   ; SCR SET INK
plat_gfx__lvl:                          ; A (low 4 bits) -> 0 (0..4), 1 (5..10), 2 (11..15)
        AND     15
        CP      5
        JR      C,plat_gfx__l0
        CP      11
        LD      A,1
        RET     C
        INC     A
        RET
plat_gfx__l0:
        XOR     A
        RET

; DE = x, HL = y (base line), BC = string descriptor: TEXT (characters of 8 x 8 pixels, the top row
; 7 rows above the base line)
plat_gfx_text:
        PUSH    BC
        LD      BC,-7
        ADD     HL,BC
        CALL    plat_gfx_move
        POP     HL
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        INC     HL
        LD      C,(HL)
        INC     HL
        LD      B,(HL)
plat_gfx__t:
        LD      A,B
        OR      C
        RET     Z
        LD      A,(DE)
        PUSH    BC
        PUSH    DE
        CALL    &BBFC                   ; GRA WR CHAR
        POP     DE
        POP     BC
        INC     DE
        DEC     BC
        JR      plat_gfx__t

; CLEARW, CLS: the graphics window in the background pen
plat_gfx_clear:
        JP      &BBDB                   ; GRA CLEAR WINDOW

;! section bss
plat_gfx__pen:
        DS      1
plat_gfx__apen:                         ; the pen of the program (COLOR)
        DS      1
plat_gfx__xor:                          ; 1 in XOR mode
        DS      1
