;! module    plat_getc
;! provides  plat_getc plat_inkey plat_getc_gfx plat_getc_pend
;! requires  plat_dosbase plat_in plat_flush plat_putc_gfx
;! group     core
;! clobbers  D0-D1/A0-A1
;! doc       amiga: keyboard from Input() (dos Read), or from the current window when graphics are open (D31).

; Without graphics the characters come from Input() of the shell (D20). Once plat_gfx has opened its
; screen and window, the keys go to the program's own window, as in the original: plat_gfx sets
; plat_getc_gfx to a routine that gives the current window. Then
; - plat_getc reads a whole line from the window's VANILLAKEY messages, writes the typed characters
;   into the window only (Backspace takes one back) and hands the line out character by character with a
;   line feed at the end, as a console in line mode would;
; - plat_inkey takes one key without waiting and without echo (INKEY$), -1 if there is none.
; VANILLAKEY is switched on (ModifyIDCMP, keeping the other flags) when a key is wanted. Messages of
; other classes that arrive meanwhile are replied and dropped; ON MENU hands keys on (plat_getc_pend).

plat_getc__LINE EQU 254

; -> D0 = character (0..255), or -1 at the end of the input
plat_getc:
        JSR     plat_flush
        TST.L   plat_getc_gfx
        BNE     plat_getc__win
plat_getc__dos:
        MOVEM.L D2-D3/A6,-(SP)
        MOVE.L  plat_in,D1
        BEQ     .end
        MOVE.L  #plat_getc__c,D2
        MOVEQ   #1,D3
        MOVE.L  plat_dosbase,A6
        JSR     -42(A6)                 ; Read(Input(), &c, 1)
        CMP.L   #1,D0
        BNE     .end
        MOVEQ   #0,D0
        MOVE.B  plat_getc__c,D0
        BRA     .out
.end:   MOVEQ   #-1,D0
.out:   MOVEM.L (SP)+,D2-D3/A6
        RTS

; -> D0 = the next key without waiting (INKEY$), -1 if there is none; without graphics: plat_getc
plat_inkey:
        TST.L   plat_getc_gfx
        BEQ     plat_getc
        BSR     plat_getc__buffered     ; the rest of a line first
        BPL     .out
        MOVEQ   #0,D1                   ; no waiting
        BSR     plat_getc__key
.out:   RTS

; ---- the window ----------------------------------------------------------------------------------

; -> D0 = the next character of the line; a new line is read (with echo) when it is used up
plat_getc__win:
        BSR     plat_getc__buffered
        BPL     .out
        MOVEM.L D2-D3,-(SP)
        MOVEQ   #0,D2                   ; D2 = length
.key:   MOVEQ   #1,D1                   ; wait for a key
        BSR     plat_getc__key
        TST.L   D0
        BMI     .nowin
        CMP.B   #13,D0
        BEQ     .eol
        CMP.B   #10,D0
        BEQ     .eol
        CMP.B   #8,D0
        BEQ     .bs
        CMP.B   #127,D0
        BEQ     .bs
        CMP.B   #32,D0
        BCS     .key                    ; other control characters: ignored
        CMP.L   #plat_getc__LINE,D2
        BCC     .key
        LEA     plat_getc__buf,A0
        MOVE.B  D0,0(A0,D2.L)
        ADDQ.L  #1,D2
        BSR     plat_getc__echo
        BRA     .key
.bs:    TST.L   D2
        BEQ     .key
        SUBQ.L  #1,D2
        MOVEQ   #8,D0                   ; back, blank, back
        BSR     plat_getc__echo
        MOVEQ   #32,D0
        BSR     plat_getc__echo
        MOVEQ   #8,D0
        BSR     plat_getc__echo
        BRA     .key
.nowin: MOVEM.L (SP)+,D2-D3            ; no window (any more): Input() again
        BRA     plat_getc__dos
