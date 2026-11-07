;! module    rt_kbd
;! provides  rt_kbd_begin rt_kbd_field rt_kbd_num rt_kbd_line rt_kbd_end rt_kbd_inkey rt_kbd_chars
;! requires  plat_getc plat_alloc rt_print_str rt_print_col rt_str_newtemp rt_val rt_error
;! group     input
;! clobbers  D0-D3/A0-A1
;! doc       Keyboard: INPUT, LINE INPUT, INKEY$, INPUT$(n) (semantics of brt::Keyboard, platform/pc).

; INPUT/LINE INPUT read lines through plat_getc. Prompt: "text? " (mode 1, "text";), "text" (mode 2,
; "text",), "? " (mode 0, no text). INPUT values are separated by commas; missing values come from
; further lines (prompt "? "); a value that is not a number repeats the prompt and the input.
; INKEY$ and INPUT$(n) read characters, a line feed counts as Return (13). At the end: error 26.

rt_kbd__MAX EQU 32767

; A0 = prompt text (descriptor), D0 = mode: print the prompt, read a line
rt_kbd_begin:
        MOVE.L  D2,-(SP)
        LEA     rt_kbd__prompt,A1       ; the prompt into its own buffer (for a repetition)
        MOVEQ   #0,D2
        TST.L   D0
        BEQ     .q
        MOVEQ   #0,D1
        MOVE.W  4(A0),D1
        CMP.L   #250,D1
        BLS     .ln
        MOVE.L  #250,D1
.ln:    MOVE.L  (A0),A0
        BRA     .cn
.c:     MOVE.B  (A0)+,(A1)+
        ADDQ.L  #1,D2
.cn:    SUBQ.L  #1,D1
        BPL     .c
        CMP.L   #2,D0
        BEQ     .done
.q:     MOVE.B  #'?',(A1)+
        MOVE.B  #' ',(A1)+
        ADDQ.L  #2,D2
.done:  MOVE.L  D2,rt_kbd__plen
        MOVE.L  (SP)+,D2
        LEA     rt_kbd__prompt,A0
        MOVE.L  rt_kbd__plen,D0
        BRA     rt_kbd__read

; A0 = prompt, D0 = length: print it, read a line into the buffer (error 26 at the end)
rt_kbd__read:
        MOVEM.L D3/A3,-(SP)
        JSR     rt_print_str
        BSR     rt_kbd__ensure
        BEQ     .out
        MOVEQ   #0,D3
.l:     JSR     plat_getc
        TST.L   D0
        BMI     .eof
        CMP.B   #10,D0
        BEQ     .eol
        CMP.L   #rt_kbd__MAX,D3
        BCC     .l
        MOVE.B  D0,0(A3,D3.L)
        ADDQ.L  #1,D3
        BRA     .l
.eof:   TST.L   D3                      ; the end before any character: error 26
        BNE     .eol
        MOVEQ   #26,D0
        JSR     rt_error
.eol:   TST.L   D3                      ; a CR before the LF away
        BEQ     .set
        CMP.B   #13,-1(A3,D3.L)
        BNE     .set
        SUBQ.L  #1,D3
.set:   MOVE.L  D3,rt_kbd__len
        CLR.L   rt_kbd__pos
        MOVE.W  #1,rt_kbd__whole
.out:   MOVEM.L (SP)+,D3/A3
        RTS

; -> A3 = line buffer (32 KiB from the platform on first use); Z set (after error 8) if there is none
rt_kbd__ensure:
        MOVE.L  rt_kbd__buf,A3
        MOVE.L  A3,D0
        BNE     .out
        MOVE.L  #rt_kbd__MAX+1,D0
        JSR     plat_alloc
        MOVE.L  A0,rt_kbd__buf
        MOVE.L  A0,A3
        MOVE.L  A0,D0
        BNE     .out
        MOVEQ   #8,D0
        JSR     rt_error
        MOVEQ   #0,D0
.out:   RTS

; -> A0 = start, D0 = length of the next field (a new line with "? " if none is left)
rt_kbd__next:
        TST.L   rt_kbd__pos
        BPL     .go
        LEA     rt_kbd__qm,A0
        MOVEQ   #2,D0
        BSR     rt_kbd__read
.go:    CLR.W   rt_kbd__whole
        MOVE.L  rt_kbd__buf,A0
        MOVE.L  rt_kbd__pos,D1
        ADD.L   D1,A0                   ; A0 = start of the field
        MOVE.L  rt_kbd__len,D0
        SUB.L   D1,D0                   ; characters left in the line
        MOVEQ   #0,D1
