;! module    plat_timer
;! provides  plat_timer
;! group     core
;! clobbers  D0-D3/A0-A1
;! doc       m68k-test: TIMER without a clock: counts up by one on every call (deterministic, never stands).

; -> D0 = TIMER
plat_timer:
        ADDQ.L  #1,plat_timer__n
        MOVE.L  plat_timer__n,D0
        RTS

;! section bss
plat_timer__n:
        DS.L    1
