;! module    plat_pause
;! provides  plat_pause
;! group     core
;! clobbers  D0-D3/A0-A1
;! doc       m68k-test: PAUSE/DELAY do not wait (no clock).

; D0 = ticks
plat_pause:
        RTS
