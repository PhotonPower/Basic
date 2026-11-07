;! module    plat_exit
;! provides  plat_exit
;! group     core
;! doc       m68k-test: ends the program (STOP).

plat_exit:
        STOP    #$2700
        BRA     plat_exit
