;! module    rt_for
;! provides  rt_for_wrap_int rt_for_wrap_word rt_for_wrap_byte rt_for_wrap_bool
;! requires  fp_trunc fp_cmp fp_add fp_sub fp_to_i32
;! group     core
;! clobbers  D0-D3/A0-A1
;! doc       FOR with an integer variable: the new value (D0:D1) wraps like INC (probe2 T13) -> D0.

rt_for_wrap_int:
        BSR     rt_for__wrap
        RTS

rt_for_wrap_word:
        BSR     rt_for__wrap
        EXT.L   D0
        RTS

rt_for_wrap_byte:
        BSR     rt_for__wrap
        AND.L   #$FF,D0
        RTS

rt_for_wrap_bool:
        BSR     rt_for__wrap
        TST.L   D0
        SNE     D0
        EXT.W   D0
        EXT.L   D0
        RTS

; truncate, bring into -2^31 .. 2^31-1 by +/- 2^32 (|x| < 2^32), convert
rt_for__wrap:
        MOVEM.L D4-D5,-(SP)
        JSR     fp_trunc
        MOVE.L  D0,D4
        MOVE.L  D1,D5
        MOVE.L  #$41E00000,D2           ; >= 2^31: - 2^32
        MOVEQ   #0,D3
        JSR     fp_cmp
        TST.L   D0
        BLT     .low
        MOVE.L  D4,D0
        MOVE.L  D5,D1
        MOVE.L  #$41F00000,D2
        MOVEQ   #0,D3
        JSR     fp_sub
        BRA     .conv
.low:   MOVE.L  D4,D0                   ; < -2^31: + 2^32
        MOVE.L  D5,D1
        MOVE.L  #$C1E00000,D2
        MOVEQ   #0,D3
        JSR     fp_cmp
        MOVE.L  D0,D2
        MOVE.L  D4,D0
        MOVE.L  D5,D1
        TST.L   D2
        BGE     .conv
        MOVE.L  #$41F00000,D2
        MOVEQ   #0,D3
        JSR     fp_add
.conv:  JSR     fp_to_i32
        MOVEM.L (SP)+,D4-D5
        RTS
