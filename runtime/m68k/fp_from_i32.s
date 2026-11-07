;! module    fp_from_i32
;! provides  fp_from_i32
;! requires  fp_round_pack
;! group     float
;! clobbers  D0-D3/A0-A1
;! doc       Exact conversion of the 32-bit integer in D0 to binary64 in D0:D1.

fp_from_i32:
        MOVE.L  D0,D3                   ; sign
        BPL     .pos
        NEG.L   D0                      ; -2^31 stays $80000000 = 2^31 unsigned
.pos:   MOVE.L  D0,D1
        MOVEQ   #0,D0
        MOVEQ   #0,D2                   ; m = |v| * 2^0
        JMP     fp_round_pack           ; exact, no overflow possible
