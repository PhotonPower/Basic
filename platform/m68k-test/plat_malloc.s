;! module    plat_malloc
;! provides  plat_malloc plat_mfree
;! requires  plat_alloc
;! group     memory
;! clobbers  D0-D3/A0-A1
;! doc       m68k-test: MALLOC from the area of plat_alloc (cleared); MFREE gives nothing back.

; D0 = size, D1 = flags -> D0 = address (0 if there is no room)
plat_malloc:
        JSR     plat_alloc
        MOVE.L  A0,D0
        RTS

; D0 = address, D1 = size
plat_mfree:
        RTS
