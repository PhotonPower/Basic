;! module    rt_strval
;! provides  rt_str_val rt_str_valq
;! requires  rt_val
;! group     string
;! clobbers  D0-D3/A0-A1
;! doc       VAL(a$) and VAL?(a$) on string descriptors.

; A0 = descriptor -> D0:D1 = VAL
rt_str_val:
        MOVEQ   #0,D0
        MOVE.W  4(A0),D0
        MOVE.L  (A0),A0
        JMP     rt_val

; A0 = descriptor -> D0 = VAL? (characters that VAL reads)
rt_str_valq:
        MOVEQ   #0,D0
        MOVE.W  4(A0),D0
        MOVE.L  (A0),A0
        JSR     rt_val
        MOVE.L  D2,D0
        RTS
