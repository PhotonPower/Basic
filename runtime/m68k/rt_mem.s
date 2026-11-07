;! module    rt_mem
;! provides  rt_mem_chars rt_peek rt_dpeek rt_lpeek rt_poke rt_dpoke rt_lpoke rt_malloc rt_mfree rt_bmove
;! provides  rt_wpeek rt_float_get rt_float_set rt_single_get rt_single_set rt_double_get rt_double_set
;! provides  rt_char_get rt_char_set
;! requires  plat_malloc plat_mfree rt_mkd rt_cvd rt_mks rt_cvs rt_str_newtemp
;! group     memory
;! clobbers  D0-D3/A0-A1
;! doc       Group memory: PEEK/POKE (8/16/32 bit), BYTE{} ... CHAR{}, V: of strings, MALLOC/MFREE, BMOVE on real memory.

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

; D0 = address -> D0 = INT{} (signed word)
rt_wpeek:
        MOVE.L  D0,A0
        MOVE.W  (A0),D0
        EXT.L   D0
        RTS

; D0 = address -> A0 = a descriptor of D1 bytes there (for CVD/CVS)
rt_mem__desc:
        LEA     rt_mem__d,A0
        MOVE.L  D0,(A0)
        SWAP    D1
        CLR.W   D1
        MOVE.L  D1,4(A0)
        RTS

; D0 = address -> D0:D1 = FLOAT{} (GFA 8-byte format), SINGLE{} (IEEE single), DOUBLE{} (IEEE double)
rt_float_get:
        MOVEQ   #8,D1
        BSR     rt_mem__desc
        JMP     rt_cvd
rt_single_get:
        MOVEQ   #4,D1
        BSR     rt_mem__desc
        JMP     rt_cvs
rt_double_get:
        MOVE.L  D0,A0
        MOVE.L  (A0),D0
        MOVE.L  4(A0),D1
        RTS

; D0 = address, D2:D3 = value
rt_float_set:
        MOVE.L  D0,-(SP)
        MOVE.L  D2,D0
        MOVE.L  D3,D1
        JSR     rt_mkd                  ; A0 = temporary with the 8 bytes
        MOVEQ   #8,D0
        BRA     rt_mem__put
rt_single_set:
        MOVE.L  D0,-(SP)
        MOVE.L  D2,D0
        MOVE.L  D3,D1
        JSR     rt_mks
        MOVEQ   #4,D0
rt_mem__put:                            ; (SP) = address, A0 = descriptor, D0 = count
        MOVE.L  (SP)+,A1
        MOVE.L  (A0),A0
        SUBQ.L  #1,D0
.c:     MOVE.B  (A0)+,(A1)+
        DBRA    D0,.c
        RTS
rt_double_set:
        MOVE.L  D0,A0
        MOVE.L  D2,(A0)
        MOVE.L  D3,4(A0)
        RTS

; D0 = address -> A0 = temporary with the characters up to the zero byte (at most 32767)
rt_char_get:
        MOVE.L  D0,A0
        MOVEQ   #0,D1
.n:     TST.B   (A0)+
        BEQ     .len
        ADDQ.L  #1,D1
        CMP.L   #32767,D1
        BLO     .n
.len:   MOVEM.L D0-D1,-(SP)
        MOVE.L  D1,D0
        JSR     rt_str_newtemp          ; A1 = descriptor, A0 = characters
        MOVEM.L (SP)+,D0-D1             ; D0 = source, D1 = length
        MOVE.L  A1,D2
        MOVE.L  D0,A1
        BRA     .cn
.c:     MOVE.B  (A1)+,(A0)+
.cn:    SUBQ.L  #1,D1
        BPL     .c
        MOVE.L  D2,A0
        RTS

; D0 = address, A0 = descriptor: CHAR{a}=s$ (the characters and a zero byte)
rt_char_set:
        MOVE.L  D0,A1
        MOVEQ   #0,D1
        MOVE.W  4(A0),D1
        MOVE.L  (A0),A0
        BRA     .cn
.c:     MOVE.B  (A0)+,(A1)+
.cn:    SUBQ.L  #1,D1
        BPL     .c
        CLR.B   (A1)
        RTS

;! section bss
rt_mem__tab:
        DS.L    2*rt_mem__MAX
rt_mem__d:
        DS.L    2
