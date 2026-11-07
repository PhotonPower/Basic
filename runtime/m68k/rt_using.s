;! module    rt_using
;! provides  rt_using_begin rt_using_num rt_using_str rt_using_end rt_using_numb rt_using_strb rt_using_endb rt_using_fmt rt_set_mode rt_mode
;! requires  rt_flt_dec rt_fmt_flt rt_print_str
;! group     print
;! clobbers  D0-D3/A0-A1
;! doc       PRINT USING (runtime/ref/rt_using.c): number fields (# * $ , . ^^^^ + -), string fields (& ! \..\), _ escape.

; One PRINT USING: rt_using_begin (A0 = descriptor of the format), for every value rt_using_num
; (D0:D1) or rt_using_str (A0 = descriptor), then rt_using_end; each prints its text. The variants
; rt_using_numb, rt_using_strb, rt_using_endb return the text instead (A0 = characters, D0 = count). The state is
; kept on a small stack, so that a PRINT USING inside a function of the value list works. The
; format is read through its descriptor on every call (the string heap may move it).
; rt_using_fmt (STR$(x,y,z)): D0:D1 = x, D2 = y, D3 = z -> A0 = characters, D0 = length.
;
; Registers inside: A3 = format characters, D5 = format length, A4 = output buffer, D7 = output
; count, D6 = output capacity, A5 = field record.

rt_using__MAXDEC EQU 300
rt_using__DEPTH  EQU 8

; field record (longs)
rt_using__KIND   EQU 0
rt_using__WIDTH  EQU 4
rt_using__LEAD   EQU 8
rt_using__TRAIL  EQU 12
rt_using__DOLLAR EQU 16
rt_using__STAR   EQU 20
rt_using__THOU   EQU 24
rt_using__POINT  EQU 28
rt_using__WINT   EQU 32
rt_using__NDIG   EQU 36
rt_using__DEC    EQU 40
rt_using__CARETS EQU 44
rt_using__END    EQU 48

; D0 = MODE (0-3)
rt_set_mode:
        AND.L   #3,D0
        MOVE.L  D0,rt_mode
        RTS

; ---- entry points ------------------------------------------------------------------------------

; A0 = descriptor of the format
rt_using_begin:
        MOVEM.L D4-D7/A2-A5,-(SP)
        MOVE.L  rt_using__sp,D0         ; push the state of an outer PRINT USING
        CMP.L   #rt_using__DEPTH,D0
        BCC     .full
        LSL.L   #4,D0
        LEA     rt_using__stack,A1
        ADD.L   D0,A1
        MOVE.L  rt_using__fdesc,(A1)+
        MOVE.L  rt_using__pos,(A1)+
        MOVE.L  rt_using__md,(A1)+
        MOVE.L  rt_using__has,(A1)+
.full:  ADDQ.L  #1,rt_using__sp
        MOVE.L  rt_mode,D0
        BSR     rt_using__init
        MOVEM.L (SP)+,D4-D7/A2-A5
        RTS

; A0 = format descriptor, D0 = mode: new state (pos 0, has_field)
rt_using__init:
        MOVE.L  A0,rt_using__fdesc
        MOVE.L  D0,rt_using__md
        CLR.L   rt_using__pos
        CLR.L   rt_using__has
        BSR     rt_using__load
        MOVEQ   #0,D2
.l:     CMP.L   D5,D2
        BGE     .out
        CMP.B   #'_',0(A3,D2.L)
        BNE     .t
        ADDQ.L  #2,D2
        BRA     .l
.t:     BSR     rt_using__start
        TST.L   D0
        BNE     .yes
        ADDQ.L  #1,D2
        BRA     .l
.yes:   MOVE.L  #1,rt_using__has
.out:   RTS

; A3/D5 = format characters and length from the descriptor; decimal point and separator of the mode
rt_using__load:
        MOVE.L  rt_using__fdesc,A0
        MOVEQ   #0,D5
        MOVE.W  4(A0),D5
        MOVE.L  (A0),A3
        MOVE.B  #'.',rt_using__dpt
        MOVE.B  #',',rt_using__thc
        CMP.L   #2,rt_using__md
        BLT     .m
        MOVE.B  #',',rt_using__dpt
        MOVE.B  #'.',rt_using__thc
.m:     RTS

rt_using_num:
        BSR     rt_using_numb
        BRA     rt_using__print
rt_using_str:
        BSR     rt_using_strb
        BRA     rt_using__print
rt_using_end:
        BSR     rt_using_endb
rt_using__print:
        TST.L   D0
        BEQ     .out
        JMP     rt_print_str
.out:   RTS

; D0:D1 = number
rt_using_numb:
        MOVEM.L D4-D7/A2-A5,-(SP)
        MOVEM.L D0-D1,rt_using__x
        MOVE.L  #4096,D6
        BSR     rt_using__prep
        BSR     rt_using__numc
        BRA     rt_using__ret

; the text in the buffer as the result: A0 = characters, D0 = count
rt_using__ret:
        LEA     rt_using__buf,A0
        MOVE.L  D7,D0
        MOVEM.L (SP)+,D4-D7/A2-A5
        RTS

; A0 = string descriptor
rt_using_strb:
        MOVEM.L D4-D7/A2-A5,-(SP)
        MOVE.L  A0,rt_using__sdesc
        MOVE.L  #4096,D6
        BSR     rt_using__prep
        MOVEQ   #1,D0                   ; wrap at the end of the format
        MOVEQ   #0,D1
        BSR     rt_using__literal
        TST.L   D0
        BEQ     .nofld
        MOVE.L  rt_using__pos,D2
        BSR     rt_using__parse
        CMP.L   #1,rt_using__KIND(A5)
        BEQ     .mis
        MOVE.L  rt_using__END(A5),rt_using__pos
        BSR     rt_using__string
        BRA     .done
.mis:   BSR     rt_using__mismatch
.nofld: MOVEQ   #'?',D0                 ; no suitable field: "?" and the string
        BSR     rt_using__put
        MOVE.L  rt_using__sdesc,A0
        MOVEQ   #0,D2
        MOVE.W  4(A0),D2
        MOVE.L  (A0),A0
        BRA     .cn
.c:     MOVE.B  (A0)+,D0
        BSR     rt_using__put
.cn:    SUBQ.L  #1,D2
        BPL     .c
.done:  BRA     rt_using__ret

; the rest of the format as text (probe7 T11), then the outer state back
rt_using_endb:
        MOVEM.L D4-D7/A2-A5,-(SP)
        MOVE.L  #4096,D6
        BSR     rt_using__prep
        MOVEQ   #0,D0
        MOVEQ   #1,D1
        BSR     rt_using__literal
        SUBQ.L  #1,rt_using__sp
        MOVE.L  rt_using__sp,D0
        CMP.L   #rt_using__DEPTH,D0
        BCC     .out
        LSL.L   #4,D0
        LEA     rt_using__stack,A1
        ADD.L   D0,A1
        MOVE.L  (A1)+,rt_using__fdesc
        MOVE.L  (A1)+,rt_using__pos
        MOVE.L  (A1)+,rt_using__md
        MOVE.L  (A1)+,rt_using__has
.out:   BRA     rt_using__ret

; D0:D1 = x, D2 = y, D3 = z -> A0 = characters, D0 = count (STR$(x,y,z), at most 512 characters)
rt_using_fmt:
        MOVEM.L D4-D7/A2-A5,-(SP)
        MOVEM.L D0-D1,rt_using__x
        MOVE.L  rt_using__fdesc,-(SP)   ; keep a running PRINT USING intact
        MOVE.L  rt_using__pos,-(SP)
        MOVE.L  rt_using__md,-(SP)
        MOVE.L  rt_using__has,-(SP)
        MOVEQ   #0,D7
        TST.L   D2                      ; y <= 0: ""
        BLE     .ret
        CMP.L   #255,D2
        BLE     .y
        MOVE.L  #255,D2
.y:     TST.L   D3
        BPL     .z
        MOVEQ   #0,D3
.z:     MOVE.B  #'.',D4                 ; the point of the mode
        CMP.L   #2,rt_mode
        BLT     .p
        MOVE.B  #',',D4
.p:     LEA     rt_using__sfmt,A0       ; build "###.##"
        MOVEQ   #0,D0                   ; D0 = n
        TST.L   D3
        BEQ     .noz
        MOVE.L  D2,D1                   ; y - z - 1 '#'
        SUB.L   D3,D1
        SUBQ.L  #1,D1
        BRA     .h1n
.h1:    MOVE.B  #'#',0(A0,D0.L)
        ADDQ.L  #1,D0
.h1n:   SUBQ.L  #1,D1
        BPL     .h1
        MOVE.B  D4,0(A0,D0.L)
        ADDQ.L  #1,D0
        MOVE.L  D3,D1
        BRA     .h2n
.h2:    CMP.L   #258,D0
        BGE     .fd
        MOVE.B  #'#',0(A0,D0.L)
        ADDQ.L  #1,D0
.h2n:   SUBQ.L  #1,D1
        BPL     .h2
        BRA     .fd
.noz:   MOVE.L  D2,D1
        BRA     .h3n
.h3:    MOVE.B  #'#',0(A0,D0.L)
        ADDQ.L  #1,D0
.h3n:   SUBQ.L  #1,D1
        BPL     .h3
.fd:    LEA     rt_using__sdsc,A1       ; static descriptor of the format
        MOVE.L  A0,(A1)
        MOVE.W  D0,4(A1)
        MOVE.L  A1,A0
        MOVE.L  rt_mode,D0
        BSR     rt_using__init
        MOVE.L  #512,D6
        BSR     rt_using__prep
        BSR     rt_using__numc
.ret:   MOVE.L  (SP)+,rt_using__has
        MOVE.L  (SP)+,rt_using__md
        MOVE.L  (SP)+,rt_using__pos
        MOVE.L  (SP)+,rt_using__fdesc
        LEA     rt_using__buf,A0
        MOVE.L  D7,D0
        MOVEM.L (SP)+,D4-D7/A2-A5
        RTS

; ---- common --------------------------------------------------------------------------------------

; D6 = output capacity
rt_using__prep:
        MOVE.L  D6,rt_using__cap
        BSR     rt_using__load
        LEA     rt_using__buf,A4
        MOVEQ   #0,D7
        LEA     rt_using__fl,A5
        RTS

; D0.B = character into the output (if there is room)
rt_using__put:
        CMP.L   D6,D7
        BGE     .full
        MOVE.B  D0,0(A4,D7.L)
        ADDQ.L  #1,D7
.full:  RTS

; number field at the current position (or "?" and the number): x in rt_using__x
rt_using__numc:
        MOVEQ   #1,D0
        MOVEQ   #0,D1
        BSR     rt_using__literal
        TST.L   D0
        BEQ     .unf
        MOVE.L  rt_using__pos,D2
        BSR     rt_using__parse
        CMP.L   #1,rt_using__KIND(A5)
        BNE     .mis
        MOVE.L  rt_using__END(A5),rt_using__pos
        BRA     rt_using__number
.mis:   BSR     rt_using__mismatch
.unf:   MOVEQ   #'?',D0                 ; "?" and the number as by PRINT
        BSR     rt_using__put
        MOVEM.L rt_using__x,D0-D1
        LEA     rt_using__tmp,A0
        MOVEQ   #13,D2
        JSR     rt_fmt_flt
        LEA     rt_using__tmp,A0
        MOVE.L  D0,D2
        BRA     .cn
.c:     MOVE.B  (A0)+,D0
        BSR     rt_using__put
.cn:    SUBQ.L  #1,D2
        BPL     .c
        RTS

; D0 = index, D1.B = character -> Z set if the format has that character there
rt_using__at:
        CMP.L   D5,D0
        BGE     .no
        CMP.B   0(A3,D0.L),D1
        RTS
.no:    ANDI    #$FB,CCR
        RTS

; D2 = index -> D0 = kind of a field starting there (0 none, 1 number, 2 &, 3 !, 4 \..\)
; (keeps D2, uses D1, D3)
rt_using__start:
        MOVE.B  0(A3,D2.L),D3
        CMP.B   #'#',D3
        BEQ     .one
        CMP.B   rt_using__dpt,D3
        BNE     .sg
        MOVE.L  D2,D0                   ; point: followed by '#'
        ADDQ.L  #1,D0
        MOVEQ   #'#',D1
        BSR     rt_using__at
        BEQ     .one
        BRA     .zero
.sg:    CMP.B   #'+',D3
        BEQ     .sign
        CMP.B   #'-',D3
        BNE     .st
.sign:  MOVE.L  D2,D0                   ; sign: then # * $ or point and #
        ADDQ.L  #1,D0
        BSR     rt_using__nstart
        BEQ     .one
        MOVE.L  D2,D0
        ADDQ.L  #1,D0
        MOVEQ   #'*',D1
        BSR     rt_using__at
        BEQ     .one
        BRA     .zero
.st:    CMP.B   #'*',D3
        BNE     .dl
        MOVE.L  D2,D0                   ; stars: then # $ or point and #
.sl:    MOVEQ   #'*',D1
        BSR     rt_using__at
        BNE     .se
        ADDQ.L  #1,D0
        BRA     .sl
.se:    BSR     rt_using__nstart
        BEQ     .one
        BRA     .zero
.dl:    CMP.B   #'$',D3
        BNE     .amp
        MOVE.L  D2,D0                   ; dollar: then # or *
        ADDQ.L  #1,D0
        MOVEQ   #'#',D1
        BSR     rt_using__at
        BEQ     .one
        MOVE.L  D2,D0
        ADDQ.L  #1,D0
        MOVEQ   #'*',D1
        BSR     rt_using__at
        BEQ     .one
        BRA     .zero
.amp:   CMP.B   #'&',D3
        BNE     .exc
        MOVEQ   #2,D0
        RTS
.exc:   CMP.B   #'!',D3
        BNE     .bs
        MOVEQ   #3,D0
        RTS
.bs:    CMP.B   #'\',D3
        BNE     .zero
        MOVE.L  D2,D0                   ; backslash, spaces, backslash
.bl:    ADDQ.L  #1,D0
        MOVEQ   #' ',D1
        BSR     rt_using__at
        BEQ     .bl
        MOVEQ   #'\',D1
        BSR     rt_using__at
        BNE     .zero
        MOVEQ   #4,D0
        RTS
.one:   MOVEQ   #1,D0
        RTS
.zero:  MOVEQ   #0,D0
        RTS

; D0 = index -> Z set if '#', '$' or point and '#' follow there (D0 kept)
rt_using__nstart:
        MOVE.L  D0,-(SP)
        MOVEQ   #'#',D1
        BSR     rt_using__at
        BEQ     .out
        MOVE.L  (SP),D0
        MOVEQ   #'$',D1
        BSR     rt_using__at
        BEQ     .out
        MOVE.L  (SP),D0
        MOVE.B  rt_using__dpt,D1
        BSR     rt_using__at
        BNE     .out
        MOVE.L  (SP),D0
        ADDQ.L  #1,D0
        MOVEQ   #'#',D1
        BSR     rt_using__at
.out:   MOVEM.L (SP)+,D0                ; MOVEM keeps the flags
        RTS

; D2 = index of a field -> field record (A5)
rt_using__parse:
        MOVE.L  A5,A0                   ; clear the record
        MOVEQ   #12,D0
.cl:    CLR.L   (A0)+
        DBRA    D0,.cl
        BSR     rt_using__start
        MOVE.L  D0,rt_using__KIND(A5)
        CMP.L   #2,D0
        BEQ     .one
        CMP.L   #3,D0
        BNE     .k4
.one:   MOVE.L  D2,D0
        ADDQ.L  #1,D0
        MOVE.L  D0,rt_using__END(A5)
        RTS
.k4:    CMP.L   #4,D0
        BNE     .num
        MOVE.L  D2,D0                   ; up to the closing backslash
.bk:    ADDQ.L  #1,D0
        CMP.B   #'\',0(A3,D0.L)
        BNE     .bk
        MOVE.L  D0,D1
        SUB.L   D2,D1
        ADDQ.L  #1,D1
        MOVE.L  D1,rt_using__WIDTH(A5)
        ADDQ.L  #1,D0
        MOVE.L  D0,rt_using__END(A5)
        RTS
.num:   MOVE.L  D2,D4                   ; D4 = j
        MOVE.B  0(A3,D4.L),D3
        CMP.B   #'+',D3
        BEQ     .ls
        CMP.B   #'-',D3
        BNE     .ip
.ls:    EXT.W   D3
        EXT.L   D3
        MOVE.L  D3,rt_using__LEAD(A5)
        ADDQ.L  #1,D4
.ip:    CMP.L   D5,D4                   ; positions before the point
        BGE     .pt
        MOVE.B  0(A3,D4.L),D3
        CMP.B   #'#',D3
        BEQ     .dig
        CMP.B   #'*',D3
        BNE     .dol
        MOVE.L  #1,rt_using__STAR(A5)
.dig:   ADDQ.L  #1,rt_using__NDIG(A5)
        BRA     .nxt
.dol:   CMP.B   #'$',D3
        BNE     .th
        TST.L   rt_using__DOLLAR(A5)
        BNE     .pt
        MOVE.L  #1,rt_using__DOLLAR(A5)
        BRA     .nxt
.th:    CMP.B   rt_using__thc,D3
        BNE     .pt
        TST.L   rt_using__NDIG(A5)
        BEQ     .pt
        MOVE.L  D4,D0
        ADDQ.L  #1,D0
        MOVEQ   #'#',D1
        BSR     rt_using__at
        BNE     .pt
        MOVE.L  #1,rt_using__THOU(A5)
.nxt:   ADDQ.L  #1,rt_using__WINT(A5)
        ADDQ.L  #1,D4
        BRA     .ip
.pt:    MOVE.L  D4,D0                   ; point and digits after it
        MOVE.B  rt_using__dpt,D1
        BSR     rt_using__at
        BNE     .car
        MOVE.L  #1,rt_using__POINT(A5)
        ADDQ.L  #1,D4
.dc:    MOVE.L  D4,D0
        MOVEQ   #'#',D1
        BSR     rt_using__at
        BNE     .car
        ADDQ.L  #1,rt_using__DEC(A5)
        ADDQ.L  #1,D4
        BRA     .dc
.car:   MOVEQ   #0,D3                   ; run of '^' (3 or more: exponent)
.cr:    MOVE.L  D4,D0
        ADD.L   D3,D0
        MOVEQ   #'^',D1
        BSR     rt_using__at
        BNE     .cre
        ADDQ.L  #1,D3
        BRA     .cr
.cre:   CMP.L   #3,D3
        BLT     .tr
        MOVE.L  D3,rt_using__CARETS(A5)
        ADD.L   D3,D4
.tr:    TST.L   rt_using__LEAD(A5)      ; sign at the end
        BNE     .end
        MOVE.L  D4,D0
        MOVEQ   #'-',D1
        BSR     rt_using__at
        BEQ     .ts
        MOVE.L  D4,D0
        MOVEQ   #'+',D1
        BSR     rt_using__at
        BNE     .end
.ts:    MOVEQ   #0,D3
        MOVE.B  0(A3,D4.L),D3
        MOVE.L  D3,rt_using__TRAIL(A5)
        ADDQ.L  #1,D4
.end:   MOVE.L  D4,rt_using__END(A5)
        RTS

; D0 = wrap, D1 = all: literal text from pos up to the next field -> D0 = 1 if a field starts at pos
rt_using__literal:
        MOVE.L  D4,-(SP)                ; D4 = wrap, (SP) = all
        MOVE.L  D0,D4
        MOVE.L  D1,-(SP)
.loop:  MOVE.L  rt_using__pos,D2
.w:     CMP.L   D5,D2
        BGE     .end
        MOVE.B  0(A3,D2.L),D0
        CMP.B   #'_',D0
        BNE     .nu
        MOVE.L  D2,D1                   ; escape: the next character (or '_' at the end)
        ADDQ.L  #1,D1
        CMP.L   D5,D1
        BGE     .ue
        MOVE.B  0(A3,D1.L),D0
.ue:    BSR     rt_using__put
        ADDQ.L  #2,D2
        MOVE.L  D2,rt_using__pos
        BRA     .w
.nu:    TST.L   (SP)
        BNE     .txt
        MOVE.L  D0,-(SP)
        BSR     rt_using__start
        MOVE.L  D0,D1
        MOVE.L  (SP)+,D0
        TST.L   D1
        BEQ     .txt
        MOVEQ   #1,D0                   ; a field
        BRA     .out
.txt:   BSR     rt_using__put
        ADDQ.L  #1,D2
        MOVE.L  D2,rt_using__pos
        BRA     .w
.end:   CMP.L   D5,D2                   ; pos > len (escape at the end): len
        BLE     .e2
        MOVE.L  D5,rt_using__pos
.e2:    TST.L   D4
        BEQ     .none
        TST.L   rt_using__has
        BEQ     .none
        CLR.L   rt_using__pos           ; once more from the start
        MOVEQ   #0,D4
        BRA     .loop
.none:  MOVEQ   #0,D0
.out:   ADDQ.L  #4,SP
        MOVE.L  (SP)+,D4
        RTS

; a field of the wrong kind as text (probe7 T17, T18)
rt_using__mismatch:
        MOVE.L  rt_using__pos,D2
.l:     CMP.L   rt_using__END(A5),D2
        BGE     .out
        MOVE.B  0(A3,D2.L),D0
        BSR     rt_using__put
        ADDQ.L  #1,D2
        BRA     .l
.out:   MOVE.L  D2,rt_using__pos
        RTS

; string field (A5) with the string rt_using__sdesc
rt_using__string:
        MOVE.L  rt_using__sdesc,A0
        MOVEQ   #0,D2
        MOVE.W  4(A0),D2                ; D2 = length
        MOVE.L  (A0),A2                 ; A2 = characters
        MOVE.L  rt_using__KIND(A5),D0
        CMP.L   #2,D0
        BNE     .k3
        MOVEQ   #0,D3                   ; '&': all of it
.a:     CMP.L   D2,D3
        BGE     .out
        MOVE.B  0(A2,D3.L),D0
        BSR     rt_using__put
        ADDQ.L  #1,D3
        BRA     .a
.k3:    CMP.L   #3,D0
        BNE     .k4
        MOVEQ   #' ',D0                 ; '!': the first character or a space (probe7 T19)
        TST.L   D2
        BEQ     .p
        MOVE.B  (A2),D0
.p:     BRA     rt_using__put
.k4:    MOVEQ   #0,D3                   ; '\..\': padded or cut to the width
.b:     CMP.L   rt_using__WIDTH(A5),D3
        BGE     .out
        MOVEQ   #' ',D0
        CMP.L   D2,D3
        BGE     .bp
        MOVE.B  0(A2,D3.L),D0
.bp:    BSR     rt_using__put
        ADDQ.L  #1,D3
        BRA     .b
.out:   RTS

; ---- numbers -------------------------------------------------------------------------------------

; D2 = dd (weight of ds[0] is 10^dd), D3 = dec -> rt_using__n holds the digits of
; N = round(|x| * 10^dec) (ties away from 0), D0 = their count (at least 1)
rt_using__scaled:
        MOVEM.L D2-D4/A2,-(SP)
        LEA     rt_using__n,A1
        LEA     rt_using__ds,A2
        MOVE.L  D2,D4
        ADD.L   D3,D4                   ; D4 = idx (units digit of N in ds)
        CMP.L   #-1,D4
        BGT     .norm
        BLT     .zero
        MOVE.B  #'0',(A1)               ; idx = -1: 1 if ds[0] >= 5
        CMP.B   #'5',(A2)
        BCS     .one
        MOVE.B  #'1',(A1)
.one:   MOVEQ   #1,D0
        BRA     .out
.zero:  MOVE.B  #'0',(A1)
        MOVEQ   #1,D0
        BRA     .out
.norm:  MOVEQ   #0,D0                   ; digits 0..idx (beyond 13: '0')
.cp:    MOVEQ   #'0',D1
        CMP.L   #13,D0
        BGE     .c0
        MOVE.B  0(A2,D0.L),D1
.c0:    MOVE.B  D1,0(A1,D0.L)
        ADDQ.L  #1,D0
        CMP.L   D4,D0
        BLE     .cp
        MOVE.L  D4,D1                   ; round with ds[idx + 1]
        ADDQ.L  #1,D1
        CMP.L   #13,D1
        BGE     .out
        CMP.B   #'5',0(A2,D1.L)
        BCS     .out
        MOVE.L  D0,D1
.inc:   SUBQ.L  #1,D1
        BMI     .carry
        CMP.B   #'9',0(A1,D1.L)
        BEQ     .nine
        ADDQ.B  #1,0(A1,D1.L)
        BRA     .out
.nine:  MOVE.B  #'0',0(A1,D1.L)
        BRA     .inc
.carry: MOVE.L  D0,D1                   ; all nines: a new leading 1
.sh:    MOVE.B  -1(A1,D1.L),0(A1,D1.L)
        SUBQ.L  #1,D1
        BNE     .sh
        MOVE.B  #'1',(A1)
        ADDQ.L  #1,D0
.out:   MOVEM.L (SP)+,D2-D4/A2
        RTS

; D0 = nlen, D3 = dec -> D0 = digits before the point in rt_using__n (without leading zeros, >= 1)
rt_using__intdig:
        MOVE.L  D0,D1
        SUB.L   D3,D1
        SUBQ.L  #1,D1                   ; D1 = units index
        BPL     .u
        MOVEQ   #1,D0
        RTS
.u:     LEA     rt_using__n,A0
        MOVEQ   #0,D0                   ; first
.f:     CMP.L   D1,D0
        BGE     .d
        CMP.B   #'0',0(A0,D0.L)
        BNE     .d
        ADDQ.L  #1,D0
        BRA     .f
.d:     SUB.L   D0,D1
        ADDQ.L  #1,D1
        MOVE.L  D1,D0
        RTS

; number field (A5) with x in rt_using__x
rt_using__number:
        MOVEM.L rt_using__x,D0-D1       ; 13 digits, d, sign
        LEA     rt_using__ds,A0
        MOVEQ   #13,D2
        JSR     rt_flt_dec
        MOVE.L  D0,rt_using__d
        MOVE.L  D1,rt_using__neg
        MOVE.L  rt_using__DEC(A5),D3    ; D3 = dec (at most 300)
        CMP.L   #rt_using__MAXDEC,D3
        BLE     .dm
        MOVE.L  #rt_using__MAXDEC,D3
.dm:    CLR.L   rt_using__e
        TST.L   rt_using__CARETS(A5)
        BEQ     .plain
        MOVE.L  rt_using__NDIG(A5),D4   ; exponent format: e a multiple of k = digit positions
        MOVE.L  rt_using__d,D2
        CMP.B   #'0',rt_using__ds
        BEQ     .e0
        TST.L   D4
        BEQ     .k0
        MOVE.L  D2,D0                   ; d >= 0: d / k * k, else -((-d + k - 1) / k) * k
        BMI     .dn
        DIVU    D4,D0
        BRA     .dk
.dn:    NEG.L   D0
        ADD.L   D4,D0
        SUBQ.L  #1,D0
        DIVU    D4,D0
        NEG.W   D0
.dk:    MULS    D4,D0
        MOVE.L  D0,rt_using__e
        BRA     .e0
.k0:    MOVE.L  D2,D0
        ADDQ.L  #1,D0
        MOVE.L  D0,rt_using__e
.e0:    MOVE.L  rt_using__d,D2
        SUB.L   rt_using__e,D2
        BSR     rt_using__scaled
        CMP.B   #'0',rt_using__ds       ; rounding carried into a new digit: once more
        BEQ     .pad
        TST.L   D4
        BEQ     .ck0
        MOVE.L  D0,-(SP)
        BSR     rt_using__intdig
        MOVE.L  D0,D1
        MOVE.L  (SP)+,D0
        CMP.L   D4,D1
        BLE     .pad
        BRA     .ren
.ck0:   CMP.L   D3,D0                   ; k = 0: nlen > dec and n[0] != '0'
        BLE     .pad
        CMP.B   #'0',rt_using__n
        BEQ     .pad
.ren:   MOVE.L  D4,D1
        BNE     .rk
        MOVEQ   #1,D1
.rk:    ADD.L   D1,rt_using__e
        MOVE.L  rt_using__d,D2
        SUB.L   rt_using__e,D2
        BSR     rt_using__scaled
        BRA     .pad
.plain: MOVE.L  rt_using__d,D2
        BSR     rt_using__scaled
.pad:   MOVE.L  D0,D4                   ; D4 = nlen; at least one digit before the point
        MOVE.L  D3,D1
        ADDQ.L  #1,D1
        SUB.L   D4,D1                   ; D1 = shift
        BLE     .ip
        LEA     rt_using__n,A0
        MOVE.L  D4,D0
.mv:    SUBQ.L  #1,D0
        BMI     .mz
        LEA     0(A0,D0.L),A1
        MOVE.B  (A1),0(A1,D1.L)
        BRA     .mv
.mz:    MOVE.L  D1,D0
.z:     SUBQ.L  #1,D0
        BMI     .zd
        MOVE.B  #'0',0(A0,D0.L)
        BRA     .z
.zd:    MOVE.L  D3,D4
        ADDQ.L  #1,D4
; integer part, right to left: digits with separators, then '$' and sign
.ip:    MOVE.L  D4,D0
        BSR     rt_using__intdig
        MOVE.L  D4,D2
        SUB.L   D3,D2
        SUB.L   D0,D2                   ; D2 = first
        LEA     rt_using__n,A0
        LEA     rt_using__ip,A1
        MOVEQ   #0,D1                   ; D1 = ilen
        MOVE.L  D4,D0
        SUB.L   D3,D0
        SUBQ.L  #1,D0                   ; D0 = i (units index)
        MOVE.L  D0,-(SP)                ; (SP) = units index
.il:    CMP.L   D2,D0
        BLT     .ie
        TST.L   rt_using__THOU(A5)
        BEQ     .nd
        MOVE.L  (SP),D6                 ; pos = units - i; separator before every third
        SUB.L   D0,D6
        BEQ     .nd
        DIVU    #3,D6
        SWAP    D6
        TST.W   D6
        BNE     .nd
        MOVE.B  rt_using__thc,0(A1,D1.L)
        ADDQ.L  #1,D1
.nd:    MOVE.B  0(A0,D0.L),0(A1,D1.L)
        ADDQ.L  #1,D1
        SUBQ.L  #1,D0
        BRA     .il
.ie:    ADDQ.L  #4,SP
        MOVE.L  rt_using__cap,D6        ; D6 was used: the capacity back
        TST.L   rt_using__NDIG(A5)      ; ".##": no units digit
        BNE     .dol
        CMP.L   #1,D1
        BNE     .dol
        CMP.B   #'0',(A1)
        BNE     .dol
        MOVEQ   #0,D1
.dol:   TST.L   rt_using__DOLLAR(A5)
        BEQ     .mi
        MOVE.B  #'$',0(A1,D1.L)
        ADDQ.L  #1,D1
.mi:    TST.L   rt_using__neg           ; minus in a free digit position (not in the exponent format)
        BEQ     .ls
        TST.L   rt_using__LEAD(A5)
        BNE     .ls
        TST.L   rt_using__TRAIL(A5)
        BNE     .ls
        TST.L   rt_using__CARETS(A5)
        BNE     .ls
        MOVE.B  #'-',0(A1,D1.L)
        ADDQ.L  #1,D1
.ls:    MOVE.L  D1,D2                   ; D2 = ilen
        MOVE.L  rt_using__LEAD(A5),D0   ; fixed sign in front
        BEQ     .fld
        BSR     rt_using__sgn
.fld:   MOVE.L  rt_using__WINT(A5),D1   ; D1 = pad
        SUB.L   D2,D1
        MOVE.L  D1,-(SP)
.pl:    SUBQ.L  #1,D1
        BMI     .pe
        MOVEQ   #' ',D0
        TST.L   rt_using__STAR(A5)
        BEQ     .pp
        MOVEQ   #'*',D0
.pp:    BSR     rt_using__put
        BRA     .pl
.pe:    MOVE.L  (SP)+,D1
        MOVE.L  D2,D0                   ; the integer characters, too many dropped at the front
        TST.L   D1
        BPL     .ic
        ADD.L   D1,D0
.ic:    LEA     rt_using__ip,A1
.io:    SUBQ.L  #1,D0
        BMI     .fr
        MOVE.L  D0,-(SP)
        MOVE.B  0(A1,D0.L),D0
        BSR     rt_using__put
        MOVE.L  (SP)+,D0
        BRA     .io
.fr:    TST.L   rt_using__POINT(A5)     ; point and decimals
        BEQ     .ex
        MOVE.B  rt_using__dpt,D0
        BSR     rt_using__put
        MOVE.L  D4,D2
        SUB.L   D3,D2
        LEA     rt_using__n,A1
.fl:    CMP.L   D4,D2
        BGE     .ex
        MOVE.B  0(A1,D2.L),D0
        BSR     rt_using__put
        ADDQ.L  #1,D2
        BRA     .fl
.ex:    MOVE.L  rt_using__CARETS(A5),D2 ; exponent: exactly carets-2 digits (at most 9), leading ones dropped
        BEQ     .ts
        MOVEQ   #'E',D0
        BSR     rt_using__put
        MOVE.L  rt_using__e,D1
        MOVEQ   #'+',D0
        TST.L   D1
        BPL     .ep
        MOVEQ   #'-',D0
        NEG.L   D1
.ep:    BSR     rt_using__put
        LEA     rt_using__tmp+9,A1      ; nine digits of |e| from the end
        MOVEQ   #8,D0
.ed:    DIVU    #10,D1
        SWAP    D1
        ADD.B   #'0',D1
        MOVE.B  D1,-(A1)
        CLR.W   D1
        SWAP    D1
        DBRA    D0,.ed
        SUBQ.L  #2,D2                   ; want = min(carets - 2, 9)
        CMP.L   #9,D2
        BLE     .w
        MOVEQ   #9,D2
.w:     LEA     rt_using__tmp+9,A1
        SUB.L   D2,A1
        BRA     .on
.o:     MOVE.B  (A1)+,D0
        BSR     rt_using__put
.on:    SUBQ.L  #1,D2
        BPL     .o
.ts:    MOVE.L  rt_using__TRAIL(A5),D0  ; fixed sign at the end
        BEQ     .out
        BRA     rt_using__sgn
.out:   RTS

; D0 = '+' or '-' of the field: the sign position
rt_using__sgn:
        TST.L   rt_using__neg
        BEQ     .pos
        MOVEQ   #'-',D0
        BRA     rt_using__put
.pos:   CMP.B   #'+',D0
        BEQ     .p
        MOVEQ   #' ',D0
.p:     BRA     rt_using__put

;! section data
rt_mode:
        DC.L    0
rt_using__sp:
        DC.L    0

;! section bss
rt_using__fdesc:
        DS.L    1
rt_using__pos:
        DS.L    1
rt_using__md:
        DS.L    1
rt_using__has:
        DS.L    1
rt_using__stack:
        DS.L    4*rt_using__DEPTH
rt_using__sdesc:
        DS.L    1
rt_using__x:
        DS.L    2
rt_using__d:
        DS.L    1
rt_using__e:
        DS.L    1
rt_using__neg:
        DS.L    1
rt_using__cap:
        DS.L    1
rt_using__sdsc:
        DS.L    2
rt_using__fl:
        DS.L    13
rt_using__dpt:
        DS.B    1
rt_using__thc:
        DS.B    1
rt_using__ds:
        DS.B    14
rt_using__tmp:
        DS.B    40
rt_using__sfmt:
        DS.B    260
rt_using__n:
        DS.B    720
rt_using__ip:
        DS.B    964
rt_using__buf:
        DS.B    4096