.eol:   LEA     plat_getc__buf,A0
        MOVE.B  #10,0(A0,D2.L)
        ADDQ.L  #1,D2
        MOVE.L  D2,plat_getc__len
        CLR.L   plat_getc__pos
        MOVEQ   #10,D0                  ; the cursor to the next line
        BSR     plat_getc__echo
        MOVEM.L (SP)+,D2-D3
        BSR     plat_getc__buffered
.out:   RTS

; D0.B = character: the echo, only into the window (as a console: not into Output(), which may be a
; file); all registers are kept
plat_getc__echo:
        MOVE.L  A0,-(SP)
        MOVE.L  plat_putc_gfx,A0
        CMP.L   #0,A0
        BEQ     .out
        JSR     (A0)
.out:   MOVE.L  (SP)+,A0
        RTS

; -> D0 = the next character of the line buffer, N set (D0 = -1) if it is used up
plat_getc__buffered:
        MOVE.L  plat_getc__pos,D1
        CMP.L   plat_getc__len,D1
        BCC     .none
        LEA     plat_getc__buf,A0
        MOVEQ   #0,D0
        MOVE.B  0(A0,D1.L),D0
        ADDQ.L  #1,D1
        MOVE.L  D1,plat_getc__pos
        TST.L   D0
        RTS
.none:  MOVEQ   #-1,D0
        RTS

; D1 = 1: wait for a key, 0: do not wait -> D0 = the character of the next VANILLAKEY message of the
; current window, -1 if there is none (or no window)
plat_getc__key:
        MOVEM.L D2-D4/A2/A6,-(SP)
        MOVE.L  D1,D3                   ; D3 = wait
        MOVE.W  plat_getc_pend,D0       ; a key that ON MENU passed on
        BMI     .ask
        MOVE.W  #-1,plat_getc_pend
        EXT.L   D0
        BRA     .out
.ask:   MOVE.L  plat_getc_gfx,A0
        JSR     (A0)                    ; D0 = window, D1 = intuition.library
        TST.L   D0
        BEQ     .none
        MOVE.L  D0,A2                   ; A2 = window
        MOVE.L  82(A2),D0               ; Window->IDCMPFlags
        BTST    #21,D0                  ; VANILLAKEY ($200000)
        BNE     .on
        BSET    #21,D0
        MOVE.L  A2,A0
        MOVE.L  D1,A6
        JSR     -150(A6)                ; ModifyIDCMP(window, flags)
.on:    MOVE.L  86(A2),D0               ; Window->UserPort
        BEQ     .none                   ; no port (no memory)
.l:     MOVE.L  4,A6
        MOVE.L  86(A2),A0
        JSR     -372(A6)                ; GetMsg
        TST.L   D0
        BNE     .got
        TST.L   D3
        BEQ     .none
        MOVE.L  86(A2),A0
        JSR     -384(A6)                ; WaitPort
        BRA     .l
.got:   MOVE.L  D0,A1
        MOVE.L  20(A1),D2               ; Class
        MOVEQ   #0,D4
        MOVE.W  24(A1),D4               ; Code
        JSR     -378(A6)                ; ReplyMsg
        CMP.L   #$200000,D2
        BNE     .l                      ; other messages: dropped
        MOVE.L  D4,D0
        AND.L   #$FF,D0
        BRA     .out
.none:  MOVEQ   #-1,D0
.out:   MOVEM.L (SP)+,D2-D4/A2/A6
        RTS

;! section data
plat_getc_gfx:                          ; routine of plat_gfx: D0 = current window, D1 = intuition base
        DC.L    0
plat_getc_pend:                         ; a key from ON MENU, -1: none
        DC.W    -1
        EVEN
plat_getc__pos:
        DC.L    0
plat_getc__len:
        DC.L    0

;! section bss
plat_getc__c:
        DS.B    2
plat_getc__buf:
        DS.B    plat_getc__LINE+2
