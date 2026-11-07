;! module    rt_strfn
;! provides  rt_str_midset rt_str_lset rt_str_rset rt_str_concat rt_str_cmp rt_str_len rt_str_asc rt_str_chr rt_str_left rt_str_right rt_str_mid rt_str_space rt_str_string rt_str_instr rt_str_upper rt_str_trim rt_str_pack rt_str_str rt_str_radix
;! requires  rt_str_newtemp rt_error rt_fmt_flt rt_defnum
;! group     string
;! clobbers  D0-D3/A0-A1
;! doc       String functions (semantics of platform/pc/rt_pc.cpp); results are new temporaries.

; Arguments: strings as descriptors in A0, A1; numbers in D0, D1, D2. String results: A0 =
; temporary descriptor; numbers: D0. A temporary may move other blocks (compaction), so the
; characters of a source are read only after the allocation.

; A0 = source descriptor, D0 = offset (0-based), D1 = count -> A0 = temporary with the part
rt_strfn__sub:
        MOVEM.L D2-D3/A2,-(SP)
        MOVE.L  A0,A2
        MOVE.L  D0,D2
        MOVE.L  D1,D3
        MOVE.L  D1,D0
        JSR     rt_str_newtemp
        MOVE.L  A1,-(SP)
        TST.L   D3
        BEQ     .done
        MOVE.L  (A2),A1                 ; source characters (after the allocation)
        ADD.L   D2,A1
        SUBQ.L  #1,D3
.cp:    MOVE.B  (A1)+,(A0)+
        DBRA    D3,.cp
.done:  MOVE.L  (SP)+,A0
        MOVEM.L (SP)+,D2-D3/A2
        RTS

rt_strfn__err10:
        MOVEQ   #10,D0                  ; string too long (also negative lengths, probe4 T45)
        JSR     rt_error
        MOVEQ   #0,D0
        JSR     rt_str_newtemp
        MOVE.L  A1,A0
        RTS

; A0 = a, A1 = b -> A0 = a + b (error 10 above 32767 characters)
rt_str_concat:
        MOVEM.L D2-D3/A2-A3,-(SP)
        MOVE.L  A0,A2
        MOVE.L  A1,A3
        MOVEQ   #0,D2
        MOVE.W  4(A2),D2
        MOVEQ   #0,D3
        MOVE.W  4(A3),D3
        MOVE.L  D2,D0
        ADD.L   D3,D0
        CMP.L   #32767,D0
        BLE     .ok
        MOVEM.L (SP)+,D2-D3/A2-A3
        BRA     rt_strfn__err10
.ok:    JSR     rt_str_newtemp
        MOVE.L  A1,-(SP)
        MOVE.L  (A2),A1
        BRA     .n1
.c1:    MOVE.B  (A1)+,(A0)+
.n1:    SUBQ.L  #1,D2
        BPL     .c1
        MOVE.L  (A3),A1
        BRA     .n2
.c2:    MOVE.B  (A1)+,(A0)+
.n2:    SUBQ.L  #1,D3
        BPL     .c2
        MOVE.L  (SP)+,A0
        MOVEM.L (SP)+,D2-D3/A2-A3
        RTS

; A0 = a, A1 = b -> D0 = -1, 0, 1 (bytes unsigned; a prefix is smaller)
rt_str_cmp:
        MOVEM.L D2-D3,-(SP)
        MOVEQ   #0,D2
        MOVE.W  4(A0),D2
        MOVEQ   #0,D3
        MOVE.W  4(A1),D3
        MOVE.L  (A0),A0
        MOVE.L  (A1),A1
.loop:  TST.L   D2
        BEQ     .aend
        TST.L   D3
        BEQ     .gt
        CMPM.B  (A1)+,(A0)+
        BHI     .gt
        BCS     .lt
        SUBQ.L  #1,D2
        SUBQ.L  #1,D3
        BRA     .loop
.aend:  TST.L   D3
        BEQ     .eq
.lt:    MOVEQ   #-1,D0
        BRA     .out
.gt:    MOVEQ   #1,D0
        BRA     .out
.eq:    MOVEQ   #0,D0
.out:   MOVEM.L (SP)+,D2-D3
        RTS

rt_str_len:
        MOVEQ   #0,D0
        MOVE.W  4(A0),D0
        RTS

rt_str_asc:
        MOVEQ   #0,D0
        TST.W   4(A0)
        BEQ     .out
        MOVE.L  (A0),A0
        MOVE.B  (A0),D0
.out:   RTS

