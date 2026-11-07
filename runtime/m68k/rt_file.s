;! module    rt_file
;! provides  rt_file_open rt_file_close rt_file_close_all rt_file_select rt_file_deselect rt_file_line rt_file_item rt_file_num rt_file_chars rt_file_eof rt_file_lof rt_file_exist rt_file_kill rt_write_str
;! requires  rt_error rt_err_hook plat_exit_hook rt_print_out rt_print_col rt_print_str rt_print_sdesc rt_str_newtemp rt_val plat_alloc plat_fopen plat_fread plat_fwrite plat_fclose plat_fsize plat_fdelete plat_fexists
;! group     file-seq
;! clobbers  D0-D3/A0-A1
;! doc       Sequential files (probe14): channels 0-99, buffered, PRINT # through the output vector of rt_print.

; Channels are numbers (D0) after the conversion to an integer; strings are descriptors (A0, A1).
; Channel record (16 bytes): +0 handle of the platform (0: closed), +4 buffer (512 bytes), +8 flags
; (bit 0: input), +10 column (PRINT #), +12 read position, +14 bytes in the buffer.
; The platform (plat_f*) opens, reads, writes, closes, deletes; errors are AmigaDOS codes
; (205 object not found, 202 object in use; ERR shows them as -51, -54).

rt_file__BUF  EQU 512
rt_file__BUFS EQU 16                    ; open files at the same time
rt_file__MAXS EQU 32767                 ; longest string read

; ---- channels ---------------------------------------------------------------------------------------

; D0 = channel -> A2 = record, carry set (after error 23) if outside 0..99
rt_file__chan:
        CMP.L   #99,D0
        BHI     .bad
        LSL.L   #4,D0
        LEA     rt_file__ch,A2
        ADD.L   D0,A2
        ANDI    #$FE,CCR
        RTS
.bad:   MOVEQ   #23,D0                  ; file # wrong
        JSR     rt_error
        ORI     #$01,CCR
        RTS

; D0 = channel -> A2 = open record, carry set after error 23 or 24
rt_file__open:
        BSR     rt_file__chan
        BCS     .out
        TST.L   (A2)
        BNE     .out
        MOVEQ   #24,D0                  ; file not open
        JSR     rt_error
        ORI     #$01,CCR
.out:   RTS

; D0 = channel -> A2 = record open for input, carry set after an error (24 for an output file)
rt_file__in:
        BSR     rt_file__open
        BCS     .out
        BTST    #0,9(A2)
        BNE     .out
        MOVEQ   #24,D0
        JSR     rt_error
        ORI     #$01,CCR
.out:   RTS

; A0 = mode descriptor, D0 = channel, A1 = name descriptor: OPEN
rt_file_open:
        MOVEM.L D2-D4/A2-A4,-(SP)
        MOVE.L  A0,A3
        MOVE.L  A1,A4
        BSR     rt_file__chan
        BCS     .out
        MOVEQ   #0,D3                   ; mode: first character, upper case
        TST.W   4(A3)
        BEQ     .badm
        MOVE.L  (A3),A0
        MOVE.B  (A0),D3
        AND.B   #$DF,D3
        MOVEQ   #0,D4                   ; D4 = 0 input, 1 output, 2 append
        CMP.B   #'I',D3
        BEQ     .m
        MOVEQ   #1,D4
        CMP.B   #'O',D3
        BEQ     .m
        MOVEQ   #2,D4
        CMP.B   #'A',D3
        BEQ     .m
.badm:  MOVEQ   #21,D0                  ; open allows i o r a u (r and u: not yet)
        JSR     rt_error
        BRA     .out
.m:     TST.L   (A2)
        BEQ     .free
        MOVEQ   #22,D0                  ; file already open
        JSR     rt_error
        BRA     .out
.free:  LEA     rt_file__busy,A0        ; a free buffer
        MOVEQ   #rt_file__BUFS-1,D1
.bs:    TST.B   (A0)+
        DBEQ    D1,.bs
        BEQ     .got
        MOVEQ   #8,D0                   ; too many open files: out of memory
        JSR     rt_error
        BRA     .out
.got:   SUBQ.L  #1,A0
        MOVE.B  #1,(A0)
        MOVE.L  A0,D1
        SUB.L   #rt_file__busy,D1
        MOVEQ   #9,D0
        LSL.L   D0,D1                   ; * 512
        ADD.L   #rt_file__bufs,D1
        MOVE.L  D1,4(A2)
        MOVE.L  (A4),A0                 ; plat_fopen(name, length, mode)
        MOVEQ   #0,D0
        MOVE.W  4(A4),D0
        MOVE.L  D4,D1
        JSR     plat_fopen
        TST.L   D0
        BNE     .ok
        MOVE.L  D1,-(SP)                ; failed: buffer back, error of the platform
        BSR     rt_file__freebuf
        MOVE.L  (SP)+,D0
        JSR     rt_error
        BRA     .out
.ok:    MOVE.L  D0,(A2)
        CLR.L   8(A2)                   ; flags, column
        CLR.L   12(A2)                  ; position, count
        TST.L   D4
        BNE     .hooks
        BSET    #0,9(A2)
.hooks: MOVE.L  #rt_file_close_all,plat_exit_hook
        MOVE.L  #rt_file_deselect,rt_err_hook
.out:   MOVEM.L (SP)+,D2-D4/A2-A4
        RTS

; A2 = record: its buffer is free again
rt_file__freebuf:
        MOVE.L  4(A2),D0
        SUB.L   #rt_file__bufs,D0
        MOVEQ   #9,D1
        LSR.L   D1,D0
        LEA     rt_file__busy,A0
        CLR.B   0(A0,D0.L)
        RTS

; A2 = output record: write the buffer
rt_file__flush:
        MOVEQ   #0,D1
        MOVE.W  14(A2),D1
        BEQ     .out
        CLR.W   14(A2)
        MOVE.L  D1,-(SP)
        MOVE.L  (A2),D0
        MOVE.L  4(A2),A0
        JSR     plat_fwrite
        CMP.L   (SP)+,D0
        BEQ     .out
        MOVE.L  #221,D0                 ; disk full (AmigaDOS ERROR_DISK_FULL)
        JSR     rt_error
.out:   RTS

; D0 = channel: CLOSE #n (a closed channel: nothing)
rt_file_close:
        MOVE.L  A2,-(SP)
        BSR     rt_file__chan
        BCS     .out
        TST.L   (A2)
        BEQ     .out
        CMP.L   rt_file__cur,A2         ; still the output of a PRINT #?
        BNE     .ns
        BSR     rt_file_deselect
.ns:    BTST    #0,9(A2)
        BNE     .in
        BSR     rt_file__flush
.in:    MOVE.L  (A2),D0
        JSR     plat_fclose
        BSR     rt_file__freebuf
        CLR.L   (A2)
.out:   MOVE.L  (SP)+,A2
        RTS

; CLOSE: all channels
rt_file_close_all:
        MOVE.L  D4,-(SP)
        MOVEQ   #0,D4
.l:     MOVE.L  D4,D0
        BSR     rt_file_close
        ADDQ.L  #1,D4
        CMP.L   #100,D4
        BCS     .l
        MOVE.L  (SP)+,D4
        RTS

; ---- PRINT #, WRITE # --------------------------------------------------------------------------------

; D0 = channel: the following output goes to the file (with the channel's column)
rt_file_select:
        MOVE.L  A2,-(SP)
        BSR     rt_file__open
        BCS     .out
        BTST    #0,9(A2)
        BEQ     .ok
        MOVEQ   #24,D0                  ; an input file: file not open (for output)
        JSR     rt_error
        BRA     .out
.ok:    BSR     rt_file_deselect        ; (an interrupted PRINT # before)
        MOVE.L  rt_print_out,rt_file__saveout
        MOVE.W  rt_print_col,rt_file__savecol
        MOVE.L  A2,rt_file__cur
        MOVE.L  #rt_file__putc,rt_print_out
        MOVE.W  10(A2),rt_print_col
.out:   MOVE.L  (SP)+,A2
        RTS

; end of PRINT #: column back to the channel, buffer written, output to the screen again
rt_file_deselect:
        MOVE.L  A2,-(SP)
        MOVE.L  rt_file__cur,D0
        BEQ     .out
        MOVE.L  D0,A2
        CLR.L   rt_file__cur
        MOVE.W  rt_print_col,10(A2)
        MOVE.L  rt_file__saveout,rt_print_out
        MOVE.W  rt_file__savecol,rt_print_col
        BSR     rt_file__flush
.out:   MOVE.L  (SP)+,A2
        RTS

; D0.B = character into the buffer of the current channel (keeps all registers)
rt_file__putc:
        MOVEM.L D0-D3/A0-A2,-(SP)
        MOVE.L  rt_file__cur,A2
        MOVE.L  4(A2),A0
        MOVE.W  14(A2),D1
        MOVE.B  D0,0(A0,D1.W)
        ADDQ.W  #1,D1
        MOVE.W  D1,14(A2)
        CMP.W   #rt_file__BUF,D1
        BCS     .out
        BSR     rt_file__flush
.out:   MOVEM.L (SP)+,D0-D3/A0-A2
        RTS

; A0 = string descriptor: WRITE prints it in quotes
rt_write_str:
        MOVE.L  A0,-(SP)
        LEA     rt_file__quote,A0
        MOVEQ   #1,D0
        JSR     rt_print_str
        MOVE.L  (SP)+,A0
        JSR     rt_print_sdesc
        LEA     rt_file__quote,A0
        MOVEQ   #1,D0
        JMP     rt_print_str

; ---- reading -----------------------------------------------------------------------------------------

; A2 = input record -> D0 = next byte (0..255), or -1 at the end of the file
rt_file__getb:
        MOVE.W  12(A2),D0
        CMP.W   14(A2),D0
        BCS     .have
        MOVEM.L D2-D3/A1,-(SP)          ; refill (the platform may change D0-D3/A0-A1)
        MOVE.L  (A2),D0
        MOVE.L  4(A2),A0
        MOVE.L  #rt_file__BUF,D1
        JSR     plat_fread
        MOVEM.L (SP)+,D2-D3/A1
        MOVE.W  D0,14(A2)
        CLR.W   12(A2)
        TST.L   D0
        BNE     .refd
        MOVEQ   #-1,D0
        RTS
.refd:  MOVEQ   #0,D0
.have:  MOVE.L  4(A2),A0
        MOVEQ   #0,D1
        MOVE.B  0(A0,D0.W),D1
        ADDQ.W  #1,12(A2)
        MOVE.L  D1,D0
        RTS

; A2 = input record -> D0 = next byte without taking it (-1 at the end)
rt_file__peek:
        BSR     rt_file__getb
        TST.L   D0
        BMI     .out
        SUBQ.W  #1,12(A2)
.out:   RTS

; -> A3 = scratch for the characters read (32 KiB from the platform on first use; 0 if none)
rt_file__scratch:
        MOVE.L  rt_file__sb,A3
        MOVE.L  A3,D0
        BNE     .out
        MOVE.L  #rt_file__MAXS+1,D0
        JSR     plat_alloc
        MOVE.L  A0,rt_file__sb
        MOVE.L  A0,A3
        MOVE.L  A0,D0
        BNE     .out
        MOVEQ   #8,D0                   ; out of memory
        JSR     rt_error
.out:   RTS

; D3 = count of the characters in the scratch (A3) -> A0 = temporary string with them
rt_file__temp:
        MOVE.L  D3,-(SP)
        MOVE.L  D3,D0
        JSR     rt_str_newtemp          ; A1 = descriptor, A0 = characters
        MOVE.L  (SP)+,D3
        MOVE.L  A3,D1
        EXG     D1,A1
        MOVE.L  D3,D0
        BRA     .n
.c:     MOVE.B  (A1)+,(A0)+
.n:     SUBQ.L  #1,D0
        BPL     .c
        MOVE.L  D1,A0
        RTS

; D3 = count, D0 = character: append to the scratch (beyond 32767 characters: dropped)
rt_file__keep:
        CMP.L   #rt_file__MAXS,D3
        BCC     .out
        MOVE.B  D0,0(A3,D3.L)
        ADDQ.L  #1,D3
.out:   RTS

; D0 = channel -> A0 = the next line without the line feed (LINE INPUT #); error 26 at the end
rt_file_line:
        MOVEM.L D3/A2-A3,-(SP)
        BSR     rt_file__in
        BCS     .empty
        BSR     rt_file__peek
        TST.L   D0
        BMI     .eof
        BSR     rt_file__scratch
        MOVEQ   #0,D3
.l:     BSR     rt_file__getb
        TST.L   D0
        BMI     .done
        CMP.B   #10,D0
        BEQ     .done
        BSR     rt_file__keep
        BRA     .l
.done:  BSR     rt_file__temp
        BRA     .out
.eof:   MOVEQ   #26,D0                  ; end of file
        JSR     rt_error
.empty: MOVEQ   #0,D3
        BSR     rt_file__temp
.out:   MOVEM.L (SP)+,D3/A2-A3
        RTS

; A2 = input record -> D3 = characters of the next INPUT # item in the scratch A3; carry set after
; error 26. Up to a comma or the line end; in quotes everything up to the closing quote (then on to
; the comma); unquoted text keeps its spaces (probe14 T4).
rt_file__item0:
        BSR     rt_file__peek
        TST.L   D0
        BPL     .go
        MOVEQ   #26,D0                  ; end of file
        JSR     rt_error
        ORI     #$01,CCR
        RTS
.go:    BSR     rt_file__scratch
        MOVEQ   #0,D3
        MOVE.L  D4,-(SP)
        MOVEQ   #0,D4                   ; D4 = leading spaces
.sp:    BSR     rt_file__getb
        CMP.L   #' ',D0
        BNE     .first
        ADDQ.L  #1,D4
        BRA     .sp
.first: CMP.L   #'"',D0
        BNE     .plain
.q:     BSR     rt_file__getb           ; quoted
        TST.L   D0
        BMI     .end
        CMP.B   #'"',D0
        BEQ     .skip
        CMP.B   #10,D0
        BEQ     .end
        BSR     rt_file__keep
        BRA     .q
.skip:  BSR     rt_file__getb           ; on to the comma or the line end
        TST.L   D0
        BMI     .end
        CMP.B   #',',D0
        BEQ     .end
        CMP.B   #10,D0
        BNE     .skip
        BRA     .end
.plain: MOVE.L  D0,-(SP)                ; the leading spaces belong to the item
        BRA     .ln
.lp:    MOVEQ   #' ',D0
        BSR     rt_file__keep
.ln:    SUBQ.L  #1,D4
        BPL     .lp
        MOVE.L  (SP)+,D0
.pl:    TST.L   D0
        BMI     .end
        CMP.B   #',',D0
        BEQ     .end
        CMP.B   #10,D0
        BEQ     .end
        BSR     rt_file__keep
        BSR     rt_file__getb
        BRA     .pl
.end:   MOVE.L  (SP)+,D4
        ANDI    #$FE,CCR
        RTS

; D0 = channel -> A0 = the next item as a string (INPUT # into a string variable)
rt_file_item:
        MOVEM.L D3/A2-A3,-(SP)
        BSR     rt_file__in
        BCS     .empty
        BSR     rt_file__item0
        BCC     .ok
.empty: MOVEQ   #0,D3
.ok:    BSR     rt_file__temp
        MOVEM.L (SP)+,D3/A2-A3
        RTS

; D0 = channel -> D0:D1 = the next item as a number (spaces around it allowed); error 35 if it is none
rt_file_num:
        MOVEM.L D3/A2-A3,-(SP)
        BSR     rt_file__in
        BCS     .zero
        BSR     rt_file__item0
        BCS     .zero
.tl:    TST.L   D3                      ; spaces at the end away
        BEQ     .bad
        CMP.B   #' ',-1(A3,D3.L)
        BNE     .lead
        SUBQ.L  #1,D3
        BRA     .tl
.lead:  MOVE.L  A3,A0                   ; and at the front
.ld:    CMP.B   #' ',(A0)
        BNE     .val
        ADDQ.L  #1,A0
        SUBQ.L  #1,D3
        BRA     .ld
.val:   MOVE.L  D3,D0
        MOVE.L  D3,-(SP)
        JSR     rt_val
        CMP.L   (SP)+,D2                ; all characters must belong to the number
        BEQ     .out
.bad:   MOVEQ   #35,D0                  ; data not numeric
        JSR     rt_error
.zero:  MOVEQ   #0,D0
        MOVEQ   #0,D1
.out:   MOVEM.L (SP)+,D3/A2-A3
        RTS

; D0 = n, D1 = channel -> A0 = the next n bytes (INPUT$(n,#c)); error 26 at the end
rt_file_chars:
        MOVEM.L D3-D4/A2-A3,-(SP)
        MOVE.L  D0,D4
        MOVE.L  D1,D0
        BSR     rt_file__in
        BCS     .empty
        BSR     rt_file__scratch
        MOVEQ   #0,D3
.l:     CMP.L   D4,D3
        BGE     .done
        BSR     rt_file__getb
        TST.L   D0
        BMI     .eof
        BSR     rt_file__keep
        BRA     .l
.eof:   MOVEQ   #26,D0
        JSR     rt_error
.empty: MOVEQ   #0,D3
.done:  BSR     rt_file__temp
        MOVEM.L (SP)+,D3-D4/A2-A3
        RTS

; D0 = channel -> D0 = EOF(#n): -1 at the end (and for an output file), else 0
rt_file_eof:
        MOVE.L  A2,-(SP)
        BSR     rt_file__open
        BCS     .no
        BTST    #0,9(A2)
        BEQ     .yes
        BSR     rt_file__peek
        TST.L   D0
        BMI     .yes
.no:    MOVEQ   #0,D0
        BRA     .out
.yes:   MOVEQ   #-1,D0
.out:   MOVE.L  (SP)+,A2
        RTS

; D0 = channel -> D0 = LOF(#n), the length of the file
rt_file_lof:
        MOVE.L  A2,-(SP)
        BSR     rt_file__open
        BCS     .zero
        BTST    #0,9(A2)
        BNE     .size
        BSR     rt_file__flush
.size:  MOVE.L  (A2),D0
        JSR     plat_fsize
        BRA     .out
.zero:  MOVEQ   #0,D0
.out:   MOVE.L  (SP)+,A2
        RTS

; A0 = name descriptor -> D0 = EXIST(name$): -1 or 0
rt_file_exist:
        MOVEQ   #0,D0
        MOVE.W  4(A0),D0
        MOVE.L  (A0),A0
        JMP     plat_fexists

; A0 = name descriptor: KILL
rt_file_kill:
        MOVEQ   #0,D0
        MOVE.W  4(A0),D0
        MOVE.L  (A0),A0
        JSR     plat_fdelete
        TST.L   D0
        BEQ     .out
        JSR     rt_error
.out:   RTS

;! section data
rt_file__cur:
        DC.L    0
rt_file__sb:
        DC.L    0
rt_file__quote:
        DC.B    '"'
        EVEN

;! section bss
rt_file__saveout:
        DS.L    1
rt_file__savecol:
        DS.W    1
rt_file__busy:
        DS.B    rt_file__BUFS
rt_file__ch:
        DS.B    16*100
rt_file__bufs:
        DS.B    rt_file__BUF*rt_file__BUFS
