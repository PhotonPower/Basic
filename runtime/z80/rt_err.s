;! module    rt_err
;! provides  rt_error rt_err_hook rt_err_code rt_err_get rt_err_ctx rt_err_log rt_err_handler rt_err_at rt_err_frame
;! provides  rt_err_line
;! requires  plat_error
;! group     core
;! clobbers  AF BC DE HL
;! doc       Runtime errors (A = number): without ON ERROR GOSUB to the platform, else to the error routine.

; With an error routine (rt_err_handler, set by rt_on_error): ERR = A, the routine is called (it has
; to set ON ERROR GOSUB again); if it returns without RESUME, the error counts as unhandled (probe12
; T2). Statement records and the save log: docs/runtime.md, D30.
rt_error:
        LD      (rt_err__a),A
        LD      HL,(rt_err_hook)        ; first undo a running PRINT # (files)
        LD      A,H
        OR      L
        JR      Z,rt_err__nohook
        LD      A,(rt_err__a)
        CALL    rt_err__jp
rt_err__nohook:
        LD      HL,(rt_err_handler)
        LD      A,H
        OR      L
        JR      NZ,rt_err__trap
        LD      A,(rt_err__a)
        JP      plat_error
rt_err__trap:
        LD      A,(rt_err__a)
        LD      (rt_err_code),A
        LD      HL,(rt_err_frame)       ; an error inside the error routine nests
        PUSH    HL
        LD      HL,(rt_err_at)
        PUSH    HL
        LD      HL,0
        ADD     HL,SP
        LD      (rt_err_frame),HL
        LD      HL,(rt_err_ctx)         ; the failing statement
        LD      (rt_err_at),HL
        LD      HL,(rt_err_handler)
        LD      DE,0
        LD      (rt_err_handler),DE
        CALL    rt_err__jp
        LD      A,(rt_err_code)         ; ended without RESUME: unhandled, the program ends
        JP      plat_error
rt_err__jp:
        JP      (HL)

; HL = result: ERR (the error number as a signed byte, probe9 T6)
rt_err_get:
        LD      A,(rt_err_code)
        LD      (HL),A
        INC     HL
        RLCA
        SBC     A,A
        LD      (HL),A
        INC     HL
        LD      (HL),A
        INC     HL
        LD      (HL),A
        RET

; start of a statement (inline after the CALL: DW repeat, after): the record of the running
; routine (rt_err_ctx) gets both addresses, SP and IX
rt_err_line:
        POP     HL                      ; HL -> the two addresses
        LD      (rt_err__sp),SP         ; SP of the statement
        LD      DE,(rt_err_ctx)
        INC     DE
        INC     DE
        LD      BC,4
        LDIR                            ; repeat, after; HL = return address
        PUSH    HL
        LD      HL,(rt_err__sp)
        EX      DE,HL
        LD      (HL),E
        INC     HL
        LD      (HL),D
        INC     HL
        PUSH    IX
        POP     DE
        LD      (HL),E
        INC     HL
        LD      (HL),D
        RET

;! section data
rt_err_handler:
        DW      0
rt_err_hook:
        DW      0
rt_err_ctx:
        DW      0
rt_err_log:
        DW      0
rt_err_at:
        DW      0
rt_err_frame:
        DW      0
rt_err_code:
        DB      0
rt_err__a:
        DB      0
rt_err__sp:
        DW      0
