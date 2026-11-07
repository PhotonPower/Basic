;! module    rt_strfmt
;! provides  rt_str_n rt_str_fmt
;! requires  rt_using_fmt rt_fmt_flt rt_str_newtemp
;! group     string
;! clobbers  D0-D3/A0-A1
;! doc       STR$(x,n) and STR$(x,y,z) (runtime/ref/rt_str_fmt.c) as string temporaries.

; D0:D1 = x, D2 = n -> A0 = temporary: left-padded to n, or the first n characters
; (probe4 T33, probe5 T19)
rt_str_n:
        LINK    A6,#-32
        MOVE.L  D4,-(SP)
        MOVE.L  D2,D4                   ; D4 = n
        LEA     -32(A6),A0
        MOVEQ   #13,D2
        JSR     rt_fmt_flt              ; D0 = len
        TST.L   D4
        BGT     .n
        MOVEQ   #0,D4
.n:     MOVE.L  D0,-(SP)
        MOVE.L  D4,D0
        JSR     rt_str_newtemp          ; A1 = descriptor, A0 = characters
        MOVE.L  (SP)+,D1                ; D1 = len
        MOVE.L  A1,-(SP)
        LEA     -32(A6),A1
        MOVE.L  D4,D0
        SUB.L   D1,D0                   ; D0 = n - len: spaces in front (if > 0)
        BLE     .cut
.sp:    MOVE.B  #' ',(A0)+
        SUBQ.L  #1,D0
        BNE     .sp
        MOVE.L  D1,D0
        BRA     .cn
.cut:   MOVE.L  D4,D0                   ; the first n characters
        BRA     .cn
.c:     MOVE.B  (A1)+,(A0)+
.cn:    SUBQ.L  #1,D0
        BPL     .c
        MOVE.L  (SP)+,A0
        MOVE.L  (SP)+,D4
        UNLK    A6
        RTS

; D0:D1 = x, D2 = y, D3 = z -> A0 = temporary: PRINT USING with y positions, z after the point
rt_str_fmt:
        JSR     rt_using_fmt            ; A0 = characters (static), D0 = count
        MOVEM.L D0/A0,-(SP)
        JSR     rt_str_newtemp
        MOVE.L  A1,D1
        MOVEM.L (SP)+,D0/A1
        BRA     .n
.c:     MOVE.B  (A1)+,(A0)+
.n:     SUBQ.L  #1,D0
        BPL     .c
        MOVE.L  D1,A0
        RTS
