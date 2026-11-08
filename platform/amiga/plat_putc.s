;! module    plat_putc
;! provides  plat_putc plat_flush plat_putc_gfx
;! requires  plat_dosbase plat_out
;! group     core
;! clobbers  -
;! doc       amiga: buffered output through dos.library Write() to Output(); flushed at a line feed.

plat_putc__SIZE EQU 256

; D0.B = character (all registers are kept)
plat_putc:
        MOVE.L  A0,-(SP)
        MOVE.L  D1,-(SP)
        MOVE.L  plat_putc__n,D1
        LEA     plat_putc__buf,A0
        MOVE.B  D0,0(A0,D1.L)
        ADDQ.L  #1,D1
        MOVE.L  D1,plat_putc__n
        MOVE.L  (SP)+,D1
        MOVE.L  plat_putc_gfx,A0        ; graphics open: also into the current window (plat_gfx)
        CMP.L   #0,A0
        BEQ     .nogfx
        JSR     (A0)
.nogfx: MOVE.L  (SP)+,A0
        CMP.B   #10,D0
        BEQ     plat_flush
        CMP.L   #plat_putc__SIZE,plat_putc__n
        BCC     plat_flush
        RTS

; writes the buffer (all registers are kept); dos.library Write -48: D1 = file, D2 = buffer, D3 = length
plat_flush:
        MOVEM.L D0-D3/A0-A1/A6,-(SP)
        MOVE.L  plat_putc__n,D3
        BEQ     .out
        CLR.L   plat_putc__n
        MOVE.L  plat_out,D1
        BEQ     .out                    ; no output (started from the Workbench)
        MOVE.L  #plat_putc__buf,D2
        MOVE.L  plat_dosbase,A6
        JSR     -48(A6)
.out:   MOVEM.L (SP)+,D0-D3/A0-A1/A6
        RTS

;! section data
plat_putc__n:
        DC.L    0
plat_putc_gfx:
        DC.L    0

;! section bss
plat_putc__buf:
        DS.B    plat_putc__SIZE
