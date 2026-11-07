;! module    plat_pause
;! provides  plat_pause
;! requires  plat_dosbase
;! group     core
;! clobbers  D0-D3/A0-A1
;! doc       amiga: PAUSE/DELAY wait with dos Delay (ticks of 1/50 s).

; D0 = ticks
plat_pause:
        TST.L   D0
        BLE     .out
        MOVE.L  A6,-(SP)
        MOVE.L  D0,D1
        MOVE.L  plat_dosbase,A6
        JSR     -198(A6)                ; Delay(D1)
        MOVE.L  (SP)+,A6
.out:   RTS
