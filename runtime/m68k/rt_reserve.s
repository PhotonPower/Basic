;! module    rt_reserve
;! provides  rt_reserve rt_fre
;! group     core
;! clobbers  D0
;! doc       RESERVE n / FRE (D27): the size of the BASIC memory, without accounting of what is used.

; D0 = n: RESERVE n (the variables are cleared by the compiled code, probe25 T2)
rt_reserve:
        MOVE.L  D0,rt_reserve__size
        RTS

; -> D0 = FRE(x) / FRE(): the reserved size (before any RESERVE: about what the original shows for
; a small program, probe25 T1)
rt_fre:
        MOVE.L  rt_reserve__size,D0
        RTS

;! section data
rt_reserve__size:
        DC.L    61440
