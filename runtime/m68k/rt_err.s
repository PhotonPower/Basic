;! module    rt_err
;! provides  rt_error rt_err_hook rt_err_code rt_err_get rt_err_ctx rt_err_log rt_err_handler rt_err_fin rt_err_at rt_err_frame
;! requires  plat_error
;! group     core
;! clobbers  D0-D3/A0-A1
;! doc       Runtime errors: without ON ERROR GOSUB to the platform (plat_error), else to the error routine.

; D0 = error number. With an error routine (rt_err_handler, set by rt_on_error): ERR = D0, the
; routine is called (it has to set ON ERROR GOSUB again); if it returns without RESUME, the program
; goes on after the failing statement (rt_err_fin, see rt_onerr).
;
; Every routine of a program with ON ERROR has a record of its running statement (24 bytes in its
; frame): +0 record of the caller, +4 address to repeat the statement, +8 address after it,
; +12 SP and +16 A6 at its start, +20 address of the routine. rt_err_ctx points to the current record. rt_err_log chains the
; variables saved on the stack (LOCAL, parameters) so that RESUME can restore them.

rt_error:
        TST.L   rt_err_hook             ; first undo a running PRINT # (rt_file): output to the screen
        BEQ     .nohook
        MOVE.L  D0,-(SP)
        MOVE.L  rt_err_hook,A0
        JSR     (A0)
        MOVE.L  (SP)+,D0
.nohook:
        TST.L   rt_err_handler
        BNE     .trap
        JMP     plat_error
.trap:  MOVE.L  D0,rt_err_code
        MOVE.L  rt_err_frame,-(SP)      ; an error inside the error routine nests
        MOVE.L  rt_err_at,-(SP)
        MOVE.L  SP,rt_err_frame
        MOVE.L  rt_err_ctx,rt_err_at    ; the failing statement
        MOVE.L  rt_err_handler,A0
        CLR.L   rt_err_handler
        JSR     (A0)
        MOVEQ   #1,D1                   ; ended without RESUME: after the statement
        MOVE.L  rt_err_fin,A0
        JMP     (A0)

; -> D0 = ERR (the error number as a signed byte, probe9 T6)
rt_err_get:
        MOVE.B  rt_err_code+3,D0
        EXT.W   D0
        EXT.L   D0
        RTS

;! section data
rt_err_handler:
        DC.L    0
rt_err_hook:
        DC.L    0
rt_err_fin:
        DC.L    0
rt_err_ctx:
        DC.L    0
rt_err_log:
        DC.L    0
rt_err_at:
        DC.L    0
rt_err_frame:
        DC.L    0
rt_err_code:
        DC.L    0
