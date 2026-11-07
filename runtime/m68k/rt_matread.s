;! module    rt_matread
;! provides  rt_mat_read
;! requires  rt_mat_v rt_mat_el rt_data_num
;! group     matrix
;! clobbers  D0-D3/A0-A1
;! doc       MAT READ (D23): view 0 of rt_mat row by row from DATA.

; MAT READ: view 0 row by row from DATA
rt_mat_read:
        MOVEM.L D6-D7/A2,-(SP)
        MOVEQ   #0,D6
        BRA     .in
.i:     MOVEQ   #0,D7
        BRA     .jn
.j:     JSR     rt_data_num
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
        MOVEM.L (SP)+,D6-D7/A2
        RTS
