;! module    rt_mat
;! provides  rt_mat_v rt_mat_set_base rt_mat_view rt_mat_at rt_mat_limit rt_mat_fill rt_mat_one rt_mat_el rt_mat_get
;! provides  rt_mat_addsub rt_mat_scalar rt_mat_mul rt_mat_dot rt_mat_triple rt_mat_norm rt_mat_det rt_mat_rank rt_mat_inv
;! provides  rt_mat_trans rt_mat_cpy
;! requires  rt_error rt_imul rt_arr_base fp_add fp_sub fp_mul fp_div fp_sqrt fp_from_i32 plat_alloc
;! group     matrix
;! clobbers  D0-D3/A0-A1
;! doc       MAT commands (D23): the operations of runtime/ref/rt_mat.c in the same order, on views.

; A view (24 bytes, four of them in rt_mat_v): +0 address of element (0,0), +4 rows, +8 columns,
; +12 row step, +16 column step (bytes), +20 dimensions of the array. Element (i,j) is at
; p + i * rs + j * cs. The program builds the views (rt_mat_view, rt_mat_at, rt_mat_limit), then
; calls the operation; operands are view 0, 1, 2 in the order of runtime/ref/rt_mat.c.

rt_mat__P     EQU 0
rt_mat__R     EQU 4
rt_mat__C     EQU 8
rt_mat__RS    EQU 12
rt_mat__CS    EQU 16
rt_mat__D     EQU 20
rt_mat__VS  EQU 24

; D0 = MAT BASE (0 or 1)
rt_mat_set_base:
        MOVEQ   #0,D1
        TST.L   D0
        BNE     .s
        MOVEQ   #1,D1
.s:     MOVE.L  D1,rt_mat__b0
        RTS

; -> D0 = first index of a matrix: MAT BASE, at least OPTION BASE
rt_mat__lo:
        MOVEQ   #1,D0
        SUB.L   rt_mat__b0,D0
        CMP.L   rt_arr_base,D0
        BGE     .o
        MOVE.L  rt_arr_base,D0
.o:     RTS

; D0 = view number -> A0 = view
rt_mat__vp:
        MULU    #rt_mat__VS,D0
        LEA     rt_mat_v,A0
        ADD.L   D0,A0
        RTS

; A0 = slot of a float array, D0 = view number, D1 = 1: a 1-D array as column (n x 1)
rt_mat_view:
        MOVEM.L D4-D5/A2,-(SP)
        MOVE.L  D1,D5
        MOVE.L  (A0),D2
        BNE     .dim
        MOVEQ   #15,D0                  ; array undimensioned
        BRA     .err
.dim:   MOVE.L  D2,A1                   ; A1 = block
        BSR     rt_mat__vp
        MOVE.L  A0,A2                   ; A2 = view
        MOVEQ   #0,D3
        MOVE.W  (A1),D3                 ; dimensions
        CMP.L   #2,D3
        BLS     .ok
        MOVEQ   #18,D0                  ; wrong number of indices
        BRA     .err
.ok:    MOVE.L  D3,rt_mat__D(A2)
        BSR     rt_mat__lo
        MOVE.L  D0,D4                   ; D4 = lo
        MOVE.L  D3,D1
        LSL.L   #2,D1
        LEA     4(A1,D1.L),A0           ; A0 = elements
        MOVE.L  D4,D2
        SUB.L   rt_arr_base,D2          ; D2 = lo - OPTION BASE (0 or 1)
        MOVE.L  4(A1),D0                ; first extent: u0 - lo + 1, at least 0
        SUB.L   D4,D0
        ADDQ.L  #1,D0
        BPL     .p0
        MOVEQ   #0,D0
.p0:    CMP.L   #1,D3
        BNE     .two
        TST.L   D2
        BEQ     .n1
        ADDQ.L  #8,A0
.n1:    MOVE.L  A0,rt_mat__P(A2)
        TST.L   D5
        BNE     .col
        MOVE.L  #1,rt_mat__R(A2)              ; row vector 1 x n
        MOVE.L  D0,rt_mat__C(A2)
        CLR.L   rt_mat__RS(A2)
        MOVE.L  #8,rt_mat__CS(A2)
        BRA     .out
.col:   MOVE.L  D0,rt_mat__R(A2)              ; column vector n x 1
        MOVE.L  #1,rt_mat__C(A2)
        MOVE.L  #8,rt_mat__RS(A2)
        CLR.L   rt_mat__CS(A2)
        BRA     .out
.two:   MOVE.L  D0,rt_mat__R(A2)
        MOVE.L  #8,rt_mat__RS(A2)
        MOVE.L  4(A1),D1                ; column step: (u0 - OPTION BASE + 1) * 8
        SUB.L   rt_arr_base,D1
        ADDQ.L  #1,D1
        LSL.L   #3,D1
        MOVE.L  D1,rt_mat__CS(A2)
        MOVE.L  8(A1),D0
        SUB.L   D4,D0
        ADDQ.L  #1,D0
        BPL     .p1
        MOVEQ   #0,D0
.p1:    MOVE.L  D0,rt_mat__C(A2)
        TST.L   D2
        BEQ     .n2
        ADD.L   D1,A0
        ADDQ.L  #8,A0
.n2:    MOVE.L  A0,rt_mat__P(A2)
.out:   MOVEM.L (SP)+,D4-D5/A2
        RTS
.err:   MOVEM.L (SP)+,D4-D5/A2
        JMP     rt_error

; D0 = view number | indices given << 8 | CPY << 10 | XCPY << 11, D1 = i, D2 = j (BASIC indices):
; move the view to the offsets. DET/RANG: a 1-D array takes i as column offset, a 2-D one both or
; none; outside the matrix error 16. CPY/XCPY: a 1-D array takes i as row offset (XCPY: column), a 2-D
; one i and j (j missing: 0); a negative offset leaves an empty view (nothing is copied).
rt_mat_at:
        MOVEM.L D4-D7/A2,-(SP)
        MOVE.L  D0,D7
        MOVE.L  D1,D5
        MOVE.L  D2,D6
        AND.L   #255,D0
        BSR     rt_mat__vp
        MOVE.L  A0,A2
        BSR     rt_mat__lo
        SUB.L   D0,D5                   ; D5 = i - lo
        SUB.L   D0,D6                   ; D6 = j - lo
        MOVE.L  D7,D3
        LSR.L   #8,D3
        AND.L   #3,D3                   ; D3 = indices given
        BEQ     .out
        MOVEQ   #0,D1                   ; D1 = row offset, D2 = column offset
        MOVEQ   #0,D2
        BTST    #10,D7
        BNE     .cpy
        CMP.L   #1,rt_mat__D(A2)
        BNE     .s2
        MOVE.L  D5,D2
        BRA     .chk
.s2:    CMP.L   #2,D3
        BNE     .chk
        MOVE.L  D5,D1
        MOVE.L  D6,D2
.chk:   TST.L   D1
        BMI     .range
        TST.L   D2
        BMI     .range
        CMP.L   rt_mat__R(A2),D1
        BGT     .range
        CMP.L   rt_mat__C(A2),D2
        BGT     .range
        BRA     .move
.cpy:   CMP.L   #1,rt_mat__D(A2)
        BNE     .c2
        BTST    #11,D7
        BNE     .cx
        MOVE.L  D5,D1
        BRA     .cchk
