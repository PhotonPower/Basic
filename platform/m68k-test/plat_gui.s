;! module    plat_gui
;! provides  plat_gui
;! group     gui
;! clobbers  D0-D3/A0-A1
;! doc       m68k-test: requesters and menus through ports of the test harness (text dialogs of D26).

; Ports: $00FF0030 (long) next number; $00FF0034/$00FF0038 (long) address/length of the next
; string (the length adds it); $00FF003C (long) the code: executes the command. Result: long read of
; $00FF0040 the number; $00FF0044 the length of the string result, then $00FF0048 its characters.

; D0 = code, D1 = number of numbers, A0 = numbers, D2 = number of strings, A1 = descriptor
; addresses -> D0 = number; A0 = characters and D1 = length of the string result
plat_gui:
        BRA     .nn
.na:    MOVE.L  (A0)+,$00FF0030
.nn:    SUBQ.L  #1,D1
        BPL     .na
        BRA     .sn
.sa:    MOVE.L  (A1)+,A0
        MOVE.L  (A0),$00FF0034
        MOVEQ   #0,D1
        MOVE.W  4(A0),D1
        MOVE.L  D1,$00FF0038
.sn:    SUBQ.L  #1,D2
        BPL     .sa
        MOVE.L  D0,$00FF003C
        MOVE.L  $00FF0044,D1            ; the string result
        LEA     plat_gui__buf,A0
        MOVE.L  D1,D2
        BRA     .cn
.c:     MOVE.L  $00FF0048,D3
        MOVE.B  D3,(A0)+
.cn:    SUBQ.L  #1,D2
        BPL     .c
        LEA     plat_gui__buf,A0
        MOVE.L  $00FF0040,D0
        RTS

;! section bss
plat_gui__buf:
        DS.B    256
