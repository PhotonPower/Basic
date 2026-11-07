;! module    rt_arr
;! provides  rt_arr_dim rt_arr_elem rt_arr_count rt_arr_erase rt_arr_base rt_arr_set_base rt_swap8
;! requires  rt_error rt_imul rt_idiv rt_str_free plat_alloc
;! group     core
;! clobbers  D0-D3/A0-A1
;! doc       Arrays: DIM, element address with checks (errors 14-18), DIM?, ERASE, OPTION BASE.

; An array variable is an 8-byte slot: +0 address of the array block (0: not dimensioned).
; Block in the array area: +0 number of dimensions (word), +2 element size (word),
; +4 upper bounds (long each), then the elements (cleared at DIM; strings: descriptors).
; The area (from plat_alloc on first use) is used like a stack; ERASE of the most recent array
; gives its room back.

rt_arr__AREASIZE EQU 65536

; A0 = slot, D0 = element size << 8 | number of dimensions (1-3), D1-D3 = upper bounds
rt_arr_dim:
        MOVEM.L D4-D7/A2,-(SP)
        TST.L   (A0)
        BEQ     .new
        MOVEQ   #14,D0                  ; redimensioned array
        BRA     .err
.new:   MOVE.L  A0,A2
        MOVEQ   #0,D6
        MOVE.B  D0,D6                   ; D6 = dimensions
        MOVE.L  D0,D7
        LSR.L   #8,D7                   ; D7 = element size
        MOVEM.L D1-D3,-(SP)             ; bounds in memory: (SP), 4(SP), 8(SP)
        MOVEQ   #1,D4                   ; D4 = number of elements
        MOVEQ   #0,D5                   ; D5 = dimension index
.dims:  MOVE.L  0(SP,D5.L),D1
        SUB.L   rt_arr_base,D1          ; u - base + 1
        BMI     .range
        ADDQ.L  #1,D1
        MOVE.L  D1,-(SP)                ; n * f > 16M?  (n > 16M / f)
        MOVE.L  #16777216,D0
        JSR     rt_idiv
        MOVE.L  (SP)+,D1
        CMP.L   D0,D4
        BHI     .range
        MOVE.L  D4,D0
        JSR     rt_imul
        MOVE.L  D0,D4
        ADDQ.L  #4,D5
        MOVE.L  D6,D0
        LSL.L   #2,D0
        CMP.L   D0,D5
        BCS     .dims
        MOVE.L  D4,D0                   ; bytes = header + elements * size, even
        MOVE.L  D7,D1
        JSR     rt_imul
        ADDQ.L  #1,D0
        AND.L   #-2,D0
        MOVE.L  D6,D1
        LSL.L   #2,D1
        ADDQ.L  #4,D1
        ADD.L   D1,D0                   ; D0 = block size
        MOVE.L  rt_arr__top,D1
        BNE     .t
        MOVE.L  D0,-(SP)                ; first use: the area from the platform
        MOVE.L  #rt_arr__AREASIZE,D0
        JSR     plat_alloc
        MOVE.L  (SP)+,D0
        MOVE.L  A0,D1
        BEQ     .mem
        ADD.L   #rt_arr__AREASIZE,A0
        MOVE.L  A0,rt_arr__hi
.t:     MOVE.L  D1,A0
        ADD.L   D0,D1
        CMP.L   rt_arr__hi,D1
        BHI     .mem
        MOVE.L  D1,rt_arr__top
        MOVE.L  A0,(A2)
        CLR.L   4(A2)
        MOVE.W  D6,(A0)+
        MOVE.W  D7,(A0)+
        SUBQ.L  #1,D6
        MOVE.L  SP,A1
.cb:    MOVE.L  (A1)+,(A0)+
        DBRA    D6,.cb
        SUB.L   A0,D1                   ; clear the elements (an even number of bytes)
        LSR.L   #1,D1
        BRA     .cn
.cl:    CLR.W   (A0)+
.cn:    SUBQ.L  #1,D1
        BPL     .cl
        LEA     12(SP),SP
        MOVEM.L (SP)+,D4-D7/A2
        RTS
.mem:   MOVEQ   #8,D0                   ; out of memory
        BRA     .err2
.range: MOVEQ   #17,D0                  ; dim out of range
.err2:  LEA     12(SP),SP
.err:   JSR     rt_error
        MOVEM.L (SP)+,D4-D7/A2
        RTS

; A0 = slot, D0 = number of indices (1-3), D1-D3 = indices -> A0 = address of the element
rt_arr_elem:
        MOVEM.L D4-D7/A2,-(SP)
        MOVE.L  (A0),D4
        BNE     .dimd
        MOVEQ   #15,D0                  ; array undimensioned
        BRA     .err
.dimd:  MOVE.L  D4,A2                   ; A2 = block
        CMP.W   (A2),D0
        BEQ     .cnt
        MOVEQ   #18,D0                  ; wrong number of indices
        BRA     .err