.cx:    MOVE.L  D5,D2
        BRA     .cchk
.c2:    MOVE.L  D5,D1
        CMP.L   #2,D3
        BNE     .cchk
        MOVE.L  D6,D2
.cchk:  TST.L   D1
        BMI     .empty
        TST.L   D2
        BPL     .move
.empty: CLR.L   rt_mat__R(A2)
        CLR.L   rt_mat__C(A2)
        BRA     .out
.move:  SUB.L   D1,rt_mat__R(A2)
        SUB.L   D2,rt_mat__C(A2)
        MOVE.L  D2,D6
        MOVE.L  D1,D0
        MOVE.L  rt_mat__RS(A2),D1
        JSR     rt_imul
        ADD.L   D0,rt_mat__P(A2)
        MOVE.L  D6,D0
        MOVE.L  rt_mat__CS(A2),D1
        JSR     rt_imul
        ADD.L   D0,rt_mat__P(A2)
.out:   MOVEM.L (SP)+,D4-D7/A2
        RTS
.range: MOVEM.L (SP)+,D4-D7/A2
        MOVEQ   #16,D0                  ; array index out of range
        JMP     rt_error

; D0 = view number, D1 = n: at most n x n (n >= 0)
rt_mat_limit:
        TST.L   D1
        BMI     .out
        BSR     rt_mat__vp
        CMP.L   rt_mat__R(A0),D1
        BGE     .c
        MOVE.L  D1,rt_mat__R(A0)
.c:     CMP.L   rt_mat__C(A0),D1
        BGE     .out
        MOVE.L  D1,rt_mat__C(A0)
.out:   RTS

; A0 = view, D0 = i, D1 = j -> A1 = address of the element (A0 kept)
rt_mat_el:
        MOVE.L  D1,-(SP)
        MOVE.L  rt_mat__RS(A0),D1
        JSR     rt_imul
        MOVE.L  D0,A1
        MOVE.L  (SP)+,D0
        MOVE.L  rt_mat__CS(A0),D1
        JSR     rt_imul
        ADD.L   D0,A1
        ADD.L   rt_mat__P(A0),A1
        RTS

; D0 = view number, D1 = i, D2 = j -> D0:D1 = element
rt_mat_get:
        MOVE.L  D2,-(SP)
        BSR     rt_mat__vp
        MOVE.L  D1,D0
        MOVE.L  (SP)+,D1
        BSR     rt_mat_el
        MOVE.L  (A1),D0
        MOVE.L  4(A1),D1
        RTS

; D0 = bytes -> A0 = work storage (grows when needed; error 8 if there is no room)
rt_mat__work:
        CMP.L   rt_mat__wcap,D0
        BLS     .ok
        MOVE.L  D0,-(SP)
        JSR     plat_alloc
        MOVE.L  (SP)+,D0
        MOVE.L  A0,D1
        BNE     .got
        MOVEQ   #8,D0                   ; memory full
        JMP     rt_error
.got:   MOVE.L  A0,rt_mat__wbuf
        MOVE.L  D0,rt_mat__wcap
.ok:    MOVE.L  rt_mat__wbuf,A0
        RTS

; A0 = slot, D0:D1 = x: MAT CLR / MAT SET, the whole array (index 0 included)
rt_mat_fill:
        MOVEM.L D4-D7/A2,-(SP)
        MOVE.L  D0,D4
        MOVE.L  D1,D5
        MOVE.L  (A0),D0
        BNE     .dim
        MOVEM.L (SP)+,D4-D7/A2
        MOVEQ   #15,D0
        JMP     rt_error
.dim:   MOVE.L  D0,A2
        MOVEQ   #0,D6
        MOVE.W  (A2),D6                 ; dimensions
        MOVEQ   #1,D7                   ; D7 = elements
        LEA     4(A2),A2
.cnt:   MOVE.L  (A2)+,D1
        SUB.L   rt_arr_base,D1
        ADDQ.L  #1,D1
        MOVE.L  D7,D0
        JSR     rt_imul
        MOVE.L  D0,D7
        SUBQ.L  #1,D6
        BNE     .cnt
        BRA     .n                      ; A2 = elements
.f:     MOVE.L  D4,(A2)+
        MOVE.L  D5,(A2)+
.n:     SUBQ.L  #1,D7
        BPL     .f
        MOVEM.L (SP)+,D4-D7/A2
        RTS

; MAT ONE: view 0 square (else 82) -> 1 on the diagonal, 0 elsewhere
rt_mat_one:
        MOVEM.L D6-D7,-(SP)
        LEA     rt_mat_v,A0
        MOVE.L  rt_mat__R(A0),D0
        CMP.L   rt_mat__C(A0),D0
        BEQ     .sq
        MOVEM.L (SP)+,D6-D7
        MOVEQ   #82,D0
        JMP     rt_error
.sq:    MOVEQ   #0,D6
        BRA     .in
.i:     MOVEQ   #0,D7
        BRA     .jn
.j:     LEA     rt_mat_v,A0
        MOVE.L  D6,D0
        MOVE.L  D7,D1
        BSR     rt_mat_el
        CLR.L   (A1)
        CLR.L   4(A1)
        CMP.L   D6,D7
        BNE     .z
        MOVE.L  #$3FF00000,(A1)
.z:     ADDQ.L  #1,D7
.jn:    CMP.L   rt_mat_v+rt_mat__C,D7
        BLT     .j
        ADDQ.L  #1,D6
.in:    CMP.L   rt_mat_v+rt_mat__R,D6
        BLT     .i
        MOVEM.L (SP)+,D6-D7
        RTS

; D0 = 1: subtract. view 0 = view 1 +/- view 2 (same order, else 77)
rt_mat_addsub:
        MOVEM.L D4-D7/A5,-(SP)
        LEA     fp_add,A5
        TST.L   D0
        BEQ     .a
        LEA     fp_sub,A5
.a:     LEA     rt_mat_v,A0
        MOVE.L  rt_mat__R(A0),D0
        CMP.L   rt_mat__VS+rt_mat__R(A0),D0
        BNE     .ord
        MOVE.L  rt_mat__C(A0),D0
        CMP.L   rt_mat__VS+rt_mat__C(A0),D0
        BNE     .ord
        MOVE.L  rt_mat__VS+rt_mat__R(A0),D0
        CMP.L   2*rt_mat__VS+rt_mat__R(A0),D0
        BNE     .ord
        MOVE.L  rt_mat__VS+rt_mat__C(A0),D0
        CMP.L   2*rt_mat__VS+rt_mat__C(A0),D0
        BNE     .ord
        MOVEQ   #0,D6
        BRA     .in
.i:     MOVEQ   #0,D7
        BRA     .jn
.j:     MOVEQ   #1,D0
        MOVE.L  D6,D1
        MOVE.L  D7,D2
        BSR     rt_mat_get
        MOVE.L  D0,D4
        MOVE.L  D1,D5
        MOVEQ   #2,D0
        MOVE.L  D6,D1
        MOVE.L  D7,D2
        BSR     rt_mat_get
        MOVE.L  D0,D2
        MOVE.L  D1,D3
        MOVE.L  D4,D0
        MOVE.L  D5,D1
        JSR     (A5)
        MOVE.L  D0,D4
        MOVE.L  D1,D5
        LEA     rt_mat_v,A0
        MOVE.L  D6,D0
        MOVE.L  D7,D1
        BSR     rt_mat_el
        MOVE.L  D4,(A1)
        MOVE.L  D5,4(A1)
        ADDQ.L  #1,D7
