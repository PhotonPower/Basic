;! module    rt_resume
;! provides  rt_resume
;! requires  rt_err_finish rt_err_frame rt_err_code plat_error prg_rlab
;! group     core
;! clobbers  D0-D3/A0-A1
;! doc       RESUME in the error routine (also in routines it calls).

; D0 = 0 RESUME, 1 RESUME NEXT, 2 RESUME label; D1 = label number (prg_rlab: per label its address
; and the address of its routine)
rt_resume:
        TST.L   rt_err_frame
        BNE     .ok
        MOVEQ   #-1,D0                  ; RESUME outside the error routine
        JMP     plat_error
.ok:    CLR.L   rt_err_code             ; RESUME clears ERR (probe12 T1)
        CMP.L   #2,D0
        BNE     .go
        LSL.L   #3,D1
        LEA     prg_rlab,A1
        MOVE.L  4(A1,D1.L),D2
        MOVE.L  0(A1,D1.L),A1
.go:    MOVE.L  D0,D1
        JMP     rt_err_finish
