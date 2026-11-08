;! module    rt_print
;! provides  rt_print_flt rt_print_int rt_print_str rt_print_nl rt_print_comma rt_print_apos rt_print_tab rt_print_spc rt_set_defnum rt_defnum rt_print_out rt_print_col
;! provides  rt_print_begin rt_print_at rt_print_locate rt_print_htab rt_print_vtab rt_print_cls rt_pos rt_crscol rt_crslin
;! requires  plat_putc rt_fmt_flt fp_from_i32 rt_fmt_int
;! group     print
;! clobbers  D0-D3/A0-A1
;! doc       PRINT: numbers (DEFNUM), strings, separators; the column since the last line feed (POS).

; rt_print_flt: D0:D1.  rt_print_int: D0.  rt_print_str: A0 = text, D0.L = length.
; rt_print_tab, rt_print_spc, rt_set_defnum: D0.L.
; Every character goes to the routine in rt_print_out (plat_putc; PRINT # sets a file, rt_file);
; rt_print_col is the column of the current output (POS, probe20 T1: a LF sets it to 0, a BS takes
; one back, every other character counts, also CR). rt_print__zone counts the characters since the
; start of the PRINT statement (rt_print_begin): ',' pads to 16 of them (probe21). The cursor
; (CRSCOL, CRSLIN) follows the screen output only; PRINT AT, LOCATE, HTAB, VTAB and CLS move it with
; ANSI sequences (ESC [ line ; column H, ESC [ H ESC [ J). That routine must keep all registers.

rt_print_int:
        CMP.W   #13,rt_defnum           ; DEFNUM 13: the digits (below: through the float, rounded)
        BGE     .dig
        JSR     fp_from_i32
        BRA     rt_print_flt
.dig:   LINK    A6,#-12
        LEA     -12(A6),A0
        JSR     rt_fmt_int
        LEA     -12(A6),A0
        BSR     rt_print_str
        UNLK    A6
        RTS
rt_print_flt:
        LINK    A6,#-32
        LEA     -32(A6),A0
        MOVE.W  rt_defnum,D2
        JSR     rt_fmt_flt
        LEA     -32(A6),A0
        BSR     rt_print_str
        UNLK    A6
        RTS

rt_print_str:
        MOVE.L  D4,-(SP)
        MOVE.L  D0,D4
        BRA     .next
.loop:  MOVE.B  (A0)+,D0
        BSR     rt_print__putc
.next:  SUBQ.L  #1,D4
        BPL     .loop
        MOVE.L  (SP)+,D4
        RTS

rt_print_nl:
        MOVEQ   #10,D0
        BRA     rt_print__putc

rt_print_apos:
        MOVEQ   #' ',D0
        BRA     rt_print__putc

; ',': spaces up to the next multiple of 16 characters of this PRINT statement
rt_print_comma:
        MOVE.W  rt_print__zone,D0
        AND.W   #15,D0
        MOVEQ   #16,D1
        SUB.W   D0,D1
        MOVE.L  D1,D0
        EXT.L   D0
        ; fall through: D0 spaces

rt_print_spc:
        MOVE.L  D4,-(SP)
        MOVE.L  D0,D4
        BRA     .next
.loop:  MOVEQ   #' ',D0
        BSR     rt_print__putc
.next:  SUBQ.L  #1,D4
        BPL     .loop
        MOVE.L  (SP)+,D4
        RTS

; TAB(n): spaces up to column n; a new line first if the cursor is beyond it
rt_print_tab:
        MOVE.L  D4,-(SP)
        MOVE.L  D0,D4
        MOVEQ   #0,D0
        MOVE.W  rt_print_col,D0
        CMP.L   D0,D4
        BGE     .pad
        BSR     rt_print_nl
.pad:   MOVEQ   #0,D0
        MOVE.W  rt_print_col,D0
        CMP.L   D0,D4
        BLE     .done
        MOVEQ   #' ',D0
        BSR     rt_print__putc
        BRA     .pad
.done:  MOVE.L  (SP)+,D4
        RTS

rt_set_defnum:
        CMP.L   #2,D0
        BGE     .lo
        MOVEQ   #2,D0
.lo:    CMP.L   #13,D0
        BLE     .hi
        MOVEQ   #13,D0
.hi:    MOVE.W  D0,rt_defnum
        RTS

rt_print__putc:
        CMP.B   #10,D0
        BEQ     .lf
        CMP.B   #8,D0
        BEQ     .bs
        ADDQ.W  #1,rt_print_col
        ADDQ.W  #1,rt_print__zone
        CMP.L   #plat_putc,rt_print_out
        BNE     .out
        CMP.B   #13,D0
        BEQ     .cr
        ADDQ.W  #1,rt_print__ccol
        BRA     .out
.cr:    MOVE.W  #1,rt_print__ccol
        BRA     .out
.lf:    CLR.W   rt_print_col
        CLR.W   rt_print__zone
        CMP.L   #plat_putc,rt_print_out
        BNE     .out
        MOVE.W  #1,rt_print__ccol
        ADDQ.W  #1,rt_print__clin
        BRA     .out
.bs:    TST.W   rt_print_col
        BEQ     .b1
        SUBQ.W  #1,rt_print_col
.b1:    TST.W   rt_print__zone
        BEQ     .b2
        SUBQ.W  #1,rt_print__zone
.b2:    CMP.L   #plat_putc,rt_print_out
        BNE     .out
        CMP.W   #1,rt_print__ccol
        BLE     .out
        SUBQ.W  #1,rt_print__ccol
.out:   MOVE.L  rt_print_out,-(SP)      ; jump to the output routine (registers unchanged)
        RTS

; D0 = character: to the output without counting (positioning sequences)
rt_print__raw:
        MOVE.L  rt_print_out,-(SP)
        RTS

; D1 = 1..32767: its decimal digits, raw
rt_print__num:
        MOVE.L  D2,-(SP)
        MOVEQ   #0,D2                   ; digits on the stack
.d:     AND.L   #$FFFF,D1
        DIVU    #10,D1
        SWAP    D1
        MOVE.W  D1,-(SP)
        CLR.W   D1
        SWAP    D1
        ADDQ.L  #1,D2
        TST.L   D1
        BNE     .d
.o:     MOVE.W  (SP)+,D0
        ADD.B   #'0',D0
        BSR     rt_print__raw
        SUBQ.L  #1,D2
        BNE     .o
        MOVE.L  (SP)+,D2
        RTS

; D0 -> D0 limited to 1..32767
rt_print__clamp:
        CMP.L   #1,D0
        BGE     .a
        MOVEQ   #1,D0
.a:     CMP.L   #32767,D0
        BLE     .b
        MOVE.L  #32767,D0
.b:     RTS

; PRINT before the first item of a statement with ','
rt_print_begin:
        CLR.W   rt_print__zone
        RTS

; D0 = column, D1 = line: LOCATE (POS stays)
rt_print_locate:
        BSR     rt_print__clamp
        MOVE.W  D0,rt_print__ccol
        MOVE.L  D1,D0
        BSR     rt_print__clamp
        MOVE.W  D0,rt_print__clin
rt_print__goto:                         ; ESC [ line ; column H
        MOVEQ   #27,D0
        BSR     rt_print__raw
        MOVEQ   #'[',D0
        BSR     rt_print__raw
        MOVEQ   #0,D1
        MOVE.W  rt_print__clin,D1
        BSR     rt_print__num
        MOVEQ   #';',D0
        BSR     rt_print__raw
        MOVEQ   #0,D1
        MOVE.W  rt_print__ccol,D1
        BSR     rt_print__num
        MOVEQ   #'H',D0
        BRA     rt_print__raw

; D0 = column, D1 = line: PRINT AT (POS 0)
rt_print_at:
        BSR     rt_print_locate
        CLR.W   rt_print_col
        RTS

; D0 = column: HTAB
rt_print_htab:
        BSR     rt_print__clamp
        MOVE.W  D0,rt_print__ccol
        BRA     rt_print__goto

; D0 = line: VTAB
rt_print_vtab:
        BSR     rt_print__clamp
        MOVE.W  D0,rt_print__clin
        BRA     rt_print__goto

; CLS: ESC [ H ESC [ J, the cursor home, POS 0
rt_print_cls:
        LEA     rt_print__clstext,A0
        MOVEQ   #6,D1
.c:     MOVE.B  (A0)+,D0
        BSR     rt_print__raw
        SUBQ.L  #1,D1
        BNE     .c
        MOVE.W  #1,rt_print__ccol
        MOVE.W  #1,rt_print__clin
        CLR.W   rt_print_col
        RTS

; -> D0 = POS(x), CRSCOL, CRSLIN
rt_pos:
        MOVEQ   #0,D0
        MOVE.B  rt_print_col+1,D0
        RTS
rt_crscol:
        MOVEQ   #0,D0
        MOVE.W  rt_print__ccol,D0
        RTS
rt_crslin:
        MOVEQ   #0,D0
        MOVE.W  rt_print__clin,D0
        RTS

rt_print__clstext:
        DC.B    27,'[H',27,'[J'
        EVEN

;! section data
rt_print_out:
        DC.L    plat_putc
rt_print_col:
        DC.W    0
rt_print__zone:
        DC.W    0
rt_print__ccol:
        DC.W    1
rt_print__clin:
        DC.W    1
rt_defnum:
        DC.W    13
