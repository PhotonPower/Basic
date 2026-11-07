;! module    plat_start
;! provides  plat_start
;! requires  main plat_exit
;! group     core
;! doc       m68k-test: entry point at $1000, stack from $80000 downwards, calls main, then plat_exit.

plat_start:
        LEA     $80000,SP
        BSR     main
        BRA     plat_exit
