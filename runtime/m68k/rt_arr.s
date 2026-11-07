;! module    rt_arr
;! provides  rt_arr_dim rt_arr_elem rt_arr_count rt_arr_erase rt_arr_base rt_arr_set_base rt_swap8
;! requires  rt_error rt_imul rt_idiv rt_str_free plat_alloc
;! group     core
;! clobbers  D0-D3/A0-A1
;! doc       Arrays: DIM, element address with checks (errors 14-18), DIM?, ERASE, OPTION BASE.

; An array variable is an 8-byte slot, the descriptor of GFA-BASIC (probe19 T3): +0 address of the
; array block (0: not dimensioned), +4 number of dimensions (word), +6 element size (word, ours).
; Block in the array area: the extents (upper - OPTION BASE + 1, long each) with the last dimension
; first, then the elements (first index fastest; cleared at DIM; strings: descriptors). So
; LPEEK(*a()) is V:a(0..0) - 4 * dimensions, as on the original. The area (from plat_alloc on first
; use) is used like a stack; ERASE of the most recent array gives its room back.

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
        MOVEM.L D1-D3,-(SP)             ; bounds in memory: (SP), 4(SP), 8(SP) -> extents
        MOVEQ   #1,D4                   ; D4 = number of elements
        MOVEQ   #0,D5                   ; D5 = dimension index * 4
.dims:  MOVE.L  0(SP,D5.L),D1
        SUB.L   rt_arr_base,D1          ; extent = u - base + 1
        BMI     .range
        ADDQ.L  #1,D1
        MOVE.L  D1,0(SP,D5.L)
        MOVE.L  D1,-(SP)                ; n * e > 16M?  (n > 16M / e)
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
        MOVE.L  D4,D0                   ; bytes = extents + elements * size, even
        MOVE.L  D7,D1
        JSR     rt_imul
        ADDQ.L  #1,D0
        AND.L   #-2,D0
        MOVE.L  D6,D1
        LSL.L   #2,D1
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
        MOVE.L  A0,(A2)                 ; descriptor
        MOVE.W  D6,4(A2)
        MOVE.W  D7,6(A2)
        MOVE.L  D6,D0                   ; extents, the last dimension first
        LSL.L   #2,D0
.cb:    SUBQ.L  #4,D0
        MOVE.L  0(SP,D0.L),(A0)+
        TST.L   D0
        BNE     .cb
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
        MOVEM.L D4-D7/A2-A3,-(SP)
        MOVE.L  (A0),D4
        BNE     .dimd
        MOVEQ   #15,D0                  ; array undimensioned
        BRA     .err
.dimd:  MOVE.L  D4,A2                   ; A2 = block
        MOVE.L  A0,A3                   ; A3 = slot
        CMP.W   4(A3),D0
        BEQ     .cnt
        MOVEQ   #18,D0                  ; wrong number of indices
        BRA     .err
.cnt:   MOVEM.L D1-D3,-(SP)
        MOVE.L  D0,D6
        LSL.L   #2,D6                   ; D6 = 4 * dimensions
        MOVEQ   #0,D4                   ; D4 = index
        MOVEQ   #1,D7                   ; D7 = multiplier
        MOVEQ   #0,D5                   ; D5 = 4 * dimension number
.ix:    MOVE.L  0(SP,D5.L),D0
        SUB.L   rt_arr_base,D0
        BMI     .range
        MOVE.L  D6,D1                   ; extent of this dimension: block + 4 * (dims - 1 - k)
        SUB.L   D5,D1
        MOVE.L  -4(A2,D1.L),D1
        CMP.L   D1,D0
        BGE     .range
        MOVE.L  D1,-(SP)
        MOVE.L  D7,D1                   ; index += (i - base) * multiplier
        JSR     rt_imul
        ADD.L   D0,D4
        MOVE.L  (SP)+,D1                ; multiplier *= extent
        MOVE.L  D7,D0
        JSR     rt_imul
        MOVE.L  D0,D7
        ADDQ.L  #4,D5
        CMP.L   D6,D5
        BCS     .ix
        LEA     12(SP),SP
        MOVE.L  D4,D0                   ; address = block + 4 * dims + index * size
        MOVEQ   #0,D1
        MOVE.W  6(A3),D1
        JSR     rt_imul
        LEA     0(A2,D6.L),A0
        ADD.L   D0,A0
        MOVEM.L (SP)+,D4-D7/A2-A3
        RTS
.range: LEA     12(SP),SP
        MOVEQ   #16,D0                  ; array index out of range
.err:   JSR     rt_error
        LEA     rt_arr__dummy,A0        ; if the error returns: a harmless element
        CLR.L   (A0)
        CLR.L   4(A0)
        MOVEM.L (SP)+,D4-D7/A2-A3
        RTS

; A0 = slot -> D0 = number of elements (0: not dimensioned)
rt_arr_count:
        MOVE.L  (A0),D0
        BEQ     .out
        MOVEM.L D4-D6/A2,-(SP)
        MOVE.W  4(A0),D6
        MOVE.L  D0,A2
        SUBQ.W  #1,D6
        MOVEQ   #1,D4
.l:     MOVE.L  (A2)+,D1
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
        MOVEM.L D4-D7/A2,-(SP)
        MOVE.L  D1,A2                   ; A2 = block
        MOVE.L  D0,D7
        BSR     rt_arr_count            ; slot still intact
        MOVE.L  D0,D4                   ; D4 = elements
        MOVEQ   #0,D5
        MOVE.W  4(A0),D5
        LSL.L   #2,D5                   ; D5 = 4 * dimensions
        MOVEQ   #0,D6
        MOVE.W  6(A0),D6                ; D6 = element size
        CLR.L   (A0)
        CLR.L   4(A0)
        TST.L   D7
        BEQ     .area
        LEA     0(A2,D5.L),A0           ; string blocks of the elements become garbage
        MOVE.L  D4,D7
        BRA     .fn
.fl:    MOVE.L  A0,-(SP)
        JSR     rt_str_free
        MOVE.L  (SP)+,A0
        ADDQ.L  #8,A0
.fn:    SUBQ.L  #1,D7
        BPL     .fl
.area:  MOVE.L  D4,D0                   ; the most recent block: give the room back
        MOVE.L  D6,D1
        JSR     rt_imul
        ADDQ.L  #1,D0
        AND.L   #-2,D0
        ADD.L   D5,D0
        ADD.L   A2,D0
        CMP.L   rt_arr__top,D0
        BNE     .keep
        MOVE.L  A2,rt_arr__top
.keep:  MOVEM.L (SP)+,D4-D7/A2
.out:   RTS

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