; D0 = code -> CHR$(code AND 255)
rt_str_chr:
        MOVE.L  D0,-(SP)
        MOVEQ   #1,D0
        JSR     rt_str_newtemp
        MOVE.L  (SP)+,D0
        MOVE.B  D0,(A0)
        MOVE.L  A1,A0
        RTS

; A0 = s, D0 = n
rt_str_left:
        TST.L   D0
        BMI     rt_strfn__err10
        MOVEQ   #0,D1
        MOVE.W  4(A0),D1
        CMP.L   D1,D0
        BLE     .n
        MOVE.L  D1,D0
.n:     MOVE.L  D0,D1
        MOVEQ   #0,D0
        BRA     rt_strfn__sub

rt_str_right:
        TST.L   D0
        BMI     rt_strfn__err10
        MOVEQ   #0,D1
        MOVE.W  4(A0),D1
        CMP.L   D1,D0
        BLE     .n
        MOVE.L  D1,D0
.n:     SUB.L   D0,D1                   ; offset = length - n
        EXG     D0,D1
        BRA     rt_strfn__sub

; A0 = s, D0 = from (1-based, 0 counts as 1), D1 = count
rt_str_mid:
        TST.L   D0
        BMI     rt_strfn__err10
        TST.L   D1
        BMI     rt_strfn__err10
        TST.L   D0
        BNE     .f
        MOVEQ   #1,D0
.f:     MOVE.L  D2,-(SP)
        MOVEQ   #0,D2
        MOVE.W  4(A0),D2
        CMP.L   D2,D0                   ; from > length: ""
        BLE     .in
        MOVEQ   #0,D1
        MOVEQ   #0,D0
        BRA     .go
.in:    SUBQ.L  #1,D0                   ; offset
        SUB.L   D0,D2                   ; characters left
        CMP.L   D2,D1
        BLE     .go
        MOVE.L  D2,D1
.go:    MOVE.L  (SP)+,D2
        BRA     rt_strfn__sub

; D0 = n -> n spaces
rt_str_space:
        TST.L   D0
        BMI     rt_strfn__err10
        CMP.L   #32767,D0
        BGT     rt_strfn__err10
        MOVE.L  D0,-(SP)
        JSR     rt_str_newtemp
        MOVE.L  (SP)+,D0
        BRA     .n
.l:     MOVE.B  #' ',(A0)+
.n:     SUBQ.L  #1,D0
        BPL     .l
        MOVE.L  A1,A0
        RTS

; D0 = n, A0 = unit -> n copies of the unit
rt_str_string:
        TST.L   D0
        BMI     rt_strfn__err10
        CMP.L   #32767,D0
        BGT     rt_strfn__err10
        MOVEM.L D2-D3/A2,-(SP)
        MOVE.L  A0,A2
        MOVE.L  D0,D2                   ; D2 = n
        MOVEQ   #0,D3
        MOVE.W  4(A2),D3                ; D3 = unit length
        MOVE.L  D2,D0
        MULU    D3,D0                   ; n * length (both < 2^16)
        CMP.L   #32767,D0
        BLE     .ok
        MOVEM.L (SP)+,D2-D3/A2
        BRA     rt_strfn__err10
.ok:    JSR     rt_str_newtemp
        MOVE.L  A1,-(SP)
        BRA     .next
.rep:   MOVE.L  (A2),A1
        MOVE.L  D3,D0
        BRA     .cn
.c:     MOVE.B  (A1)+,(A0)+
.cn:    SUBQ.L  #1,D0
        BPL     .c
.next:  SUBQ.L  #1,D2
        BPL     .rep
        MOVE.L  (SP)+,A0
        MOVEM.L (SP)+,D2-D3/A2
        RTS

; A0 = a, A1 = b, D0 = from -> D0 = position (0: not found); empty b as decided in D12
rt_str_instr:
        TST.L   D0
        BMI     .err
        MOVEM.L D2-D5/A2-A3,-(SP)
        TST.L   D0
        BNE     .f
        MOVEQ   #1,D0
.f:     MOVEQ   #0,D2
        MOVE.W  4(A0),D2                ; D2 = length of a
        MOVEQ   #0,D3
        MOVE.W  4(A1),D3                ; D3 = length of b
        BNE     .search
        MOVE.L  D2,D1                   ; empty b: from if from <= len(a) + 1 (1 if a is empty), else 0
        ADDQ.L  #1,D1
        CMP.L   D1,D0
        BGT     .none
        TST.L   D2
        BNE     .out
        MOVEQ   #1,D0
        BRA     .out
.search:
        CMP.L   D2,D0                   ; from > len(a): 0
        BGT     .none
        MOVE.L  (A0),A2
        MOVE.L  (A1),A3
        MOVE.L  D0,D4                   ; D4 = position (1-based)
