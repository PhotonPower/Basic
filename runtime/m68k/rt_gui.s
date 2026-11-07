;! module    rt_gui
;! provides  rt_gui_num rt_gui_str rt_gui_arr rt_gui_cmd rt_gui_cmds rt_on_menu rt_menu_gosub
;! requires  plat_gui rt_str_newtemp rt_arr_count rt_end
;! group     gui
;! clobbers  D0-D3/A0-A1
;! doc       Requesters and menus (D26): collects the arguments of a command, then the platform asks.

; The codes are the index in brt::kGuiCommands (platform/pc/rt_gui.hpp).
rt_gui__NUMS EQU 8
rt_gui__STRS EQU 256                    ; MENU m$(): one string per element

; D0 = next numeric argument
rt_gui_num:
        MOVE.L  rt_gui__nn,D1
        CMP.L   #rt_gui__NUMS,D1
        BCC     .full
        LEA     rt_gui__nums,A1
        LSL.L   #2,D1
        MOVE.L  D0,0(A1,D1.L)
        ADDQ.L  #1,rt_gui__nn
.full:  RTS

; A0 = descriptor of the next string argument (stays valid until the command)
rt_gui_str:
        MOVE.L  rt_gui__ns,D1
        CMP.L   #rt_gui__STRS,D1
        BCC     .full
        LEA     rt_gui__strs,A1
        LSL.L   #2,D1
        MOVE.L  A0,0(A1,D1.L)
        ADDQ.L  #1,rt_gui__ns
.full:  RTS

; A0 = slot of a string array: all its elements as string arguments (MENU m$())
rt_gui_arr:
        MOVE.L  A2,-(SP)
        MOVE.L  A0,A2
        BSR     rt_arr_count            ; D0 = elements (0: not dimensioned)
        MOVE.L  D0,D2
        BEQ     .out
        MOVE.L  (A2),A0                 ; block: 4 bytes per dimension, then the descriptors
        MOVEQ   #0,D1
        MOVE.W  4(A2),D1
        LSL.L   #2,D1
        ADD.L   D1,A0
.l:     MOVE.L  A0,-(SP)
        BSR     rt_gui_str
        MOVE.L  (SP)+,A0
        ADDQ.L  #8,A0
        SUBQ.L  #1,D2
        BNE     .l
.out:   MOVE.L  (SP)+,A2
        RTS

; D0 = code -> D0 = the numeric result
rt_gui_cmd:
        BSR     rt_gui__call
        RTS

; D0 = code -> A0 = the result as a new temporary string
rt_gui_cmds:
        BSR     rt_gui__call            ; A0 = characters, D1 = length
        MOVE.L  A0,-(SP)
        MOVE.L  D1,-(SP)
        MOVE.L  D1,D0
        JSR     rt_str_newtemp          ; A1 = descriptor, A0 = its characters
        MOVE.L  (SP)+,D1
        MOVE.L  (SP)+,D2
        EXG     D2,A1                   ; A1 = source, D2 = descriptor
        BRA     .n
.c:     MOVE.B  (A1)+,(A0)+
.n:     SUBQ.L  #1,D1
        BPL     .c
        MOVE.L  D2,A0
        RTS

; -> plat_gui with D0 = code, D1 = number of numbers, A0 = numbers, D2 = number of strings, A1 =
; descriptor addresses; the counters start again
rt_gui__call:
        MOVE.L  rt_gui__nn,D1
        MOVE.L  rt_gui__ns,D2
        CLR.L   rt_gui__nn
        CLR.L   rt_gui__ns
        LEA     rt_gui__nums,A0
        LEA     rt_gui__strs,A1
        JMP     plat_gui

; D0 = routine number + 1 of ON MENU GOSUB (0: none)
rt_menu_gosub:
        MOVE.L  D0,rt_gui__proc
        RTS

; ON MENU: -> D0 = routine number + 1 of ON MENU GOSUB if an entry was chosen, else 0; at the end
; of the input the program ends (D26)
rt_on_menu:
        MOVEQ   #7,D0
        BSR     rt_gui__call
        TST.L   D0
        BMI     .end
        BEQ     .out
        MOVE.L  rt_gui__proc,D0
.out:   RTS
.end:   JMP     rt_end

;! section bss
rt_gui__nn:
        DS.L    1
rt_gui__ns:
        DS.L    1
rt_gui__proc:
        DS.L    1
rt_gui__nums:
        DS.L    rt_gui__NUMS
rt_gui__strs:
        DS.L    rt_gui__STRS
