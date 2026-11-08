;! module    rt_onerr
;! provides  rt_on_error rt_err_finish rt_err_mode rt_err_target rt_err_routine
;! requires  rt_err_handler rt_err_ctx rt_err_log rt_err_at rt_err_frame
;! group     core
;! clobbers  AF BC DE HL
;! doc       ON ERROR GOSUB and the continuation after an error (RESUME, RESUME NEXT, RESUME label).

; DE = address of the error routine (0: ON ERROR without GOSUB, errors end the program)
rt_on_error:
        LD      (rt_err_handler),DE
        RET

; rt_err_mode = 0 repeat the statement, 1 after it, 2 at rt_err_target in the routine rt_err_routine
; (the nearest frame of that routine in the chain of callers). Restores the variables saved after
; the start of that statement, then continues there with its SP and IX. Does not return.
; Record: +0 caller's record, +2 repeat, +4 after, +6 SP, +8 IX, +10 routine.
; Log entry: +0 variable descriptor, +2 next entry, +4 the saved data (as Restore pops it).
rt_err_finish:
        LD      HL,(rt_err_frame)       ; the invocation of rt_error: outer state back
        LD      DE,(rt_err_at)          ; DE = record of the failing statement
        LD      (rt_onerr__rec),DE
        LD      C,(HL)
        INC     HL
        LD      B,(HL)
        INC     HL
        LD      (rt_err_at),BC
        LD      C,(HL)
        INC     HL
        LD      B,(HL)
        LD      (rt_err_frame),BC
        LD      HL,(rt_onerr__rec)      ; where to continue
        LD      A,(rt_err_mode)
        LD      DE,2
        OR      A
        JR      Z,rt_onerr__t
        LD      DE,4
        CP      1
        JR      Z,rt_onerr__t
        LD      HL,(rt_err_target)      ; a label: the frame of its routine
        LD      (rt_onerr__go),HL
rt_onerr__find:
        LD      HL,(rt_onerr__rec)
        LD      DE,10
        ADD     HL,DE
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        LD      HL,(rt_err_routine)
        OR      A
        SBC     HL,DE
        JR      Z,rt_onerr__unw
        LD      HL,(rt_onerr__rec)
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        LD      A,D
        OR      E
        JR      Z,rt_onerr__unw         ; not found (sema forbids that): stay
        LD      (rt_onerr__rec),DE
        JR      rt_onerr__find
rt_onerr__t:
        ADD     HL,DE
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        LD      (rt_onerr__go),DE
rt_onerr__unw:
        LD      HL,(rt_onerr__rec)      ; SP of the statement
        LD      DE,6
        ADD     HL,DE
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        LD      (rt_onerr__sp),DE
rt_onerr__ul:                           ; log entries below that SP are undone
        LD      HL,(rt_err_log)
        LD      A,H
        OR      L
        JR      Z,rt_onerr__done
        LD      DE,(rt_onerr__sp)
        OR      A
        SBC     HL,DE
        JR      NC,rt_onerr__done
        LD      HL,(rt_err_log)
        LD      (rt_onerr__e),HL
        CALL    rt_onerr__undo
        LD      HL,(rt_onerr__e)
        INC     HL
        INC     HL
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        LD      (rt_err_log),DE
        JR      rt_onerr__ul
rt_onerr__done:
        LD      HL,(rt_onerr__rec)
        LD      (rt_err_ctx),HL
        LD      DE,8
        ADD     HL,DE
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        PUSH    DE
        POP     IX
        LD      HL,(rt_onerr__sp)
        LD      SP,HL
        LD      HL,(rt_onerr__go)
        JP      (HL)

; rt_onerr__e = log entry: the variable gets its saved value back. Descriptor: +0 storage, +2 pointer
; cell (VAR-bound variables, else 0), +4 rt_str_free and +6 rt_str_relink for strings (else 0).
rt_onerr__undo:
        LD      HL,(rt_onerr__e)
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        LD      (rt_onerr__d),DE        ; descriptor
        LD      DE,3
        ADD     HL,DE
        LD      (rt_onerr__data),HL     ; the saved data
        LD      HL,(rt_onerr__d)
        INC     HL
        INC     HL
        LD      E,(HL)
        INC     HL
        LD      D,(HL)                  ; pointer cell
        LD      A,D
        OR      E
        JR      Z,rt_onerr__own
        LD      HL,(rt_onerr__data)     ; flag, pointer, own value
        INC     HL
        INC     HL
        LD      C,(HL)
        INC     HL
        LD      B,(HL)
        EX      DE,HL
        LD      (HL),C
        INC     HL
        LD      (HL),B
        LD      HL,(rt_onerr__data)
        LD      A,(HL)
        INC     HL
        OR      (HL)
        RET     NZ                      ; bound to its own storage: keep the value
        INC     HL
        INC     HL
        INC     HL
        LD      (rt_onerr__data),HL
rt_onerr__own:
        LD      HL,(rt_onerr__d)        ; string: free the current block
        LD      DE,4
        ADD     HL,DE
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        LD      A,D
        OR      E
        JR      Z,rt_onerr__cp
        CALL    rt_onerr__var
        CALL    rt_onerr__jde
rt_onerr__cp:
        CALL    rt_onerr__var           ; copy the saved 8 bytes
        EX      DE,HL
        LD      HL,(rt_onerr__data)
        LD      BC,8
        LDIR
        LD      HL,(rt_onerr__d)        ; string: the variable owns its block again
        LD      DE,6
        ADD     HL,DE
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        LD      A,D
        OR      E
        RET     Z
        CALL    rt_onerr__var
rt_onerr__jde:
        PUSH    DE
        RET                             ; jump to DE with HL = storage
rt_onerr__var:                          ; HL = storage of the variable (DE kept)
        PUSH    DE
        LD      HL,(rt_onerr__d)
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        EX      DE,HL
        POP     DE
        RET

;! section bss
rt_err_mode:
        DS      1
rt_err_target:
        DS      2
rt_err_routine:
        DS      2
rt_onerr__rec:
        DS      2
rt_onerr__go:
        DS      2
rt_onerr__sp:
        DS      2
rt_onerr__e:
        DS      2
rt_onerr__d:
        DS      2
rt_onerr__data:
        DS      2
