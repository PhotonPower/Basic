;! module    rt_gfx
;! provides  rt_gfx_arg rt_gfx_str rt_gfx_cmd rt_gfx_point
;! requires  plat_gfx plat_gfx_point
;! group     graphics
;! clobbers  D0-D3/A0-A1
;! doc       Graphics commands (D25): collects the arguments of a command, then the platform draws.

rt_gfx__MAX EQU 64                      ; DRAW x1,y1 TO ... has the most arguments

; D0 = next numeric argument (from left to right)
rt_gfx_arg:
        MOVE.L  rt_gfx__n,D1
        CMP.L   #rt_gfx__MAX,D1
        BCC     .full
        LEA     rt_gfx__args,A0
        LSL.L   #2,D1
        MOVE.L  D0,0(A0,D1.L)
        ADDQ.L  #1,rt_gfx__n
.full:  RTS

; A0 = descriptor of the string argument (TEXT, TITLEW, TITLES)
rt_gfx_str:
        MOVE.L  A0,rt_gfx__s
        RTS

; D0 = command code (index in brt::kGfxCommands), D1 = mode (arguments given; DRAW: 1 = DRAW TO)
; -> plat_gfx with D0 = code, D1 = mode, D2 = number of arguments, A0 = arguments, A1 = string
; descriptor or 0
rt_gfx_cmd:
        MOVE.L  rt_gfx__n,D2
        LEA     rt_gfx__args,A0
        MOVE.L  rt_gfx__s,A1
        CLR.L   rt_gfx__n
        CLR.L   rt_gfx__s
        JMP     plat_gfx

; D0 = x, D1 = y -> D0 = POINT(x,y)
rt_gfx_point:
        JMP     plat_gfx_point

;! section bss
rt_gfx__n:
        DS.L    1
rt_gfx__s:
        DS.L    1
rt_gfx__args:
        DS.L    rt_gfx__MAX