.jn:    CMP.L   rt_mat_v+rt_mat__C,D7
        BLT     .j
        ADDQ.L  #1,D6
.in:    CMP.L   rt_mat_v+rt_mat__R,D6
        BLT     .i
        MOVEM.L (SP)+,D4-D7/A5
        RTS
.ord:   MOVEM.L (SP)+,D4-D7/A5
        MOVEQ   #77,D0
        JMP     rt_error

; D0:D1 = x, D2 = 0 add, 1 subtract, 2 multiply: every element of view 0 (element op x)
rt_mat_scalar:
        MOVEM.L D6-D7/A2/A5,-(SP)
        MOVE.L  D0,rt_mat__x
        MOVE.L  D1,rt_mat__x+4
        LEA     fp_add,A5
        CMP.L   #1,D2
        BLT     .a
        LEA     fp_sub,A5
        BEQ     .a
        LEA     fp_mul,A5
.a:     MOVEQ   #0,D6
        BRA     .in
.i:     MOVEQ   #0,D7
        BRA     .jn
.j:     LEA     rt_mat_v,A0
        MOVE.L  D6,D0
        MOVE.L  D7,D1
        BSR     rt_mat_el
        MOVE.L  A1,A2
        MOVE.L  (A2),D0
        MOVE.L  4(A2),D1
        MOVE.L  rt_mat__x,D2
        MOVE.L  rt_mat__x+4,D3
        JSR     (A5)
        MOVE.L  D0,(A2)
        MOVE.L  D1,4(A2)
        ADDQ.L  #1,D7
.jn:    CMP.L   rt_mat_v+rt_mat__C,D7
        BLT     .j
        ADDQ.L  #1,D6
.in:    CMP.L   rt_mat_v+rt_mat__R,D6
        BLT     .i
        MOVEM.L (SP)+,D6-D7/A2/A5
        RTS

; view 0 = view 1 * view 2 (two 1-D arrays: column times row, the outer product); else 77
rt_mat_mul:
        MOVEM.L D4-D7/A2-A4,-(SP)
        LEA     rt_mat_v,A0
        CMP.L   #1,rt_mat__VS+rt_mat__D(A0)
        BNE     .chk
        CMP.L   #1,2*rt_mat__VS+rt_mat__D(A0)
        BNE     .chk
        LEA     rt_mat__VS(A0),A1           ; view 1 as column: rows <-> columns, steps too
        MOVE.L  rt_mat__R(A1),D0
        MOVE.L  rt_mat__C(A1),rt_mat__R(A1)
        MOVE.L  D0,rt_mat__C(A1)
        MOVE.L  rt_mat__RS(A1),D0
        MOVE.L  rt_mat__CS(A1),rt_mat__RS(A1)
        MOVE.L  D0,rt_mat__CS(A1)
.chk:   MOVE.L  rt_mat__VS+rt_mat__C(A0),D0       ; cols(b) = rows(c), rows(a) = rows(b), cols(a) = cols(c)
        CMP.L   2*rt_mat__VS+rt_mat__R(A0),D0
        BNE     .ord
        MOVE.L  rt_mat__R(A0),D0
        CMP.L   rt_mat__VS+rt_mat__R(A0),D0
        BNE     .ord
        MOVE.L  rt_mat__C(A0),D0
        CMP.L   2*rt_mat__VS+rt_mat__C(A0),D0
        BNE     .ord
        MOVE.L  rt_mat__R(A0),D0              ; work: rows(a) * cols(a) * 8
        MOVE.L  rt_mat__C(A0),D1
        JSR     rt_imul
        LSL.L   #3,D0
        BSR     rt_mat__work
        MOVE.L  A0,A3                   ; A3 = next result
        MOVE.L  A0,A2                   ; A2 = start
        MOVEQ   #0,D6
        BRA     .in
.i:     MOVEQ   #0,D7
        BRA     .jn
.j:     MOVEQ   #0,D4                   ; s = 0
        MOVEQ   #0,D5
        SUB.L   A4,A4                   ; k = 0
        BRA     .kn
.k:     MOVEQ   #1,D0                   ; b(i,k)
        MOVE.L  D6,D1
        MOVE.L  A4,D2
        BSR     rt_mat_get
        MOVEM.L D0-D1,-(SP)
        MOVEQ   #2,D0                   ; c(k,j)
        MOVE.L  A4,D1
        MOVE.L  D7,D2
        BSR     rt_mat_get
        MOVE.L  D0,D2
        MOVE.L  D1,D3
        MOVEM.L (SP)+,D0-D1
        JSR     fp_mul
        MOVE.L  D0,D2
        MOVE.L  D1,D3
        MOVE.L  D4,D0
        MOVE.L  D5,D1
        JSR     fp_add
        MOVE.L  D0,D4
        MOVE.L  D1,D5
        ADDQ.L  #1,A4
.kn:    CMP.L   rt_mat_v+rt_mat__VS+rt_mat__C,A4
        BLT     .k
        MOVE.L  D4,(A3)+
        MOVE.L  D5,(A3)+
        ADDQ.L  #1,D7
.jn:    CMP.L   rt_mat_v+rt_mat__C,D7
        BLT     .j
        ADDQ.L  #1,D6
.in:    CMP.L   rt_mat_v+rt_mat__R,D6
        BLT     .i
        MOVE.L  A2,A3                   ; copy into view 0
        MOVEQ   #0,D6
        BRA     .cin
.ci:    MOVEQ   #0,D7
        BRA     .cjn
.cj:    LEA     rt_mat_v,A0
        MOVE.L  D6,D0
        MOVE.L  D7,D1
        BSR     rt_mat_el
        MOVE.L  (A3)+,(A1)
        MOVE.L  (A3)+,4(A1)
        ADDQ.L  #1,D7
.cjn:   CMP.L   rt_mat_v+rt_mat__C,D7
        BLT     .cj
        ADDQ.L  #1,D6
.cin:   CMP.L   rt_mat_v+rt_mat__R,D6
        BLT     .ci
        MOVEM.L (SP)+,D4-D7/A2-A4
        RTS
.ord:   MOVEM.L (SP)+,D4-D7/A2-A4
        MOVEQ   #77,D0
        JMP     rt_error

; D0 = view number, D1 = k -> D0:D1 = element k of a vector view (row: (0,k), column: (k,0))
rt_mat__vec:
        MOVE.L  D0,-(SP)
        BSR     rt_mat__vp
        MOVE.L  (SP)+,D0
        MOVEQ   #0,D2
        CMP.L   #1,rt_mat__R(A0)
        BNE     .col
        EXG     D1,D2
.col:   BRA     rt_mat_get

; D0 = view number -> D0 = number of elements if the view is a vector, else -1
rt_mat__veclen:
        BSR     rt_mat__vp
        CMP.L   #1,rt_mat__R(A0)
        BEQ     .v
        CMP.L   #1,rt_mat__C(A0)
        BEQ     .v
        MOVEQ   #-1,D0
        RTS
.v:     MOVE.L  rt_mat__R(A0),D0
        MOVE.L  rt_mat__C(A0),D1
        JMP     rt_imul

