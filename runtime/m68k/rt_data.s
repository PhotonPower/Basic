;! module    rt_data
;! provides  rt_data_str rt_data_num rt_data_restore
;! requires  rt_error rt_val prg_data prg_data_count
;! group     core
;! clobbers  D0-D3/A0-A1
;! doc       DATA, READ, RESTORE: items as descriptors in the program's table prg_data.

; The program provides prg_data (8 bytes per item: address, length (word), 1 if quoted (word)) and
; prg_data_count (long). Errors: 34 out of data, 35 data not numeric (the item stays the next one).

; -> A0 = descriptor of the next item (for a string variable)
rt_data_str:
        BSR     rt_data__item
        BCS     .out
        ADDQ.L  #1,rt_data__pos
.out:   RTS

; -> D0:D1 = value of the next item
rt_data_num:
        MOVE.L  A2,-(SP)
        BSR     rt_data__item
        BCS     .zero
        MOVE.L  A0,A2
        TST.W   6(A2)                   ; quoted or empty: not numeric
        BNE     .nn
        MOVEQ   #0,D0
        MOVE.W  4(A2),D0
        BEQ     .nn
        MOVE.L  (A2),A0
        JSR     rt_val
        CMP.W   4(A2),D2                ; all characters must belong to the number
        BNE     .nn
        ADDQ.L  #1,rt_data__pos
        MOVE.L  (SP)+,A2
        RTS
.nn:    MOVEQ   #35,D0                  ; data not numeric
        JSR     rt_error
.zero:  MOVEQ   #0,D0
        MOVEQ   #0,D1
        MOVE.L  (SP)+,A2
        RTS

; D0 = index of the next item
rt_data_restore:
        MOVE.L  D0,rt_data__pos
        RTS

; -> A0 = descriptor of the item at the read position, carry set (after error 34) if there is none
rt_data__item:
        MOVE.L  rt_data__pos,D0
        CMP.L   prg_data_count,D0
        BCS     .ok
        MOVEQ   #34,D0                  ; out of data
        JSR     rt_error
        LEA     rt_data__empty,A0
        ORI     #$01,CCR
        RTS
.ok:    LSL.L   #3,D0
        LEA     prg_data,A0
        ADD.L   D0,A0
        ANDI    #$FE,CCR
        RTS

;! section data
rt_data__pos:
        DC.L    0
rt_data__empty:
        DC.L    0,0
