;! module    plat_gfx
;! provides  plat_gfx plat_gfx_point
;! group     graphics
;! clobbers  D0-D3/A0-A1
;! doc       amiga: graphics (D25 stage 2 follows: intuition/graphics.library); for now nothing is drawn.

; D0 = code, D1 = mode, D2 = number of arguments, A0 = arguments, A1 = string descriptor or 0
plat_gfx:
        RTS

; D0 = x, D1 = y -> D0 = POINT(x,y): -1 (nothing is drawn yet)
plat_gfx_point:
        MOVEQ   #-1,D0
        RTS