; -> D0:D1 = view 0 * view 1 (two vectors of the same length, else 77)
rt_mat_dot:
        MOVEM.L D4-D7,-(SP)
        MOVEQ   #0,D0
        BSR     rt_mat__veclen
        MOVE.L  D0,D6                   ; D6 = n
        BMI     .ord
        MOVEQ   #1,D0
        BSR     rt_mat__veclen
        TST.L   D0
        BMI     .ord
        CMP.L   D6,D0
        BNE     .ord
        MOVEQ   #0,D4
        MOVEQ   #0,D5
        MOVEQ   #0,D7
        BRA     .kn
.k:     MOVEQ   #0,D0
        MOVE.L  D7,D1
        BSR     rt_mat__vec
        MOVEM.L D0-D1,-(SP)
        MOVEQ   #1,D0
        MOVE.L  D7,D1
        BSR     rt_mat__vec
        MOVE.L  D0,D2
        MOVE.L  D1,D3
        MOVEM.L (SP)+,D0-D1
        JSR     fp_mul
        MOVE.L  D0,D2
        MOVE.L  D1,D3
        MOVE.L  D4,D0
        MOVE.L  D5,D1
        JSR     fp_add
        MOVE.L  D0,D4
        MOVE.L  D1,D5
        ADDQ.L  #1,D7
.kn:    CMP.L   D6,D7
        BLT     .k
        MOVE.L  D4,D0
        MOVE.L  D5,D1
        MOVEM.L (SP)+,D4-D7
        RTS
.ord:   MOVEM.L (SP)+,D4-D7
        MOVEQ   #77,D0
        JSR     rt_error
        MOVEQ   #0,D0
        MOVEQ   #0,D1
        RTS

; -> D0:D1 = view 0 * view 1 * view 2: x = sum over i of a(0,i) * (sum over j of b(i,j) * c(0,j));
; a and c row vectors of the lengths rows(b) and cols(b), else 79
rt_mat_triple:
        MOVEM.L D4-D7,-(SP)
        LEA     rt_mat_v,A0
        CMP.L   #1,rt_mat__R(A0)
        BNE     .err
        CMP.L   #1,2*rt_mat__VS+rt_mat__R(A0)
        BNE     .err
        MOVE.L  rt_mat__C(A0),D0
        CMP.L   rt_mat__VS+rt_mat__R(A0),D0
        BNE     .err
        MOVE.L  2*rt_mat__VS+rt_mat__C(A0),D0
        CMP.L   rt_mat__VS+rt_mat__C(A0),D0
        BNE     .err
        CLR.L   rt_mat__x
        CLR.L   rt_mat__x+4
        MOVEQ   #0,D6
        BRA     .in
.i:     MOVEQ   #0,D4                   ; t = 0
        MOVEQ   #0,D5
        MOVEQ   #0,D7
        BRA     .jn
.j:     MOVEQ   #1,D0                   ; b(i,j)
        MOVE.L  D6,D1
        MOVE.L  D7,D2
        BSR     rt_mat_get
        MOVEM.L D0-D1,-(SP)
        MOVEQ   #2,D0                   ; c(0,j)
        MOVEQ   #0,D1
        MOVE.L  D7,D2
        BSR     rt_mat_get
        MOVE.L  D0,D2
        MOVE.L  D1,D3
        MOVEM.L (SP)+,D0-D1
        JSR     fp_mul
        MOVE.L  D0,D2
        MOVE.L  D1,D3
        MOVE.L  D4,D0
        MOVE.L  D5,D1
        JSR     fp_add
        MOVE.L  D0,D4
        MOVE.L  D1,D5
        ADDQ.L  #1,D7
.jn:    CMP.L   rt_mat_v+rt_mat__VS+rt_mat__C,D7
        BLT     .j
        MOVEQ   #0,D0                   ; x += a(0,i) * t
        MOVEQ   #0,D1
        MOVE.L  D6,D2
        BSR     rt_mat_get
        MOVE.L  D4,D2
        MOVE.L  D5,D3
        JSR     fp_mul
        MOVE.L  D0,D2
        MOVE.L  D1,D3
        MOVE.L  rt_mat__x,D0
        MOVE.L  rt_mat__x+4,D1
        JSR     fp_add
        MOVE.L  D0,rt_mat__x
        MOVE.L  D1,rt_mat__x+4
        ADDQ.L  #1,D6
.in:    CMP.L   rt_mat_v+rt_mat__VS+rt_mat__R,D6
        BLT     .i
        MOVE.L  rt_mat__x,D0
        MOVE.L  rt_mat__x+4,D1
        MOVEM.L (SP)+,D4-D7
        RTS
.err:   MOVEM.L (SP)+,D4-D7
        MOVEQ   #79,D0
        JSR     rt_error
        MOVEQ   #0,D0
        MOVEQ   #0,D1
        RTS

; line l, position k of view 0 (D0 = l, D1 = k) -> A1 = address; rows, or columns if rt_mat__f
rt_mat__line:
        TST.L   rt_mat__f
        BEQ     .r
        EXG     D0,D1
.r:     LEA     rt_mat_v,A0
        BRA     rt_mat_el

; D0 = 0: every row, 1: every column of view 0 divided by the root of its sum of squares; a zero
; line: error 0 (the lines before it stay normed)
rt_mat_norm:
        MOVEM.L D4-D7,-(SP)
        MOVE.L  D0,rt_mat__f
        MOVEQ   #0,D6                   ; D6 = line
        BRA     .ln
.l:     MOVEQ   #0,D4                   ; s = 0
        MOVEQ   #0,D5
        MOVEQ   #0,D7
        BRA     .kn
.k:     MOVE.L  D6,D0
        MOVE.L  D7,D1
        BSR     rt_mat__line
        MOVE.L  (A1),D0
        MOVE.L  4(A1),D1
        MOVE.L  D0,D2
        MOVE.L  D1,D3
        JSR     fp_mul
        MOVE.L  D0,D2
        MOVE.L  D1,D3
        MOVE.L  D4,D0
        MOVE.L  D5,D1
        JSR     fp_add
        MOVE.L  D0,D4
        MOVE.L  D1,D5
        ADDQ.L  #1,D7
.kn:    BSR     .len
        CMP.L   D0,D7
        BLT     .k
        MOVE.L  D4,D0                   ; s = 0 (either sign): error 0
        AND.L   #$7FFFFFFF,D0
        OR.L    D5,D0
        BNE     .root
        MOVEM.L (SP)+,D4-D7
        MOVEQ   #0,D0
        JMP     rt_error
.root:  MOVE.L  D4,D0
        MOVE.L  D5,D1
        JSR     fp_sqrt
        MOVE.L  D0,rt_mat__x
        MOVE.L  D1,rt_mat__x+4
        MOVEQ   #0,D7
        BRA     .dn
.d:     MOVE.L  D6,D0
        MOVE.L  D7,D1
        BSR     rt_mat__line
        MOVE.L  A1,-(SP)
        MOVE.L  (A1),D0
        MOVE.L  4(A1),D1
        MOVE.L  rt_mat__x,D2
        MOVE.L  rt_mat__x+4,D3
        JSR     fp_div
        MOVE.L  (SP)+,A1
        MOVE.L  D0,(A1)
        MOVE.L  D1,4(A1)
        ADDQ.L  #1,D7
