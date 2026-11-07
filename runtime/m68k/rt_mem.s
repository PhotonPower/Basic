;! module    rt_mem
;! provides  rt_mem_chars rt_peek rt_dpeek rt_lpeek rt_poke rt_dpoke rt_lpoke rt_malloc rt_mfree rt_bmove
;! requires  plat_malloc plat_mfree
;! group     memory
;! clobbers  D0-D3/A0-A1
;! doc       Group memory, stage 1: PEEK/POKE (8/16/32 bit), V: of strings, MALLOC/MFREE, BMOVE on real memory.

rt_mem__MAX EQU 32                      ; MALLOC blocks at the same time

; A0 = string descriptor -> D0 = address of the characters (V:a$)
rt_mem_chars:
        MOVE.L  (A0),D0
        RTS

; D0 = address -> D0 = PEEK (0..255), DPEEK (0..65535), LPEEK
rt_peek:
        MOVE.L  D0,A0
        MOVEQ   #0,D0
        MOVE.B  (A0),D0
        RTS
rt_dpeek:
        MOVE.L  D0,A0
        MOVEQ   #0,D0
        MOVE.W  (A0),D0
        RTS
rt_lpeek:
        MOVE.L  D0,A0
        MOVE.L  (A0),D0
        RTS

; D0 = address, D1 = value
rt_poke:
        MOVE.L  D0,A0
        MOVE.B  D1,(A0)
        RTS
rt_dpoke:
        MOVE.L  D0,A0
        MOVE.W  D1,(A0)
        RTS
rt_lpoke:
        MOVE.L  D0,A0
        MOVE.L  D1,(A0)
        RTS

; D0 = size, D1 = flags (MEMF_*, as on the Amiga) -> D0 = address of a cleared block, 0 if none
rt_malloc:
        MOVEM.L D4/A2,-(SP)
        MOVE.L  D0,D4
        BLE     .zero
        LEA     rt_mem__tab,A2          ; a free entry
        MOVEQ   #rt_mem__MAX-1,D2
.f:     TST.L   (A2)
        BEQ     .got
        ADDQ.L  #8,A2
        DBRA    D2,.f
        BRA     .zero
.got:   JSR     plat_malloc
        TST.L   D0
        BEQ     .out
        MOVE.L  D0,(A2)
        MOVE.L  D4,4(A2)
        BRA     .out
.zero:  MOVEQ   #0,D0
.out:   MOVEM.L (SP)+,D4/A2
        RTS

; D0 = address, D1 = size -> D0 = 0, or -1 if it is no MALLOC block
rt_mfree:
        LEA     rt_mem__tab,A0
        MOVEQ   #rt_mem__MAX-1,D2
.f:     CMP.L   (A0),D0
        BEQ     .got
        ADDQ.L  #8,A0
        DBRA    D2,.f
        MOVEQ   #-1,D0
        RTS
.got:   TST.L   D0
        BEQ     .no
        MOVE.L  4(A0),D1                ; the size of the block (as MALLOC got it)
        CLR.L   (A0)
        JSR     plat_mfree
        MOVEQ   #0,D0
        RTS
.no:    MOVEQ   #-1,D0
        RTS

; D0 = from, D1 = to, D2 = count: BMOVE (overlapping allowed)
rt_bmove:
        TST.L   D2
        BLE     .out
        MOVE.L  D0,A0
        MOVE.L  D1,A1
        CMP.L   A0,A1
        BHI     .back
.fw:    MOVE.B  (A0)+,(A1)+             ; to below from: forwards
        SUBQ.L  #1,D2
        BNE     .fw
        RTS
.back:  ADD.L   D2,A0                   ; to above from: backwards
        ADD.L   D2,A1
.bw:    MOVE.B  -(A0),-(A1)
        SUBQ.L  #1,D2
        BNE     .bw
.out:   RTS

;! section bss
rt_mem__tab:
        DS.L    2*rt_mem__MAX
