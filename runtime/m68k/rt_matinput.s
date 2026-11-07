;! module    rt_matinput
;! provides  rt_mat_input
;! requires  rt_mat_v rt_mat_el rt_file_num
;! group     matrix
;! clobbers  D0-D3/A0-A1
;! doc       MAT INPUT #c (D23, probe18): view 0 of rt_mat row by row, numbers like INPUT #.

; D0 = channel: MAT INPUT #c into view 0, row by row, numbers like INPUT # (probe18); after an
; error the values read so far stay
rt_mat_input:
        MOVEM.L D5-D7,-(SP)
        MOVE.L  D0,D5
        MOVEQ   #0,D6
        BRA     .in
.i:     MOVEQ   #0,D7
        BRA     .jn
.j:     MOVE.L  D5,D0
        JSR     rt_file_num
        MOVEM.L D0-D1,-(SP)
        LEA     rt_mat_v,A0
        MOVE.L  D6,D0
        MOVE.L  D7,D1
        BSR     rt_mat_el
        MOVE.L  (SP)+,(A1)
        MOVE.L  (SP)+,4(A1)
        ADDQ.L  #1,D7
.jn:    CMP.L   rt_mat_v+8,D7
        BLT     .j
        ADDQ.L  #1,D6
.in:    CMP.L   rt_mat_v+4,D6
        BLT     .i
        MOVEM.L (SP)+,D5-D7
        RTS