.f:     CMP.L   D0,D1
        BGE     .last
        CMP.B   #',',0(A0,D1.L)
        BEQ     .comma
        ADDQ.L  #1,D1
        BRA     .f
.last:  MOVE.L  #-1,rt_kbd__pos         ; no field left after this one
        RTS
.comma: MOVE.L  D1,D0
        ADDQ.L  #1,D1
        ADD.L   D1,rt_kbd__pos
        RTS

; A0 = characters, D0 = count -> A0 = temporary string with them
rt_kbd__temp:
        MOVEM.L D0/A0,-(SP)
        JSR     rt_str_newtemp          ; A1 = descriptor, A0 = characters
        MOVE.L  A1,D1
        MOVEM.L (SP)+,D0/A1
        BRA     .n
.c:     MOVE.B  (A1)+,(A0)+
.n:     SUBQ.L  #1,D0
        BPL     .c
        MOVE.L  D1,A0
        RTS

; -> A0 = the next value as a string (INPUT into a string variable)
rt_kbd_field:
        BSR     rt_kbd__next
        BRA     rt_kbd__temp

; -> D0:D1 = the next value as a number; not a number: the prompt and the input again
rt_kbd_num:
        MOVE.L  D3,-(SP)
.again: BSR     rt_kbd__next
        MOVE.L  D0,D3
.tl:    TST.L   D3                      ; spaces at the end and at the front away
        BEQ     .bad
        CMP.B   #' ',-1(A0,D3.L)
        BNE     .ld
        SUBQ.L  #1,D3
        BRA     .tl
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
.bad:   LEA     rt_kbd__prompt,A0
        MOVE.L  rt_kbd__plen,D0
        BSR     rt_kbd__read
        BRA     .again
.out:   MOVE.L  (SP)+,D3
        RTS

; -> A0 = the whole line (LINE INPUT; for further variables a new line with "? ")
rt_kbd_line:
        TST.W   rt_kbd__whole
        BNE     .take
        LEA     rt_kbd__qm,A0
        MOVEQ   #2,D0
        BSR     rt_kbd__read
.take:  CLR.W   rt_kbd__whole
        MOVE.L  #-1,rt_kbd__pos
        MOVE.L  rt_kbd__buf,A0
        MOVE.L  rt_kbd__len,D0
        BRA     rt_kbd__temp

; end of INPUT: the Return moved the cursor to the start of the next line
rt_kbd_end:
        CLR.W   rt_print_col
        RTS

; -> A0 = INKEY$: one character, or "" at the end of the input
rt_kbd_inkey:
        JSR     plat_getc
        TST.L   D0
        BMI     .none
        BSR     rt_kbd__cr
        MOVE.B  D0,rt_kbd__ch
        LEA     rt_kbd__ch,A0
        MOVEQ   #1,D0
        BRA     rt_kbd__temp
.none:  MOVEQ   #0,D0
        BRA     rt_kbd__temp

; D0 = n -> A0 = INPUT$(n): the next n characters (error 26 at the end)
rt_kbd_chars:
        MOVEM.L D3-D4/A3,-(SP)
        MOVE.L  D0,D4
        BSR     rt_kbd__ensure          ; the characters go through the line buffer
        BEQ     .done0
        CMP.L   #rt_kbd__MAX,D4
        BLE     .n
        MOVE.L  #rt_kbd__MAX,D4
.n:     MOVEQ   #0,D3
.l:     CMP.L   D4,D3
        BGE     .done
        JSR     plat_getc
        TST.L   D0
        BMI     .eof
        BSR     rt_kbd__cr
        MOVE.B  D0,0(A3,D3.L)
        ADDQ.L  #1,D3
        BRA     .l
.eof:   MOVEQ   #26,D0
        JSR     rt_error
.done:  MOVE.L  A3,A0
        MOVE.L  D3,D0
        BSR     rt_kbd__temp
        BRA     .out
.done0: MOVEQ   #0,D0
        BSR     rt_kbd__temp
.out:
        MOVEM.L (SP)+,D3-D4/A3
        RTS

; D0 = character: a line feed becomes Return (13)
rt_kbd__cr:
        CMP.B   #10,D0
        BNE     .out
        MOVEQ   #13,D0
.out:   RTS

;! section data
rt_kbd__buf:
        DC.L    0
rt_kbd__pos:
        DC.L    -1
rt_kbd__qm:
        DC.B    '? '
        EVEN

;! section bss
rt_kbd__len:
        DS.L    1
rt_kbd__plen:
        DS.L    1
rt_kbd__whole:
        DS.W    1
rt_kbd__ch:
        DS.W    1
rt_kbd__prompt:
        DS.B    260
