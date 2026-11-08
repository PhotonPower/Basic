;! module    rt_str_int
;! provides  rt_str_int
;! requires  rt_fmt_int rt_defnum rt_str_str rt_str_newtemp fp_from_i32
;! group     string
;! clobbers  D0-D3/A0-A1
;! doc       STR$ of an integer: its digits with DEFNUM 13, else as STR$ of the float (rounded to DEFNUM digits).

; D0 = integer -> A0 = temporary string
rt_str_int:
        CMP.W   #13,rt_defnum
        BGE     .dig
        JSR     fp_from_i32
        JMP     rt_str_str
.dig:   LINK    A6,#-12
        LEA     -12(A6),A0
        JSR     rt_fmt_int              ; D0 = length
        MOVE.L  D0,-(SP)
        JSR     rt_str_newtemp          ; A1 = descriptor, A0 = characters
        MOVE.L  (SP)+,D0
        MOVE.L  A1,D2                   ; D2 = descriptor
        MOVE.L  A0,A1                   ; copy the digits there
        LEA     -12(A6),A0
        BRA     .n
.l:     MOVE.B  (A0)+,(A1)+
.n:     SUBQ.L  #1,D0
        BPL     .l
        MOVE.L  D2,A0
        UNLK    A6
        RTS
