;! module    fp_abs
;! provides  fp_abs
;! group     float
;! clobbers  D0-D3/A0-A1
;! doc       ABS: clears the sign bit (D0:D1 -> D0:D1).

fp_abs:
        BCLR    #31,D0
        RTS
