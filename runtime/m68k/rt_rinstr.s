;! module    rt_rinstr
;! provides  rt_str_rinstr
;! group     string
;! clobbers  D0-D3/A0-A1
;! doc       RINSTR (probe20 T6): the last b$ that ends before position x, i.e. in LEFT$(a$,x-1).

; A0 = a, A1 = b, D0 = x (LEN(a$)+1 if not given) -> D0 = position (0: none; an empty a$: 0)
rt_str_rinstr:
        MOVEM.L D4-D5/A2-A3,-(SP)
        MOVEQ   #0,D2
        MOVE.W  4(A0),D2                ; D2 = n
        BEQ     .none
        MOVEQ   #0,D3
        MOVE.W  4(A1),D3                ; D3 = m
        MOVE.L  (A0),A2
        MOVE.L  (A1),A3
        SUB.L   D3,D0                   ; s = min(x - m, n - m + 1)
        MOVE.L  D2,D1
        SUB.L   D3,D1
        ADDQ.L  #1,D1
        CMP.L   D1,D0
        BLE     .s
        MOVE.L  D1,D0
.s:     TST.L   D0
        BLE     .none
        LEA     -1(A2,D0.L),A0          ; compare m characters at s
        MOVE.L  A3,A1
        MOVE.L  D3,D5
        BRA     .cn
.c:     CMPM.B  (A1)+,(A0)+
        BNE     .no
.cn:    SUBQ.L  #1,D5
        BPL     .c
        BRA     .out                    ; found: D0 = s
.no:    SUBQ.L  #1,D0
        BRA     .s
.none:  MOVEQ   #0,D0
.out:   MOVEM.L (SP)+,D4-D5/A2-A3
        RTS
