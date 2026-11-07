;! module    rt_onerr
;! provides  rt_on_error rt_err_finish
;! requires  rt_error rt_err_handler rt_err_fin rt_err_ctx rt_err_log rt_err_at rt_err_frame
;! group     core
;! clobbers  D0-D3/A0-A1
;! doc       ON ERROR GOSUB and the continuation after an error (RESUME, RESUME NEXT, RESUME label).

; A0 = address of the error routine (0: ON ERROR without GOSUB, errors end the program)
rt_on_error:
        MOVE.L  A0,rt_err_handler
        MOVE.L  #rt_err_finish,rt_err_fin
        RTS

; D1 = 0 repeat the statement, 1 after it, 2 at the label A1 of the routine D2 (the nearest frame
; of that routine in the chain of callers, as the interpreter looks for the label in the enclosing
; blocks). Restores the variables saved after the start of that statement, then continues there
; with its SP and A6. Does not return.
rt_err_finish:
        MOVE.L  rt_err_frame,A0         ; the invocation of rt_error: outer state back
        MOVE.L  rt_err_at,A2            ; A2 = record of the failing statement
        MOVE.L  (A0),rt_err_at
        MOVE.L  4(A0),rt_err_frame
        MOVE.L  4(A2),D6                ; D6 = where to continue (the undo uses A3)
        TST.L   D1
        BEQ     .unw
        MOVE.L  8(A2),D6
        CMP.L   #1,D1
        BEQ     .unw
        MOVE.L  A1,D6
.find:  CMP.L   20(A2),D2               ; the frame of the label's routine
        BEQ     .unw
        MOVE.L  (A2),D0
        BEQ     .unw                    ; not found (sema forbids that): stay
        MOVE.L  D0,A2
        BRA     .find
.unw:   MOVE.L  12(A2),D7               ; D7 = SP of the statement
.ul:    MOVE.L  rt_err_log,D0           ; entries below the statement's SP are undone
        BEQ     .done
        CMP.L   D7,D0
        BCC     .done
        MOVE.L  D0,A4
        BSR     rt_onerr__undo
        MOVE.L  4(A4),rt_err_log
        BRA     .ul
.done:  MOVE.L  A2,rt_err_ctx
        MOVE.L  16(A2),A6
        MOVE.L  D7,SP
        MOVE.L  D6,A0
        JMP     (A0)

; A4 = log entry (kept): +0 variable descriptor, +4 next entry, +8 the saved data (as Restore pops it).
; Variable descriptor: +0 storage, +4 pointer cell (indirect variables, else 0), +8 rt_str_free and
; +12 rt_str_relink for strings (else 0).
rt_onerr__undo:
        MOVE.L  (A4),A5                 ; A5 = descriptor
        LEA     8(A4),A3                ; A3 = saved data
        MOVE.L  4(A5),D0
        BEQ     .own
        MOVE.L  D0,A0                   ; indirect: flag, pointer, own value
        MOVE.L  4(A3),(A0)
        TST.L   (A3)
        BNE     .out                    ; bound to its own storage: keep the value
        ADDQ.L  #8,A3
.own:   MOVE.L  8(A5),D0                ; string: free the current block
        BEQ     .cp
        MOVE.L  D0,A1
        MOVE.L  (A5),A0
        JSR     (A1)
.cp:    MOVE.L  (A5),A0
        MOVE.L  (A3),(A0)
        MOVE.L  4(A3),4(A0)
        MOVE.L  12(A5),D0               ; string: the variable owns its block again
        BEQ     .out
        MOVE.L  D0,A1
        JSR     (A1)
.out:   RTS