.try:   MOVE.L  D2,D1                   ; enough characters left?
        SUB.L   D4,D1
        ADDQ.L  #1,D1
        CMP.L   D3,D1
        BLT     .none
        LEA     -1(A2,D4.L),A0
        MOVE.L  A3,A1
        MOVE.L  D3,D5
        SUBQ.L  #1,D5
.cmp:   CMPM.B  (A1)+,(A0)+
        BNE     .no
        DBRA    D5,.cmp
        MOVE.L  D4,D0
        BRA     .out
.no:    ADDQ.L  #1,D4
        BRA     .try
.none:  MOVEQ   #0,D0
.out:   MOVEM.L (SP)+,D2-D5/A2-A3
        RTS
.err:   MOVEQ   #10,D0
        JSR     rt_error
        MOVEQ   #0,D0
        RTS

; A0 = s -> upper case (a-z and the Amiga's Latin-1 letters $E0-$FE except $F7)
rt_str_upper:
        MOVEQ   #0,D0
        MOVEQ   #0,D1
        MOVE.W  4(A0),D1
        JSR     rt_strfn__sub
        MOVEQ   #0,D1
        MOVE.W  4(A0),D1
        MOVE.L  (A0),A1
        BRA     .n
.l:     MOVE.B  (A1),D0
        CMP.B   #'a',D0
        BCS     .s
        CMP.B   #'z',D0
        BLS     .up
        CMP.B   #$E0,D0
        BCS     .s
        CMP.B   #$F7,D0
        BEQ     .s
        CMP.B   #$FE,D0
        BHI     .s
.up:    SUB.B   #32,D0
        MOVE.B  D0,(A1)
.s:     ADDQ.L  #1,A1
.n:     SUBQ.L  #1,D1
        BPL     .l
        RTS

; A0 = s -> without spaces at both ends
rt_str_trim:
        MOVEQ   #0,D1
        MOVE.W  4(A0),D1                ; D1 = end (exclusive)
        MOVE.L  (A0),A1
        MOVEQ   #0,D0                   ; D0 = start
.lead:  CMP.L   D1,D0
        BCC     .empty
        CMP.B   #' ',0(A1,D0.L)
        BNE     .tail
        ADDQ.L  #1,D0
        BRA     .lead
.tail:  CMP.B   #' ',-1(A1,D1.L)
        BNE     .cut
        SUBQ.L  #1,D1
        BRA     .tail
.cut:   SUB.L   D0,D1
        JMP     rt_strfn__sub
.empty: MOVEQ   #0,D0
        MOVEQ   #0,D1
        JMP     rt_strfn__sub

; A0 = s -> D0 = up to 4 characters packed big-endian (SELECT on a string)
rt_str_pack:
        MOVEQ   #0,D0
        MOVEQ   #0,D1
        MOVE.W  4(A0),D1
        MOVE.L  (A0),A0
        CMP.L   #4,D1
        BLS     .n
        MOVEQ   #4,D1
.n:     SUBQ.L  #1,D1
        BMI     .out
.l:     LSL.L   #8,D0
        MOVE.B  (A0)+,D0
        DBRA    D1,.l
.out:   RTS

; D0:D1 = x -> STR$(x) with DEFNUM digits
rt_str_str:
        LINK    A6,#-32
        LEA     -32(A6),A0
        MOVE.W  rt_defnum,D2
        JSR     rt_fmt_flt
        BSR     rt_strfn__frombuf
        UNLK    A6
        RTS

; -32(A6) holds D0 characters -> A0 = temporary
rt_strfn__frombuf:
        MOVE.L  D0,-(SP)
        JSR     rt_str_newtemp
        MOVE.L  (SP)+,D0
        LEA     -32(A6),A0
        MOVE.L  (A1),D1
        EXG     D1,A1                   ; A1 = characters of the temporary, D1 = descriptor
        BRA     .n
.l:     MOVE.B  (A0)+,(A1)+
.n:     SUBQ.L  #1,D0
        BPL     .l
        MOVE.L  D1,A0
        RTS

; D0 = value (32 bit, unsigned digits), D1 = base (2, 8, 16), D2 = width (<= 0: as needed)
; -> HEX$, BIN$, OCT$ (digits beyond the width are cut off at the front)
rt_str_radix:
        LINK    A6,#-32
        MOVEM.L D3-D5,-(SP)
        LEA     -32(A6),A0              ; digits from the end of the buffer
        LEA     32(A0),A1
        MOVEQ   #0,D3                   ; D3 = digit count
        MOVE.L  D1,D5                   ; D5 = shift per digit (1, 3 or 4)
        MOVEQ   #4,D4
        CMP.L   #16,D1
        BEQ     .sh
        MOVEQ   #3,D4
        CMP.L   #8,D1
        BEQ     .sh
        MOVEQ   #1,D4
.sh:    SUBQ.L  #1,D5                   ; mask = base - 1
.dig:   MOVE.L  D0,D1
        AND.L   D5,D1
        MOVE.B  rt_strfn__digits(PC,D1.L),-(A1)
        ADDQ.L  #1,D3
        LSR.L   D4,D0
        BNE     .dig
        TST.L   D2                      ; width: pad with '0' or cut
        BLE     .done
.pad:   CMP.L   D2,D3
        BGE     .cutw
        MOVE.B  #'0',-(A1)
        ADDQ.L  #1,D3
        BRA     .pad
.cutw:  SUB.L   D2,D3                   ; drop leading digits
        ADD.L   D3,A1
        MOVE.L  D2,D3
.done:  MOVE.L  D3,D0                   ; copy to the start of the buffer
        LEA     -32(A6),A0
        MOVE.L  D3,D1
        BRA     .mn
.mv:    MOVE.B  (A1)+,(A0)+
.mn:    SUBQ.L  #1,D1
        BPL     .mv
        MOVEM.L (SP)+,D3-D5
        BSR     rt_strfn__frombuf
        UNLK    A6
        RTS

rt_strfn__digits:
        DC.B    '0123456789ABCDEF'
        EVEN

; A0 = target descriptor, A1 = source, D0 = from (< 1 counts as 1), D1 = count:
; MID$(a$,from,count)=b$ overwrites in place, the length stays (probe4 T46)
rt_str_midset:
        MOVEM.L D2-D3,-(SP)
        TST.L   D0
        BGT     .f
        MOVEQ   #1,D0
.f:     MOVEQ   #0,D2
        MOVE.W  4(A0),D2                ; D2 = characters of a$ from position from on
        SUB.L   D0,D2
        ADDQ.L  #1,D2
        BLE     .out
        CMP.L   D1,D2
        BLE     .c1
        MOVE.L  D1,D2
.c1:    MOVEQ   #0,D3
        MOVE.W  4(A1),D3
        CMP.L   D3,D2
        BLE     .c2
        MOVE.L  D3,D2
.c2:    TST.L   D2
        BLE     .out
        MOVE.L  (A0),A0
        LEA     -1(A0,D0.L),A0
        MOVE.L  (A1),A1
        SUBQ.L  #1,D2
.cp:    MOVE.B  (A1)+,(A0)+
        DBRA    D2,.cp
.out:   MOVEM.L (SP)+,D2-D3
        RTS

; A0 = target descriptor, A1 = source: LSET a$=b$ (b$ cut or filled with spaces at the end)
rt_str_lset:
        MOVEM.L D2-D3,-(SP)
        MOVEQ   #0,D2
        MOVE.W  4(A0),D2                ; D2 = length of a$
        MOVEQ   #0,D3
        MOVE.W  4(A1),D3
        CMP.L   D2,D3
        BLE     .n
        MOVE.L  D2,D3                   ; D3 = characters taken from b$
.n:     MOVE.L  (A0),A0
        MOVE.L  (A1),A1
        SUB.L   D3,D2                   ; D2 = spaces
        BRA     .cn
.c:     MOVE.B  (A1)+,(A0)+
.cn:    SUBQ.L  #1,D3
        BPL     .c
        BRA     .sn
.s:     MOVE.B  #' ',(A0)+
.sn:    SUBQ.L  #1,D2
        BPL     .s
        MOVEM.L (SP)+,D2-D3
        RTS

; A0 = target descriptor, A1 = source: RSET a$=b$ (spaces in front)
rt_str_rset:
        MOVEM.L D2-D3,-(SP)
        MOVEQ   #0,D2
        MOVE.W  4(A0),D2
        MOVEQ   #0,D3
        MOVE.W  4(A1),D3
        CMP.L   D2,D3
        BLE     .n
        MOVE.L  D2,D3
.n:     MOVE.L  (A0),A0
        MOVE.L  (A1),A1
        SUB.L   D3,D2
        BRA     .sn
.s:     MOVE.B  #' ',(A0)+
.sn:    SUBQ.L  #1,D2
        BPL     .s
        BRA     .cn
.c:     MOVE.B  (A1)+,(A0)+
.cn:    SUBQ.L  #1,D3
        BPL     .c
        MOVEM.L (SP)+,D2-D3
        RTS