.dn:    BSR     .len
        CMP.L   D0,D7
        BLT     .d
        ADDQ.L  #1,D6
.ln:    MOVE.L  rt_mat_v+rt_mat__R,D0         ; lines: rows, or columns
        TST.L   rt_mat__f
        BEQ     .lr
        MOVE.L  rt_mat_v+rt_mat__C,D0
.lr:    CMP.L   D0,D6
        BLT     .l
        MOVEM.L (SP)+,D4-D7
        RTS
.len:   MOVE.L  rt_mat_v+rt_mat__C,D0         ; length of a line: columns, or rows
        TST.L   rt_mat__f
        BEQ     .lc
        MOVE.L  rt_mat_v+rt_mat__R,D0
.lc:    RTS

; ---- square work copies (DET, RANG, INV) --------------------------------------------------------
; Work matrix w: n x n, row-major, from rt_mat__wbuf; rt_mat__n2 = n, rt_mat__rb = 8 * n.

; D0 = i, D1 = j -> A1 = address of w(i,j) (A0 = w kept)
rt_mat__w:
        MULU    rt_mat__rb+2,D0
        LSL.L   #3,D1
        ADD.L   D1,D0
        LEA     0(A0,D0.L),A1
        RTS

; view 0 (square, checked by the caller) into w at A0 = start; -> rt_mat__tol = n * 2^-52 * max |a|
rt_mat__copy:
        MOVEM.L D4-D7/A2-A3,-(SP)
        MOVE.L  A0,A3                   ; A3 = next of w
        MOVEQ   #0,D4                   ; D4:D5 = max |a|
        MOVEQ   #0,D5
        MOVEQ   #0,D6
        BRA     .in
.i:     MOVEQ   #0,D7
        BRA     .jn
.j:     MOVEQ   #0,D0
        MOVE.L  D6,D1
        MOVE.L  D7,D2
        BSR     rt_mat_get
        MOVE.L  D0,(A3)+
        MOVE.L  D1,(A3)+
        AND.L   #$7FFFFFFF,D0           ; |v| > max: unsigned 64-bit comparison
        CMP.L   D4,D0
        BHI     .new
        BCS     .nx
        CMP.L   D5,D1
        BLS     .nx
.new:   MOVE.L  D0,D4
        MOVE.L  D1,D5
.nx:    ADDQ.L  #1,D7
.jn:    CMP.L   rt_mat__n2,D7
        BLT     .j
        ADDQ.L  #1,D6
.in:    CMP.L   rt_mat__n2,D6
        BLT     .i
        MOVE.L  D4,D0                   ; tol = n * (max * 2^-52)
        MOVE.L  D5,D1
        MOVE.L  #$3CB00000,D2
        MOVEQ   #0,D3
        JSR     fp_mul
        MOVEM.L D0-D1,-(SP)
        MOVE.L  rt_mat__n2,D0
        JSR     fp_from_i32
        MOVEM.L (SP)+,D2-D3
        JSR     fp_mul
        MOVE.L  D0,rt_mat__tol
        MOVE.L  D1,rt_mat__tol+4
        MOVEM.L (SP)+,D4-D7/A2-A3
        RTS

; view 0 must be square (82); D0 = number of n x n work matrices -> A0 = the first; sets n
rt_mat__square:
        MOVE.L  D0,D2
        LEA     rt_mat_v,A0
        MOVE.L  rt_mat__R(A0),D0
        CMP.L   rt_mat__C(A0),D0
        BNE     .no
        MOVE.L  D0,rt_mat__n2
        LSL.L   #3,D0
        MOVE.L  D0,rt_mat__rb
        MOVE.L  rt_mat__n2,D0
        MOVE.L  D0,D1
        MOVE.L  D2,-(SP)
        JSR     rt_imul
        LSL.L   #3,D0                   ; bytes of one
        MOVE.L  D0,rt_mat__wsz
        MOVE.L  (SP)+,D1
        JSR     rt_imul
        BSR     rt_mat__work
        ANDI    #$FE,CCR                ; carry clear: square
        RTS
.no:    ORI     #$01,CCR
        RTS

; A0 = w, D0 = i, D1 = j -> carry set if |w(i,j)| <= tol
rt_mat__small:
        BSR     rt_mat__w
        MOVE.L  (A1),D0
        AND.L   #$7FFFFFFF,D0
        CMP.L   rt_mat__tol,D0
        BCS     .yes
        BHI     .no
        MOVE.L  4(A1),D0
        CMP.L   rt_mat__tol+4,D0
        BLS     .yes
.no:    ANDI    #$FE,CCR
        RTS
.yes:   ORI     #$01,CCR
        RTS

; A0 = w, D0 = column k, D1 = first row -> D0 = row with the largest |w(i,k)| (the first of equal ones)
rt_mat__pivot:
        MOVEM.L D4-D7,-(SP)
        MOVE.L  D0,D7                   ; k
        MOVE.L  D1,D6                   ; i
        MOVE.L  D1,D4                   ; best row
        MOVE.L  D1,D0
        MOVE.L  D7,D1
        BSR     rt_mat__w
        MOVE.L  (A1),D2
        AND.L   #$7FFFFFFF,D2
        MOVE.L  4(A1),D3                ; D2:D3 = best |w|
.n:     ADDQ.L  #1,D6
        CMP.L   rt_mat__n2,D6
        BGE     .out
        MOVE.L  D6,D0
        MOVE.L  D7,D1
        MOVEM.L D2-D3,-(SP)
        BSR     rt_mat__w
        MOVEM.L (SP)+,D2-D3
        MOVE.L  (A1),D0
        AND.L   #$7FFFFFFF,D0
        MOVE.L  4(A1),D1
        CMP.L   D2,D0
        BHI     .new
        BCS     .n
        CMP.L   D3,D1
        BLS     .n
.new:   MOVE.L  D0,D2
        MOVE.L  D1,D3
        MOVE.L  D6,D4
        BRA     .n
.out:   MOVE.L  D4,D0
        MOVEM.L (SP)+,D4-D7
        RTS

; A0 = matrix (n x n), D0 = row a, D1 = row b: exchange
rt_mat__swap:
        MOVE.L  D1,-(SP)
        MOVEQ   #0,D1
        BSR     rt_mat__w
        MOVE.L  A1,-(SP)
        MOVE.L  4(SP),D0
        MOVEQ   #0,D1
        BSR     rt_mat__w               ; A1 = row b
        MOVE.L  (SP)+,A0                ; A0 = row a (the matrix start is no longer needed here)
        ADDQ.L  #4,SP
        MOVE.L  rt_mat__n2,D0
        ADD.L   D0,D0                   ; longs
        BRA     .n
.x:     MOVE.L  (A0),D1
        MOVE.L  (A1),(A0)+
        MOVE.L  D1,(A1)+
.n:     SUBQ.L  #1,D0
        BPL     .x
        RTS

; A0 = w, D0 = row r, D1 = column k: rows i > r: f = w(i,k) / w(r,k); w(i,j) -= f * w(r,j) for
; j > k; w(i,k) = 0
rt_mat__elim:
        MOVEM.L D4-D7/A2-A4,-(SP)
        MOVE.L  A0,A4                   ; A4 = w
        MOVE.L  D0,A2                   ; A2 = r
        MOVE.L  D1,A3                   ; A3 = k
        MOVE.L  D0,D6                   ; D6 = i
        BRA     .in
