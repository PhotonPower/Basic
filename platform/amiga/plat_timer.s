;! module    plat_timer
;! provides  plat_timer
;! requires  plat_dosbase
;! group     core
;! clobbers  D0-D3/A0-A1
;! doc       amiga: TIMER = 1/200 s since midnight from dos DateStamp (minutes, ticks of 1/50 s).

; -> D0 = TIMER
plat_timer:
        MOVE.L  A6,-(SP)
        LEA     plat_timer__ds,A0
        MOVE.L  A0,D1
        MOVE.L  plat_dosbase,A6
        JSR     -192(A6)                ; DateStamp(D1)
        MOVE.L  plat_timer__ds+4,D0     ; minutes * 12000 + ticks * 4
        MULU    #12000,D0
        MOVE.L  plat_timer__ds+8,D1
        LSL.L   #2,D1
        ADD.L   D1,D0
        MOVE.L  (SP)+,A6
        RTS

;! section bss
plat_timer__ds:
        DS.L    3
