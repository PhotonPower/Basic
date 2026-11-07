;! module    rt_matprint
;! provides  rt_mat_print
;! requires  rt_mat_v rt_mat_get rt_print_flt rt_print_str rt_print_nl rt_print_sdesc rt_str_fmt rt_str_mark rt_str_release
;! group     matrix
;! clobbers  D0-D3/A0-A1
;! doc       MAT PRINT (D23, probe17): view 0 of rt_mat, elements with ",", lines CR LF.

; MAT PRINT view 0: D0 = 1 with format (D1 = g, D2 = n: STR$(x,g,n)), elements with ',', lines CR LF
rt_mat_print:
        MOVEM.L D4-D7,-(SP)
        MOVE.L  D0,D4
        MOVE.L  D1,rt_matprint__g
        MOVE.L  D2,rt_matprint__n
        MOVEQ   #0,D6
        BRA     .in
.i:     MOVEQ   #0,D7
        BRA     .jn
.j:     TST.L   D7
        BEQ     .x
        LEA     rt_matprint__comma,A0
        MOVEQ   #1,D0
        JSR     rt_print_str
.x:     MOVEQ   #0,D0
        MOVE.L  D6,D1
        MOVE.L  D7,D2
        BSR     rt_mat_get
        TST.L   D4
        BNE     .fmt
        JSR     rt_print_flt
        BRA     .nx
.fmt:   MOVEM.L D0-D1,-(SP)
        JSR     rt_str_mark
        MOVE.L  D0,D5
        MOVEM.L (SP)+,D0-D1
        MOVE.L  rt_matprint__g,D2
        MOVE.L  rt_matprint__n,D3
        JSR     rt_str_fmt
        JSR     rt_print_sdesc
        MOVE.L  D5,D0
        JSR     rt_str_release
.nx:    ADDQ.L  #1,D7
.jn:    CMP.L   rt_mat_v+8,D7
        BLT     .j
        LEA     rt_matprint__cr,A0
        MOVEQ   #1,D0
        JSR     rt_print_str
        JSR     rt_print_nl
        ADDQ.L  #1,D6
.in:    CMP.L   rt_mat_v+4,D6
        BLT     .i
        MOVEM.L (SP)+,D4-D7
        RTS

rt_matprint__comma:
        DC.B    ','
rt_matprint__cr:
        DC.B    13
        EVEN

;! section bss
rt_matprint__g:
        DS.L    1
rt_matprint__n:
        DS.L    1
