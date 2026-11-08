;! module    plat_start
;! provides  plat_start
;! requires  main plat_exit
;! group     core
;! clobbers  -
;! doc       z80-test: entry point at &4000, stack from &FFF0 downwards, calls main, then plat_exit.

plat_start:
        LD      SP,&FFF0
        CALL    main
        JP      plat_exit
