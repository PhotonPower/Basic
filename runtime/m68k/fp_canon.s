;! module    fp_canon
;! provides  fp_canon
;! requires  rt_error
;! group     float
;! clobbers  D0-D3/A0-A1
;! doc       Normalises a binary64 read from memory: denormal and -0 -> 0, infinity/NaN -> error 1.

fp_canon:
        MOVE.L  D0,D2
        AND.L   #$7FF00000,D2
        BEQ     .zero
        CMP.L   #$7FF00000,D2
        BNE     .ok
        MOVEQ   #1,D0                   ; RT_ERR_OVERFLOW
        JSR     rt_error
.zero:  MOVEQ   #0,D0
        MOVEQ   #0,D1
.ok:    RTS