.i:     MOVE.L  A4,A0                   ; f = w(i,k) / w(r,k)
        MOVE.L  A2,D0
        MOVE.L  A3,D1
        BSR     rt_mat__w
        MOVE.L  (A1),D2
        MOVE.L  4(A1),D3
        MOVE.L  D6,D0
        MOVE.L  A3,D1
        BSR     rt_mat__w
        MOVE.L  (A1),D0
        MOVE.L  4(A1),D1
        JSR     fp_div
        MOVE.L  D0,D4
        MOVE.L  D1,D5                   ; D4:D5 = f
        MOVE.L  A3,D7
        BRA     .jn
.j:     MOVE.L  A4,A0                   ; f * w(r,j)
        MOVE.L  A2,D0
        MOVE.L  D7,D1
        BSR     rt_mat__w
        MOVE.L  (A1),D2
        MOVE.L  4(A1),D3
        MOVE.L  D4,D0
        MOVE.L  D5,D1
        JSR     fp_mul
        MOVEM.L D0-D1,-(SP)
        MOVE.L  A4,A0                   ; w(i,j) - that
        MOVE.L  D6,D0
        MOVE.L  D7,D1
        BSR     rt_mat__w
        MOVE.L  A1,-(SP)
        MOVE.L  (A1),D0
        MOVE.L  4(A1),D1
        MOVEM.L 4(SP),D2-D3
        JSR     fp_sub
        MOVE.L  (SP)+,A1
        ADDQ.L  #8,SP
        MOVE.L  D0,(A1)
        MOVE.L  D1,4(A1)
.jn:    ADDQ.L  #1,D7
        CMP.L   rt_mat__n2,D7
        BLT     .j
        MOVE.L  A4,A0                   ; w(i,k) = 0
        MOVE.L  D6,D0
        MOVE.L  A3,D1
        BSR     rt_mat__w
        CLR.L   (A1)
        CLR.L   4(A1)
.in:    ADDQ.L  #1,D6
        CMP.L   rt_mat__n2,D6
        BLT     .i
        MOVEM.L (SP)+,D4-D7/A2-A4
        RTS

; D0 = 0 MAT DET, 1 MAT QDET of view 0 -> D0:D1 (not square: 82)
rt_mat_det:
        MOVEM.L D4-D7/A2,-(SP)
        MOVE.L  D0,D7                   ; D7 = quick
        MOVEQ   #1,D0
        BSR     rt_mat__square
        BCC     .sq
        MOVEM.L (SP)+,D4-D7/A2
        MOVEQ   #82,D0
        JSR     rt_error
        MOVEQ   #0,D0
        MOVEQ   #0,D1
        RTS
.sq:    MOVE.L  A0,A2                   ; A2 = w
        BSR     rt_mat__copy
        MOVE.L  #$3FF00000,D4           ; det = 1
        MOVEQ   #0,D5
        CLR.L   rt_mat__f               ; sign
        MOVEQ   #0,D6                   ; k
        BRA     .kn
.k:     TST.L   D7
        BEQ     .big
        MOVE.L  D6,D3                   ; QDET: the first row p >= k with w(p,k) <> 0
.q:     CMP.L   rt_mat__n2,D3
        BGE     .zero
        MOVE.L  A2,A0
        MOVE.L  D3,D0
        MOVE.L  D6,D1
        MOVE.L  D3,-(SP)
        BSR     rt_mat__w
        MOVE.L  (SP)+,D3
        MOVE.L  (A1),D0
        AND.L   #$7FFFFFFF,D0
        OR.L    4(A1),D0
        BNE     .piv
        ADDQ.L  #1,D3
        BRA     .q
.big:   MOVE.L  A2,A0                   ; DET: the largest pivot, up to tol it is 0
        MOVE.L  D6,D0
        MOVE.L  D6,D1
        BSR     rt_mat__pivot
        MOVE.L  D0,D3
        MOVE.L  A2,A0
        MOVE.L  D6,D1
        MOVE.L  D3,-(SP)
        BSR     rt_mat__small
        MOVEM.L (SP)+,D3                ; MOVEM keeps the flags
        BCS     .zero
.piv:   CMP.L   D6,D3
        BEQ     .ns
        MOVE.L  A2,A0
        MOVE.L  D3,D0
        MOVE.L  D6,D1
        BSR     rt_mat__swap
        EORI.L  #1,rt_mat__f
.ns:    MOVE.L  A2,A0                   ; det = det * w(k,k)
        MOVE.L  D6,D0
        MOVE.L  D6,D1
        BSR     rt_mat__w
        MOVE.L  (A1),D2
        MOVE.L  4(A1),D3
        MOVE.L  D4,D0
        MOVE.L  D5,D1
        JSR     fp_mul
        MOVE.L  D0,D4
        MOVE.L  D1,D5
        MOVE.L  A2,A0
        MOVE.L  D6,D0
        MOVE.L  D6,D1
        BSR     rt_mat__elim
        ADDQ.L  #1,D6
.kn:    CMP.L   rt_mat__n2,D6
        BLT     .k
        BRA     .sign
.zero:  MOVEQ   #0,D4
        MOVEQ   #0,D5
.sign:  TST.L   rt_mat__f
        BEQ     .out
        MOVE.L  D4,D0
        AND.L   #$7FFFFFFF,D0
        OR.L    D5,D0
        BEQ     .out
        BCHG    #31,D4
.out:   MOVE.L  D4,D0
        MOVE.L  D5,D1
        MOVEM.L (SP)+,D4-D7/A2
        RTS

; MAT RANG of view 0 -> D0:D1 (not square: 82)
rt_mat_rank:
        MOVEM.L D4-D7/A2,-(SP)
        MOVEQ   #1,D0
        BSR     rt_mat__square
        BCC     .sq
        MOVEM.L (SP)+,D4-D7/A2
        MOVEQ   #82,D0
        JSR     rt_error
        MOVEQ   #0,D0
        MOVEQ   #0,D1
        RTS
.sq:    MOVE.L  A0,A2
        BSR     rt_mat__copy
        MOVEQ   #0,D4                   ; r
        MOVEQ   #0,D6                   ; k
        BRA     .kn
.k:     MOVE.L  A2,A0
        MOVE.L  D6,D0
        MOVE.L  D4,D1
        BSR     rt_mat__pivot
        MOVE.L  D0,D5                   ; p
        MOVE.L  A2,A0
        MOVE.L  D6,D1
        BSR     rt_mat__small
        BCS     .next
        CMP.L   D4,D5
        BEQ     .ns
        MOVE.L  A2,A0
        MOVE.L  D5,D0
        MOVE.L  D4,D1
        BSR     rt_mat__swap
.ns:    MOVE.L  A2,A0
        MOVE.L  D4,D0
        MOVE.L  D6,D1
        BSR     rt_mat__elim
        ADDQ.L  #1,D4
.next:  ADDQ.L  #1,D6
.kn:    CMP.L   rt_mat__n2,D6
        BGE     .out
        CMP.L   rt_mat__n2,D4
        BLT     .k
.out:   MOVE.L  D4,D0
        JSR     fp_from_i32
        MOVEM.L (SP)+,D4-D7/A2
        RTS

