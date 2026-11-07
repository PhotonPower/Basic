;! module    fp_cmp
;! provides  fp_cmp
;! group     float
;! clobbers  D0-D3/A0-A1
;! doc       Exact comparison: D0:D1 against D2:D3 -> D0.L = -1, 0 or 1 (runtime/ref/fp_cmp.c).

fp_cmp:
        MOVE.L  D4,-(SP)
        MOVE.L  D0,D4                   ; exponent field 0 counts as +0
        AND.L   #$7FF00000,D4
        BNE     .a
        MOVEQ   #0,D0
        MOVEQ   #0,D1
.a:     MOVE.L  D2,D4
        AND.L   #$7FF00000,D4
        BNE     .b
        MOVEQ   #0,D2
        MOVEQ   #0,D3
.b:     TST.L   D0                      ; order-preserving keys: negative -> all bits inverted,
        BPL     .ap                     ; positive -> sign bit set; then compare unsigned
        NOT.L   D0
        NOT.L   D1
        BRA     .ak
.ap:    BSET    #31,D0
.ak:    TST.L   D2
        BPL     .bp
        NOT.L   D2
        NOT.L   D3
        BRA     .bk
.bp:    BSET    #31,D2
.bk:    CMP.L   D2,D0
        BHI     .gt
        BCS     .lt
        CMP.L   D3,D1
        BHI     .gt
        BCS     .lt
        MOVEQ   #0,D0
        BRA     .out
.gt:    MOVEQ   #1,D0
        BRA     .out
.lt:    MOVEQ   #-1,D0
.out:   MOVE.L  (SP)+,D4
        RTS
