;! module    plat_start
;! provides  plat_start plat_dosbase plat_out plat_in plat_sp plat_rc
;! requires  main plat_exit plat_alloc
;! group     core
;! doc       amiga: AmigaDOS entry point; own stack, opens dos.library, gets Output(), calls main, then plat_exit.

; exec.library: OpenLibrary -552; dos.library: Output -60. The program gets its own stack of
; 64 KiB: the shell gives only 4000 bytes by default, and the frames of the generated code are
; large (8 bytes per virtual register).

plat_start__STACK EQU 65536

plat_start:
        MOVEM.L D2-D7/A2-A6,-(SP)       ; registers of the caller (the shell) back at the end
        MOVE.L  SP,plat_sp
        CLR.L   plat_rc
        MOVE.L  4,A6                    ; ExecBase
        LEA     plat_start__dosname,A1
        MOVEQ   #0,D0
        JSR     -552(A6)                ; OpenLibrary("dos.library", 0)
        MOVE.L  D0,plat_dosbase
        BEQ     .fail
        MOVE.L  D0,A6
        JSR     -60(A6)                 ; Output()
        MOVE.L  D0,plat_out
        JSR     -54(A6)                 ; Input()
        MOVE.L  D0,plat_in
        MOVE.L  #plat_start__STACK,D0
        JSR     plat_alloc
        MOVE.L  A0,D0
        BEQ     .nomem
        ADD.L   #plat_start__STACK,A0
        MOVE.L  A0,SP
        JSR     main
        JMP     plat_exit
.nomem: MOVE.L  4,A6
        MOVE.L  plat_dosbase,A1
        JSR     -414(A6)                ; CloseLibrary
.fail:  MOVE.L  plat_sp,SP
        MOVEM.L (SP)+,D2-D7/A2-A6
        MOVEQ   #20,D0                  ; RETURN_FAIL
        RTS

;! section data
plat_start__dosname:
        DC.B    'dos.library',0
        EVEN
plat_dosbase:
        DC.L    0
plat_out:
        DC.L    0
plat_sp:
        DC.L    0
plat_rc:
        DC.L    0
plat_in:
        DC.L    0