; MAT INV view 0 = inverse of view 1: Gauss-Jordan; not square 82, target of another size 83,
; singular 0 (the target stays)
rt_mat_inv:
        MOVEM.L D4-D7/A2-A3,-(SP)
        LEA     rt_mat_v,A0             ; view 1 to view 0's place for the checks: exchange them
        MOVEQ   #5,D0                   ; 6 longs
.x:     MOVE.L  (A0),D1
        MOVE.L  rt_mat__VS(A0),(A0)+
        MOVE.L  D1,rt_mat__VS-4(A0)
        DBRA    D0,.x
        MOVEQ   #2,D0                   ; view 0 = a now, view 1 = b
        BSR     rt_mat__square
        BCC     .sq
        MOVEQ   #82,D0
        BRA     .err
.sq:    MOVE.L  rt_mat__n2,D0
        CMP.L   rt_mat_v+rt_mat__VS+rt_mat__R,D0
        BNE     .size
        CMP.L   rt_mat_v+rt_mat__VS+rt_mat__C,D0
        BEQ     .ok
.size:  MOVEQ   #83,D0
        BRA     .err
.ok:    MOVE.L  A0,A2                   ; A2 = w
        MOVE.L  A0,A3
        ADD.L   rt_mat__wsz,A3          ; A3 = v
        BSR     rt_mat__copy
        MOVEQ   #0,D6                   ; v = unit matrix
        BRA     .uin
.ui:    MOVEQ   #0,D7
        BRA     .ujn
.uj:    MOVE.L  A3,A0
        MOVE.L  D6,D0
        MOVE.L  D7,D1
        BSR     rt_mat__w
        CLR.L   (A1)
        CLR.L   4(A1)
        CMP.L   D6,D7
        BNE     .u0
        MOVE.L  #$3FF00000,(A1)
.u0:    ADDQ.L  #1,D7
.ujn:   CMP.L   rt_mat__n2,D7
        BLT     .uj
        ADDQ.L  #1,D6
.uin:   CMP.L   rt_mat__n2,D6
        BLT     .ui
        MOVEQ   #0,D6                   ; D6 = k
        BRA     .kn
.k:     MOVE.L  A2,A0
        MOVE.L  D6,D0
        MOVE.L  D6,D1
        BSR     rt_mat__pivot
        MOVE.L  D0,D5                   ; p
        MOVE.L  A2,A0
        MOVE.L  D6,D1
        BSR     rt_mat__small
        BCC     .reg
        MOVEQ   #0,D0                   ; singular: divide by zero
        BRA     .err
.reg:   CMP.L   D6,D5
        BEQ     .ns
        MOVE.L  A2,A0
        MOVE.L  D5,D0
        MOVE.L  D6,D1
        BSR     rt_mat__swap
        MOVE.L  A3,A0
        MOVE.L  D5,D0
        MOVE.L  D6,D1
        BSR     rt_mat__swap
.ns:    MOVE.L  A2,A0                   ; piv = w(k,k)
        MOVE.L  D6,D0
        MOVE.L  D6,D1
        BSR     rt_mat__w
        MOVE.L  (A1),rt_mat__x
        MOVE.L  4(A1),rt_mat__x+4
        MOVE.L  D6,D7                   ; w(k,j) /= piv for j > k
        BRA     .wdn
.wd:    MOVE.L  A2,A0
        BSR     .div
.wdn:   ADDQ.L  #1,D7
        CMP.L   rt_mat__n2,D7
        BLT     .wd
        MOVEQ   #0,D7                   ; v(k,j) /= piv for all j
        BRA     .vdn
.vd:    MOVE.L  A3,A0
        BSR     .div
        ADDQ.L  #1,D7
.vdn:   CMP.L   rt_mat__n2,D7
        BLT     .vd
        MOVE.L  A2,A0                   ; w(k,k) = 1
        MOVE.L  D6,D0
        MOVE.L  D6,D1
        BSR     rt_mat__w
        MOVE.L  #$3FF00000,(A1)
        CLR.L   4(A1)
        MOVEQ   #0,D4                   ; D4 = i
        BRA     .in
.i:     CMP.L   D6,D4
        BEQ     .inx
        MOVE.L  A2,A0                   ; f = w(i,k); 0: nothing to do
        MOVE.L  D4,D0
        MOVE.L  D6,D1
        BSR     rt_mat__w
        MOVE.L  (A1),D0
        MOVE.L  4(A1),D1
        MOVE.L  D0,D2
        AND.L   #$7FFFFFFF,D2
        OR.L    D1,D2
        BEQ     .inx
        MOVE.L  D0,rt_mat__y
        MOVE.L  D1,rt_mat__y+4
        MOVE.L  D6,D7                   ; w(i,j) -= f * w(k,j) for j > k
        BRA     .wsn
.ws:    MOVE.L  A2,A0
        BSR     .sub
.wsn:   ADDQ.L  #1,D7
        CMP.L   rt_mat__n2,D7
        BLT     .ws
        MOVEQ   #0,D7                   ; v(i,j) -= f * v(k,j) for all j
        BRA     .vsn
.vs:    MOVE.L  A3,A0
        BSR     .sub
        ADDQ.L  #1,D7
.vsn:   CMP.L   rt_mat__n2,D7
        BLT     .vs
        MOVE.L  A2,A0                   ; w(i,k) = 0
        MOVE.L  D4,D0
        MOVE.L  D6,D1
        BSR     rt_mat__w
        CLR.L   (A1)
        CLR.L   4(A1)
.inx:   ADDQ.L  #1,D4
.in:    CMP.L   rt_mat__n2,D4
        BLT     .i
        ADDQ.L  #1,D6
.kn:    CMP.L   rt_mat__n2,D6
        BLT     .k
        MOVEQ   #0,D6                   ; b = v (b is view 1 now)
        BRA     .cin
.ci:    MOVEQ   #0,D7
        BRA     .cjn
.cj:    MOVE.L  A3,A0
        MOVE.L  D6,D0
        MOVE.L  D7,D1
        BSR     rt_mat__w
        MOVE.L  A1,-(SP)
        LEA     rt_mat_v+rt_mat__VS,A0
        MOVE.L  D6,D0
        MOVE.L  D7,D1
        BSR     rt_mat_el
        MOVE.L  (SP)+,A0
        MOVE.L  (A0)+,(A1)+
        MOVE.L  (A0),(A1)
        ADDQ.L  #1,D7
.cjn:   CMP.L   rt_mat__n2,D7
        BLT     .cj
        ADDQ.L  #1,D6
.cin:   CMP.L   rt_mat__n2,D6
        BLT     .ci
        MOVEM.L (SP)+,D4-D7/A2-A3
        RTS
.err:   MOVEM.L (SP)+,D4-D7/A2-A3
        JMP     rt_error
; A0 = matrix: (k, D7) = (k, D7) / piv
.div:   MOVE.L  D6,D0
        MOVE.L  D7,D1
        BSR     rt_mat__w
        MOVE.L  A1,-(SP)
        MOVE.L  (A1),D0
        MOVE.L  4(A1),D1
        MOVE.L  rt_mat__x,D2
        MOVE.L  rt_mat__x+4,D3
        JSR     fp_div
        MOVE.L  (SP)+,A1
        MOVE.L  D0,(A1)
        MOVE.L  D1,4(A1)
        RTS
