;! module    plat_gfx
;! provides  plat_gfx plat_gfx_point
;! group     graphics
;! clobbers  D0-D3/A0-A1
;! doc       m68k-test: graphics through ports of the test harness, which draws with the rules of D25.

; Ports: $00FF0014 (long) next argument; $00FF0018/$00FF001C (long) address/length of the string;
; $00FF0020 (long) mode; $00FF0010 (long) the command code: executes the command. POINT: the two
; arguments, then a long read of $00FF0024.

; D0 = code, D1 = mode, D2 = number of arguments, A0 = arguments, A1 = string descriptor or 0
plat_gfx:
        BRA     .n
.a:     MOVE.L  (A0)+,$00FF0014
.n:     SUBQ.L  #1,D2
        BPL     .a
        MOVE.L  A1,D2
        BEQ     .ns
        MOVE.L  (A1),$00FF0018
        MOVEQ   #0,D2
        MOVE.W  4(A1),D2
        MOVE.L  D2,$00FF001C
.ns:    MOVE.L  D1,$00FF0020
        MOVE.L  D0,$00FF0010
        RTS

; D0 = x, D1 = y -> D0 = POINT(x,y)
plat_gfx_point:
        MOVE.L  D0,$00FF0014
        MOVE.L  D1,$00FF0014
        MOVE.L  $00FF0024,D0
        RTS
