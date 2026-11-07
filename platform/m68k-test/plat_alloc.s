;! module    plat_alloc
;! provides  plat_alloc
;! group     core
;! clobbers  D0-D1/A0-A1
;! doc       m68k-test: memory for the string heap and the array area, from $100000 upwards (cleared).

; D0 = size in bytes -> A0 = address (0 if there is no room)
plat_alloc:
        MOVE.L  plat_alloc__next,A0
        ADDQ.L  #3,D0
        AND.L   #-4,D0
        LEA     0(A0,D0.L),A1
        CMP.L   #$00800000,A1
        BHI     .no
        MOVE.L  A1,plat_alloc__next
        MOVE.L  A0,-(SP)
        LSR.L   #2,D0
        BRA     .n
.cl:    CLR.L   (A0)+
.n:     SUBQ.L  #1,D0
        BPL     .cl
        MOVE.L  (SP)+,A0
        RTS
.no:    SUB.L   A0,A0
        RTS

;! section data
plat_alloc__next:
        DC.L    $00100000