; A0 = matrix: (i, D7) = (i, D7) - f * (k, D7)
.sub:   MOVE.L  A0,-(SP)
        MOVE.L  D6,D0
        MOVE.L  D7,D1
        BSR     rt_mat__w
        MOVE.L  (A1),D2
        MOVE.L  4(A1),D3
        MOVE.L  rt_mat__y,D0
        MOVE.L  rt_mat__y+4,D1
        JSR     fp_mul
        MOVE.L  (SP)+,A0
        MOVEM.L D0-D1,-(SP)
        MOVE.L  D4,D0
        MOVE.L  D7,D1
        BSR     rt_mat__w
        MOVE.L  A1,-(SP)
        MOVE.L  (A1),D0
        MOVE.L  4(A1),D1
        MOVEM.L 4(SP),D2-D3
        JSR     fp_sub
        MOVE.L  (SP)+,A1
        ADDQ.L  #8,SP
        MOVE.L  D0,(A1)
        MOVE.L  D1,4(A1)
        RTS

; MAT TRANS: view 0 = transposed view 1 (view 0 must be cols x rows of view 1, else 83); the same
; array (in place) works through the work copy
rt_mat_trans:
        MOVEM.L D6-D7/A2-A3,-(SP)
        LEA     rt_mat_v,A0
        MOVE.L  rt_mat__R(A0),D0
        CMP.L   rt_mat__VS+rt_mat__C(A0),D0
        BNE     .size
        MOVE.L  rt_mat__C(A0),D0
        CMP.L   rt_mat__VS+rt_mat__R(A0),D0
        BNE     .size
        MOVE.L  rt_mat__VS+rt_mat__R(A0),D0
        MOVE.L  rt_mat__VS+rt_mat__C(A0),D1
        JSR     rt_imul
        LSL.L   #3,D0
        BSR     rt_mat__work
        MOVE.L  A0,A2
        MOVE.L  A0,A3                   ; t = b, row by row
        MOVEQ   #0,D6
        BRA     .in
.i:     MOVEQ   #0,D7
        BRA     .jn
.j:     MOVEQ   #1,D0
        MOVE.L  D6,D1
        MOVE.L  D7,D2
        BSR     rt_mat_get
        MOVE.L  D0,(A3)+
        MOVE.L  D1,(A3)+
        ADDQ.L  #1,D7
.jn:    CMP.L   rt_mat_v+rt_mat__VS+rt_mat__C,D7
        BLT     .j
        ADDQ.L  #1,D6
.in:    CMP.L   rt_mat_v+rt_mat__VS+rt_mat__R,D6
        BLT     .i
        MOVE.L  A2,A3                   ; a(j,i) = t(i,j)
        MOVEQ   #0,D6
        BRA     .tin
.ti:    MOVEQ   #0,D7
        BRA     .tjn
.tj:    LEA     rt_mat_v,A0
        MOVE.L  D7,D0
        MOVE.L  D6,D1
        BSR     rt_mat_el
        MOVE.L  (A3)+,(A1)
        MOVE.L  (A3)+,4(A1)
        ADDQ.L  #1,D7
.tjn:   CMP.L   rt_mat_v+rt_mat__VS+rt_mat__C,D7
        BLT     .tj
        ADDQ.L  #1,D6
.tin:   CMP.L   rt_mat_v+rt_mat__VS+rt_mat__R,D6
        BLT     .ti
        MOVEM.L (SP)+,D6-D7/A2-A3
        RTS
.size:  MOVEM.L (SP)+,D6-D7/A2-A3
        MOVEQ   #83,D0
        JMP     rt_error

; MAT CPY / XCPY: D0 = h, D1 = w (< 0: as many as fit), D2 = 1: transposed. View 0 = destination,
; view 1 = source, both already moved to their offsets (rt_mat_at).
rt_mat_cpy:
        MOVEM.L D4-D7/A2-A3,-(SP)
        LEA     rt_mat_v,A0
        MOVE.L  rt_mat__VS+rt_mat__R(A0),D4       ; hh = rows of the source
        MOVE.L  rt_mat__VS+rt_mat__C(A0),D5       ; ww = columns of the source
        MOVE.L  rt_mat__R(A0),D6              ; destination limits: rows / columns (exchanged for XCPY)
        MOVE.L  rt_mat__C(A0),D7
        TST.L   D2
        BEQ     .nt
        EXG     D6,D7
.nt:    CMP.L   D6,D4
        BLE     .h1
        MOVE.L  D6,D4
.h1:    CMP.L   D7,D5
        BLE     .w1
        MOVE.L  D7,D5
.w1:    TST.L   D0
        BMI     .h2
        CMP.L   D4,D0
        BGE     .h2
        MOVE.L  D0,D4
.h2:    TST.L   D1
        BMI     .w2
        CMP.L   D5,D1
        BGE     .w2
        MOVE.L  D1,D5
.w2:    TST.L   D4
        BLE     .out
        TST.L   D5
        BLE     .out
        MOVE.L  D2,rt_mat__f
        MOVE.L  D4,D0                   ; work: hh * ww * 8
        MOVE.L  D5,D1
        JSR     rt_imul
        LSL.L   #3,D0
        BSR     rt_mat__work
        MOVE.L  A0,A2
        MOVE.L  A0,A3
        MOVEQ   #0,D6                   ; t(r,c) = s(r,c)
        BRA     .rn
.r:     MOVEQ   #0,D7
        BRA     .cn
.c:     MOVEQ   #1,D0
        MOVE.L  D6,D1
        MOVE.L  D7,D2
        BSR     rt_mat_get
        MOVE.L  D0,(A3)+
        MOVE.L  D1,(A3)+
        ADDQ.L  #1,D7
.cn:    CMP.L   D5,D7
        BLT     .c
        ADDQ.L  #1,D6
.rn:    CMP.L   D4,D6
        BLT     .r
        MOVE.L  A2,A3                   ; d(r,c) = t(r,c), or d(c,r)
        MOVEQ   #0,D6
        BRA     .drn
.dr:    MOVEQ   #0,D7
        BRA     .dcn
.dc:    LEA     rt_mat_v,A0
        MOVE.L  D6,D0
        MOVE.L  D7,D1
        TST.L   rt_mat__f
        BEQ     .dn
        EXG     D0,D1
.dn:    BSR     rt_mat_el
        MOVE.L  (A3)+,(A1)
        MOVE.L  (A3)+,4(A1)
        ADDQ.L  #1,D7
.dcn:   CMP.L   D5,D7
        BLT     .dc
        ADDQ.L  #1,D6
.drn:   CMP.L   D4,D6
        BLT     .dr
.out:   MOVEM.L (SP)+,D4-D7/A2-A3
        RTS

;! section bss
rt_mat_v:
        DS.B    4*rt_mat__VS
rt_mat__b0:                             ; 1 for MAT BASE 0 (the default MAT BASE 1 is 0 here)
        DS.L    1
rt_mat__x:
        DS.L    2
rt_mat__y:
        DS.L    2
rt_mat__tol:
        DS.L    2
rt_mat__f:
        DS.L    1
rt_mat__n2:
        DS.L    1
rt_mat__rb:
        DS.L    1
rt_mat__wsz:
        DS.L    1
rt_mat__wbuf:
        DS.L    1
rt_mat__wcap:
        DS.L    1
