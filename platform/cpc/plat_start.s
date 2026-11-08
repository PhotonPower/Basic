;! module    plat_start
;! provides  plat_start plat_exit
;! requires  main prg_bss prg_end
;! group     core
;! clobbers  -
;! doc       cpc: entry (CALL or RUN): own stack below HIMEM, BSS cleared; plat_exit back to the caller.

plat_start__top EQU &A600               ; the stack (1 KB) below HIMEM of a 6128 with AMSDOS (&A67B)

plat_start:
        LD      (plat_start__sp),SP
        LD      SP,plat_start__top
        LD      HL,prg_bss              ; variables, runtime state: 0
        LD      BC,prg_end-prg_bss
plat_start__clr:
        LD      A,B
        OR      C
        JR      Z,plat_start__go
        LD      (HL),0
        INC     HL
        DEC     BC
        JR      plat_start__clr
plat_start__go:
        CALL    main
plat_exit:
        LD      SP,(plat_start__sp)
        RET

;! section data
plat_start__sp:
        DW      0
