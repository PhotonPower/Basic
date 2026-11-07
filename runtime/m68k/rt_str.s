;! module    rt_str
;! provides  rt_str_alloc rt_str_gc rt_str_newtemp rt_str_mark rt_str_release rt_str_assign rt_str_free rt_str_relink rt_str_set_ret rt_str_take_ret rt_print_sdesc rt_str_ret rt_str_swap
;! requires  rt_error rt_print_str plat_alloc
;! group     string
;! clobbers  D0-D3/A0-A1
;! doc       String heap (decision D18): descriptors, blocks with a back reference, compaction, temporaries.

; Descriptor (8 bytes): +0 address of the characters (0: empty), +4 length (word), +6 unused.
; Block in the heap: +0 owner (descriptor address, 0: garbage), +4 length (word), +6 characters,
; padded to an even size. Temporaries live in rt_str__temps with stack discipline. The heap
; (rt_str__lo .. rt_str__hi) comes from plat_alloc on first use.

rt_str__HEAPSIZE EQU 65536
rt_str__TEMPS    EQU 128

; D0.L = length (1..32767) -> A0 = characters of a new block (owner 0); error 8 if there is no room
rt_str_alloc:
        MOVEM.L D2-D3,-(SP)
        MOVE.L  D0,D2
        MOVE.L  D2,D3                   ; size = (length + 6 + 1) and -2
        ADDQ.L  #7,D3
        AND.L   #-2,D3
        BSR     rt_str__room
        BCC     .ok
        JSR     rt_str_gc
        BSR     rt_str__room
        BCC     .ok
        MOVEQ   #8,D0                   ; out of memory
        JSR     rt_error
        SUB.L   A0,A0
        BRA     .out
.ok:    MOVE.L  rt_str__top,A0
        LEA     0(A0,D3.L),A1
        MOVE.L  A1,rt_str__top
        CLR.L   (A0)+
        MOVE.W  D2,(A0)+
.out:   MOVEM.L (SP)+,D2-D3
        RTS

; carry clear if D3 bytes fit at the top of the heap (initialises the top on first use)
rt_str__room:
        TST.L   rt_str__top
        BNE     .init
        MOVE.L  #rt_str__HEAPSIZE,D0    ; first use: the heap from the platform
        JSR     plat_alloc
        MOVE.L  A0,rt_str__lo
        MOVE.L  A0,rt_str__top
        BEQ     .no
        ADD.L   #rt_str__HEAPSIZE,A0
        MOVE.L  A0,rt_str__hi
.init:  MOVE.L  rt_str__top,A0
        LEA     0(A0,D3.L),A0
        CMP.L   rt_str__hi,A0
        BHI     .no
        ANDI    #$FE,CCR
        RTS
.no:    ORI     #$01,CCR
        RTS

; compaction: move all blocks with an owner down, update the owners' addresses
rt_str_gc:
        MOVEM.L D2-D3/A2-A3,-(SP)
        MOVE.L  rt_str__lo,A0           ; A0 = source, A1 = destination
        MOVE.L  A0,A1
        MOVE.L  rt_str__top,A3
.walk:  CMP.L   A3,A0                   ; A0 >= top: done
        BCC     .done
        MOVEQ   #0,D2                   ; D2 = size of the block
        MOVE.W  4(A0),D2
        ADDQ.L  #7,D2
        AND.L   #-2,D2
        MOVE.L  (A0),D3                 ; owner
        BEQ     .skip
        MOVE.L  D3,A2                   ; the owner gets the new address of the characters
        MOVE.L  A1,D3
        ADDQ.L  #6,D3
        MOVE.L  D3,(A2)
        LSR.L   #1,D2                   ; move the block down, word by word
        SUBQ.L  #1,D2
.copy:  MOVE.W  (A0)+,(A1)+
        DBRA    D2,.copy
        BRA     .walk
.skip:  ADD.L   D2,A0
        BRA     .walk
.done:  MOVE.L  A1,rt_str__top
        MOVEM.L (SP)+,D2-D3/A2-A3
        RTS

; A0 = descriptor: its block becomes garbage (if it owns one); the descriptor is not changed
rt_str_free:
        MOVE.L  (A0),D0
        BEQ     .out
        BSR     rt_str__inheap
        BCS     .out
        MOVE.L  D0,A1
        CMP.L   -6(A1),A0
        BNE     .out
        CLR.L   -6(A1)
.out:   RTS

; A0 = descriptor: the back reference of its block points to it
rt_str_relink:
        MOVE.L  (A0),D0
        BEQ     .out
        BSR     rt_str__inheap
        BCS     .out
        MOVE.L  D0,A1
        MOVE.L  A0,-6(A1)
.out:   RTS

; carry clear if the address D0 lies in the heap
rt_str__inheap:
        CMP.L   rt_str__lo,D0
        BCS     .no
        CMP.L   rt_str__hi,D0
        BCC     .no
        ANDI    #$FE,CCR
        RTS
.no:    ORI     #$01,CCR
        RTS

; D0.L = length -> A1 = new temporary descriptor owning a new block, A0 = its characters
rt_str_newtemp:
        MOVE.L  D2,-(SP)
        MOVE.L  D0,D2
        MOVE.L  rt_str__ttop,D1
        CMP.L   #rt_str__TEMPS,D1
        BCS     .room
        MOVEQ   #8,D0
        JSR     rt_error
