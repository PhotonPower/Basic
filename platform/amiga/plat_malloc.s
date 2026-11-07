;! module    plat_malloc
;! provides  plat_malloc plat_mfree
;! group     memory
;! clobbers  D0-D3/A0-A1
;! doc       amiga: MALLOC/MFREE through exec AllocMem/FreeMem (always cleared, as on the other targets).

; D0 = size, D1 = flags (MEMF_PUBLIC 1, MEMF_CHIP 2, MEMF_FAST 4, MEMF_CLEAR $10000) -> D0 = address or 0
plat_malloc:
        MOVE.L  A6,-(SP)
        OR.L    #$10000,D1              ; MEMF_CLEAR
        MOVE.L  4,A6
        JSR     -198(A6)                ; AllocMem
        MOVE.L  (SP)+,A6
        RTS

; D0 = address, D1 = size
plat_mfree:
        MOVE.L  A6,-(SP)
        MOVE.L  D0,A1
        MOVE.L  D1,D0
        MOVE.L  4,A6
        JSR     -210(A6)                ; FreeMem
        MOVE.L  (SP)+,A6
        RTS
