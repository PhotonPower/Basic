;! module    rt_print
;! provides  rt_print_flt rt_print_int rt_print_str rt_print_nl rt_print_comma rt_print_apos rt_print_tab rt_print_spc rt_set_defnum
;! requires  plat_putc rt_fmt_flt fp_from_i32
;! group     print
;! clobbers  D0-D3/A0-A1
;! doc       PRINT: numbers (DEFNUM), strings, separators; the column since the last line feed (POS).

; rt_print_flt: D0:D1.  rt_print_int: D0.  rt_print_str: A0 = text, D0.L = length.
; rt_print_tab, rt_print_spc, rt_set_defnum: D0.L.

rt_print_int:
        JSR     fp_from_i32
rt_print_flt:
        LINK    A6,#-32
        LEA     -32(A6),A0
        MOVE.W  rt_print__defnum,D2
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

; ',': spaces up to the next column divisible by 16
rt_print_comma:
        MOVE.W  rt_print__col,D0
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
        MOVE.W  rt_print__col,D0
        CMP.L   D0,D4
        BGE     .pad
        BSR     rt_print_nl
.pad:   MOVEQ   #0,D0
        MOVE.W  rt_print__col,D0
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
.hi:    MOVE.W  D0,rt_print__defnum
        RTS

rt_print__putc:
        CMP.B   #10,D0
        BNE     .col
        CLR.W   rt_print__col
        JMP     plat_putc
.col:   ADDQ.W  #1,rt_print__col
        JMP     plat_putc

;! section data
rt_print__col:
        DC.W    0
rt_print__defnum:
        DC.W    13
