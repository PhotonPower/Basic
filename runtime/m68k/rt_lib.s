;! module    rt_lib
;! provides  rt_lib_reg rt_lib_str rt_lib_call rt_window rt_screen
;! requires  plat_libcall plat_window plat_screen
;! group     amiga-os
;! clobbers  D0-D3/A0-A1
;! doc       AmigaOS library functions by name (D28): the argument registers, then the call through the base.

; Registers 0..7 d0..d7, 8..13 a0..a5 (amiga::argRegister). A string argument is copied into one of
; four buffers as a C string, the register gets its address.
rt_lib__BUFS EQU 4

; D0 = register, D1 = value
rt_lib_reg:
        LSL.L   #2,D0
        LEA     rt_lib__regs,A1
        MOVE.L  D1,0(A1,D0.L)
        RTS

; D0 = register, A0 = descriptor of the string
rt_lib_str:
        MOVE.L  D0,D2
        MOVE.L  rt_lib__next,D1         ; the next buffer
        ADDQ.L  #1,rt_lib__next
        AND.L   #rt_lib__BUFS-1,D1
        LSL.L   #8,D1
        LEA     rt_lib__buf,A1
        ADD.L   D1,A1
        MOVE.L  A1,D3                   ; D3 = its address
        MOVEQ   #0,D0
        MOVE.W  4(A0),D0
        MOVE.L  (A0),A0
        CMP.L   #255,D0
        BLS     .n
        MOVE.L  #255,D0
        BRA     .n
.c:     MOVE.B  (A0)+,(A1)+
.n:     SUBQ.L  #1,D0
        BPL     .c
        CLR.B   (A1)
        MOVE.L  D2,D0
        MOVE.L  D3,D1
        BRA     rt_lib_reg

; D0 = library ('e' exec, 'd' dos, 'i' intuition, 'g' graphics), D1 = offset -> D0 = result; the
; registers start again at 0
rt_lib_call:
        LEA     rt_lib__regs,A0
        JSR     plat_libcall
        LEA     rt_lib__regs,A0
        MOVEQ   #13,D1
.z:     CLR.L   (A0)+
        DBRA    D1,.z
        RTS

; D0 = number -> D0 = address of the Window or Screen structure (0 if not open)
rt_window:
        JMP     plat_window
rt_screen:
        JMP     plat_screen

;! section bss
rt_lib__next:
        DS.L    1
rt_lib__regs:
        DS.L    14
rt_lib__buf:
        DS.B    256*rt_lib__BUFS
