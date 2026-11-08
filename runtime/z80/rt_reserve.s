;! module    rt_reserve
;! provides  rt_reserve rt_fre
;! group     core
;! clobbers  BC DE HL
;! doc       RESERVE n / FRE (D27): the size of the BASIC memory, without accounting of what is used.

; DE -> n: RESERVE n (the variables are cleared by the compiled code)
rt_reserve:
        EX      DE,HL
        LD      DE,rt_reserve__size
        LD      BC,4
        LDIR
        RET

; HL = result: FRE(x) / FRE(): the reserved size
rt_fre:
        EX      DE,HL
        LD      HL,rt_reserve__size
        LD      BC,4
        LDIR
        RET

;! section data
rt_reserve__size:
        DW      61440,0