.room:  ADDQ.L  #1,rt_str__ttop
        LSL.L   #3,D1
        LEA     rt_str__temps,A1
        ADD.L   D1,A1
        CLR.L   (A1)
        MOVE.W  D2,4(A1)
        SUB.L   A0,A0
        TST.L   D2
        BEQ     .out
        MOVE.L  A1,-(SP)
        MOVE.L  D2,D0
        BSR     rt_str_alloc
        MOVE.L  (SP)+,A1
        MOVE.L  A0,(A1)
        MOVE.L  A1,-6(A0)
.out:   MOVE.L  (SP)+,D2
        RTS

; -> D0 = current number of temporaries (a routine remembers it at its entry)
rt_str_mark:
        MOVE.L  rt_str__ttop,D0
        RTS

; D0 = mark: release all temporaries above it
rt_str_release:
        MOVE.L  D2,-(SP)
        MOVE.L  D0,D2
.loop:  MOVE.L  rt_str__ttop,D0
        CMP.L   D2,D0
        BLE     .out
        SUBQ.L  #1,D0
        MOVE.L  D0,rt_str__ttop
        LSL.L   #3,D0
        LEA     rt_str__temps,A0
        ADD.L   D0,A0
        MOVE.L  A0,-(SP)
        BSR     rt_str_free
        MOVE.L  (SP)+,A0
        CLR.L   (A0)
        CLR.W   4(A0)
        BRA     .loop
.out:   MOVE.L  (SP)+,D2
        RTS

; A0 = destination descriptor, A1 = source descriptor: dest$ = source$
; A temporary hands its block over; variables and constants are copied.
rt_str_assign:
        CMP.L   A0,A1
        BEQ     .out
        MOVEM.L D2/A2-A3,-(SP)
        MOVE.L  A0,A2                   ; A2 = destination, A3 = source
        MOVE.L  A1,A3
        CMP.L   #rt_str__temps,A3       ; a temporary that owns its block?
        BCS     .copy
        CMP.L   #rt_str__temps+8*rt_str__TEMPS,A3
        BCC     .copy
        MOVE.L  (A3),D0
        BEQ     .copy
        MOVE.L  D0,A1
        CMP.L   -6(A1),A3
        BNE     .copy
        MOVE.L  A2,A0                   ; hand over
        BSR     rt_str_free
        MOVE.L  (A3),(A2)
        MOVE.W  4(A3),4(A2)
        MOVE.L  (A2),A1
        MOVE.L  A2,-6(A1)
        CLR.L   (A3)
        CLR.W   4(A3)
        BRA     .done
.copy:  MOVEQ   #0,D2
        MOVE.W  4(A3),D2
        BNE     .alloc
        MOVE.L  A2,A0                   ; empty
        BSR     rt_str_free
        CLR.L   (A2)
        CLR.W   4(A2)
        BRA     .done
.alloc: MOVE.L  D2,D0
        BSR     rt_str_alloc            ; may move blocks: read the addresses afterwards
        MOVE.L  A0,-(SP)
        MOVE.L  A2,A0
        BSR     rt_str_free
        MOVE.L  (SP)+,A0
        MOVE.L  A2,-6(A0)
        MOVE.L  A0,(A2)
        MOVE.W  D2,4(A2)
        MOVE.L  (A3),A1
        SUBQ.L  #1,D2
.cp:    MOVE.B  (A1)+,(A0)+
        DBRA    D2,.cp
.done:  MOVEM.L (SP)+,D2/A2-A3
.out:   RTS

; A0 = descriptor: becomes the result of a string function
rt_str_set_ret:
        MOVE.L  A0,A1
        LEA     rt_str_ret,A0
        BRA     rt_str_assign

; -> A0 = new temporary that takes over the result of a string function
rt_str_take_ret:
        MOVEQ   #0,D0
        BSR     rt_str_newtemp          ; empty temporary (no block)
        MOVE.L  rt_str_ret,(A1)
        MOVE.W  rt_str_ret+4,4(A1)
        CLR.L   rt_str_ret
        CLR.W   rt_str_ret+4
        MOVE.L  A1,A0
        MOVE.L  A0,-(SP)
        BSR     rt_str_relink
        MOVE.L  (SP)+,A0
        RTS

; A0, A1 = descriptors: exchange the strings (SWAP)
rt_str_swap:
        MOVE.L  (A0),D0
        MOVE.L  (A1),(A0)
        MOVE.L  D0,(A1)
        MOVE.L  4(A0),D0
        MOVE.L  4(A1),4(A0)
        MOVE.L  D0,4(A1)
        MOVE.L  A1,-(SP)
        BSR     rt_str_relink
        MOVE.L  (SP)+,A0
        BRA     rt_str_relink

; A0 = descriptor: PRINT
rt_print_sdesc:
        MOVEQ   #0,D0
        MOVE.W  4(A0),D0
        MOVE.L  (A0),A0
        JMP     rt_print_str

;! section data
rt_str__top:
        DC.L    0
rt_str__ttop:
        DC.L    0
rt_str__lo:
        DC.L    0
rt_str__hi:
        DC.L    0

;! section bss
rt_str_ret:
        DS.L    2
rt_str__temps:
        DS.L    2*rt_str__TEMPS
