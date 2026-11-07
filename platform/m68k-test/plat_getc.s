;! module    plat_getc
;! provides  plat_getc
;! group     core
;! clobbers  D0-D1/A0-A1
;! doc       m68k-test: next byte of the keyboard input from the port $00FF0004 (-1 at the end).

; -> D0 = character (0..255), or -1 at the end of the input
plat_getc:
        MOVE.L  $00FF0004,D0
        MOVE.L  #$DEADBEEF,D1           ; garbage where the convention allows it (finds careless callers)
        MOVE.L  D1,A0
        MOVE.L  D1,A1
        RTS