.cnt:   MOVEM.L D1-D3,-(SP)
        MOVE.L  D0,D6
        LSL.L   #2,D6                   ; D6 = 4 * dimensions
        MOVEQ   #0,D4                   ; D4 = index
        MOVEQ   #1,D7                   ; D7 = multiplier
        MOVEQ   #0,D5
.ix:    MOVE.L  0(SP,D5.L),D0
        SUB.L   rt_arr_base,D0
        BMI     .range
        MOVE.L  4(A2,D5.L),D1           ; upper - base
        SUB.L   rt_arr_base,D1
        CMP.L   D1,D0
        BGT     .range
        MOVE.L  D1,-(SP)
        MOVE.L  D7,D1                   ; index += (i - base) * multiplier
        JSR     rt_imul
        ADD.L   D0,D4
        MOVE.L  (SP)+,D1
        ADDQ.L  #1,D1                   ; multiplier *= upper - base + 1
        MOVE.L  D7,D0
        JSR     rt_imul
        MOVE.L  D0,D7
        ADDQ.L  #4,D5
        CMP.L   D6,D5
        BCS     .ix
        LEA     12(SP),SP
        MOVE.L  D4,D0                   ; address = block + 4 + 4 * dims + index * size
        MOVEQ   #0,D1
        MOVE.W  2(A2),D1
        JSR     rt_imul
        LEA     4(A2,D6.L),A0
        ADD.L   D0,A0
        MOVEM.L (SP)+,D4-D7/A2
        RTS
.range: LEA     12(SP),SP
        MOVEQ   #16,D0                  ; array index out of range
.err:   JSR     rt_error
        LEA     rt_arr__dummy,A0        ; if the error returns: a harmless element
        CLR.L   (A0)
        CLR.L   4(A0)
        MOVEM.L (SP)+,D4-D7/A2
        RTS

; A0 = slot -> D0 = number of elements (0: not dimensioned)
rt_arr_count:
        MOVE.L  (A0),D0
        BEQ     .out
        MOVEM.L D4-D6/A2,-(SP)
        MOVE.L  D0,A2
        MOVE.W  (A2),D6
        SUBQ.W  #1,D6
        MOVEQ   #1,D4
        LEA     4(A2),A2
.l:     MOVE.L  (A2)+,D1
        SUB.L   rt_arr_base,D1
        ADDQ.L  #1,D1
        MOVE.L  D4,D0
        JSR     rt_imul
        MOVE.L  D0,D4
        DBRA    D6,.l
        MOVE.L  D4,D0
        MOVEM.L (SP)+,D4-D6/A2
.out:   RTS

; A0 = slot, D0 = 1 for string arrays: the array becomes undimensioned
rt_arr_erase:
        MOVE.L  (A0),D1
        BEQ     .out
        CLR.L   (A0)
        MOVEM.L D4-D6/A2,-(SP)
        MOVE.L  D1,A2                   ; A2 = block
        TST.L   D0
        BEQ     .area
        MOVE.L  A2,A0                   ; string blocks of the elements become garbage
        BSR     rt_arr__count2
        MOVE.L  D0,D4
        MOVE.W  (A2),D0
        EXT.L   D0
        LSL.L   #2,D0
        LEA     4(A2,D0.L),A0
        BRA     .fn
.fl:    MOVE.L  A0,-(SP)
        JSR     rt_str_free
        MOVE.L  (SP)+,A0
        ADDQ.L  #8,A0
.fn:    SUBQ.L  #1,D4
        BPL     .fl
.area:  MOVE.L  A2,A0                   ; the most recent block: give the room back
        BSR     rt_arr__count2
        MOVEQ   #0,D1
        MOVE.W  2(A2),D1
        JSR     rt_imul
        ADDQ.L  #1,D0
        AND.L   #-2,D0
        MOVE.W  (A2),D1
        EXT.L   D1
        LSL.L   #2,D1
        ADDQ.L  #4,D1
        ADD.L   D1,D0
        ADD.L   A2,D0
        CMP.L   rt_arr__top,D0
        BNE     .keep
        MOVE.L  A2,rt_arr__top
.keep:  MOVEM.L (SP)+,D4-D6/A2
.out:   RTS

; A0 = block -> D0 = number of elements
rt_arr__count2:
        MOVE.L  A0,-(SP)
        MOVE.L  SP,A0
        BSR     rt_arr_count
        ADDQ.L  #4,SP
        RTS

; D0 = 0 or 1: OPTION BASE
rt_arr_set_base:
        MOVE.L  D0,rt_arr_base
        RTS

; A0, A1 = two 8-byte slots: exchange their contents (SWAP of numbers and whole arrays)
rt_swap8:
        MOVE.L  (A0),D0
        MOVE.L  (A1),(A0)
        MOVE.L  D0,(A1)
        MOVE.L  4(A0),D0
        MOVE.L  4(A1),4(A0)
        MOVE.L  D0,4(A1)
        RTS

;! section data
rt_arr_base:
        DC.L    0
rt_arr__top:
        DC.L    0
rt_arr__hi:
        DC.L    0

;! section bss
rt_arr__dummy:
        DS.L    2
