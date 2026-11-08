;! module    rt_resume
;! provides  rt_resume
;! requires  rt_err_finish rt_err_frame rt_err_code rt_err_mode rt_err_target rt_err_routine plat_error prg_rlab
;! group     core
;! clobbers  AF BC DE HL
;! doc       RESUME in the error routine (also in routines it calls).

; DE -> 0 RESUME, 1 RESUME NEXT, 2 RESUME label; BC -> label number (prg_rlab: per label its
; address and the address of its routine)
rt_resume:
        LD      HL,(rt_err_frame)
        LD      A,H
        OR      L
        JR      NZ,rt_resume__ok
        LD      A,255                   ; RESUME outside the error routine (prints -1)
        JP      plat_error
rt_resume__ok:
        XOR     A                       ; RESUME clears ERR (probe12 T1)
        LD      (rt_err_code),A
        LD      A,(DE)
        LD      (rt_err_mode),A
        CP      2
        JP      NZ,rt_err_finish
        LD      A,(BC)
        LD      L,A
        LD      H,0
        ADD     HL,HL
        ADD     HL,HL
        LD      DE,prg_rlab
        ADD     HL,DE
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        INC     HL
        LD      (rt_err_target),DE
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        LD      (rt_err_routine),DE
        JP      rt_err_finish
