;! module    rt_type
;! provides  rt_type rt_swapptr
;! requires  prg_vtypes prg_vtypes_count rt_swap8 rt_error
;! group     memory
;! clobbers  D0-D3/A0-A1
;! doc       TYPE(p) and SWAP *p,a() (probe19): the variable table of the program (address, TYPE number).

; D0 = address -> D0 = TYPE number of the variable there (probe19 T5), -1 if it is none
rt_type:
        LEA     prg_vtypes,A0
        MOVE.L  prg_vtypes_count,D1
        BRA     .n
.l:     CMP.L   (A0),D0
        BEQ     .hit
        ADDQ.L  #6,A0
.n:     SUBQ.L  #1,D1
        BPL     .l
        MOVEQ   #-1,D0
        RTS
.hit:   MOVE.W  4(A0),D0
        EXT.L   D0
        RTS

; D0 = p, D1 = TYPE number of the array a(), A0 = slot of a(): SWAP *p,a(); p must point to an array
; of the same type, else error 68 (var-type mismatch, ours: the original does not check)
rt_swapptr:
        MOVEM.L D0/A0,-(SP)
        MOVE.L  D1,-(SP)
        BSR     rt_type
        CMP.L   (SP)+,D0
        MOVEM.L (SP)+,D0/A0
        BNE     .bad
        MOVE.L  A0,A1
        MOVE.L  D0,A0
        JMP     rt_swap8
.bad:   MOVEQ   #68,D0
        JMP     rt_error
