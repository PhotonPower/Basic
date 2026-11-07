;! module    plat_alloc
;! provides  plat_alloc plat_free_all
;! group     core
;! clobbers  D0-D1/A0-A1
;! doc       amiga: memory for the string heap and the array area through exec AllocMem (cleared), freed at the end.

plat_alloc__MAX EQU 8

; D0 = size in bytes -> A0 = address (0 if there is no memory); exec AllocMem -198
plat_alloc:
        MOVE.L  plat_alloc__n,D1
        CMP.L   #plat_alloc__MAX,D1
        BCC     .no
        MOVEM.L D2/A6,-(SP)
        MOVE.L  D0,D2
        MOVE.L  #$10001,D1              ; MEMF_PUBLIC | MEMF_CLEAR
        MOVE.L  4,A6
        JSR     -198(A6)
        MOVE.L  D0,A0
        TST.L   D0
        BEQ     .done
        MOVE.L  plat_alloc__n,D1        ; remember the block for plat_free_all
        LSL.L   #3,D1
        LEA     plat_alloc__tab,A1
        MOVE.L  D0,0(A1,D1.L)
        MOVE.L  D2,4(A1,D1.L)
        ADDQ.L  #1,plat_alloc__n
.done:  MOVEM.L (SP)+,D2/A6
        RTS
.no:    SUB.L   A0,A0
        RTS

; frees all blocks (exec FreeMem -210: A1 = address, D0 = size)
plat_free_all:
        MOVEM.L D2/A6,-(SP)
        MOVE.L  4,A6
.l:     MOVE.L  plat_alloc__n,D2
        BEQ     .out
        SUBQ.L  #1,D2
        MOVE.L  D2,plat_alloc__n
        LSL.L   #3,D2
        LEA     plat_alloc__tab,A1
        MOVE.L  4(A1,D2.L),D0
        MOVE.L  0(A1,D2.L),A1
        JSR     -210(A6)
        BRA     .l
.out:   MOVEM.L (SP)+,D2/A6
        RTS

;! section data
plat_alloc__n:
        DC.L    0

;! section bss
plat_alloc__tab:
        DS.L    2*plat_alloc__MAX
