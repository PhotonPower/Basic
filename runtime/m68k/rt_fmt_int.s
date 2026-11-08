;! module    rt_fmt_int
;! provides  rt_fmt_int
;! group     core
;! clobbers  D0-D3/A0-A1
;! doc       A 32-bit integer as decimal text ("-" in front of a negative one), as PRINT and STR$ write it with DEFNUM 13.

; A0 = buffer (11 bytes), D0 = integer -> D0 = length
rt_fmt_int:
        MOVE.L  A0,A1
        MOVE.L  D0,D1
        BPL     .pos
        MOVE.B  #'-',(A1)+
        NEG.L   D1                      ; -2^31 stays as unsigned 2^31
.pos:   MOVEQ   #0,D3                   ; D3 = number of digits (on the stack)
.d:     MOVEQ   #0,D2                   ; D1 / 10 in two steps of DIVU, the remainder is the digit
        SWAP    D1
        MOVE.W  D1,D2
        DIVU    #10,D2
        MOVE.W  D2,D1
        SWAP    D1
        MOVE.W  D1,D2
        DIVU    #10,D2
        MOVE.W  D2,D1
        SWAP    D2
        ADD.B   #'0',D2
        MOVE.W  D2,-(SP)
        ADDQ.L  #1,D3
        TST.L   D1
        BNE     .d
.o:     MOVE.W  (SP)+,D2
        MOVE.B  D2,(A1)+
        SUBQ.L  #1,D3
        BNE     .o
        MOVE.L  A1,D0
        SUB.L   A0,D0
        RTS
