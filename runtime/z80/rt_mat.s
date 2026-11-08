;! module    rt_mat
;! provides  rt_mat_v rt_mat_set_base rt_mat_view rt_mat_at rt_mat_limit rt_mat_fill rt_mat_one rt_mat_el
;! provides  rt_mat_addsub rt_mat_scalar rt_mat_mul rt_mat_dot rt_mat_triple rt_mat_norm rt_mat_det rt_mat_rank rt_mat_inv
;! provides  rt_mat_trans rt_mat_cpy
;! requires  rt_error rt_argp rt_arr_base rt_arr_work fp_add fp_sub fp_mul fp_div fp_sqrt fp_cmp fp_from_i32
;! group     matrix
;! clobbers  AF BC DE HL
;! doc       MAT commands (D23): the operations of runtime/ref/rt_mat.c in the same order, on views.

; A view (12 bytes, three of them in rt_mat_v): +0 address of element (0,0), +2 rows, +4 columns,
; +6 row step, +8 column step (bytes), +10 dimensions of the array. Element (i,j) is at
; p + i * rs + j * cs. The program builds the views (rt_mat_view, rt_mat_at, rt_mat_limit), then
; calls the operation; operands are view 0, 1, 2 in the order of runtime/ref/rt_mat.c. Work
; storage lies above the arrays (rt_arr_work).

rt_mat__VS EQU 12
rt_mat__P0 EQU rt_mat_v
rt_mat__R0 EQU rt_mat_v+2
rt_mat__C0 EQU rt_mat_v+4
rt_mat__P1 EQU rt_mat_v+12
rt_mat__R1 EQU rt_mat_v+14
rt_mat__C1 EQU rt_mat_v+16
rt_mat__P2 EQU rt_mat_v+24
rt_mat__R2 EQU rt_mat_v+26
rt_mat__C2 EQU rt_mat_v+28

; ---- helpers -------------------------------------------------------------------------------------

; HL * DE -> HL (low 16 bits), CF set if the product does not fit
rt_mat__mul:
        LD      B,H
        LD      C,L
        LD      HL,0
        XOR     A
        LD      (rt_mat__ov),A
        LD      A,16
rt_mat__m1:
        ADD     HL,HL
        CALL    C,rt_mat__ovf
        SLA     E
        RL      D
        JR      NC,rt_mat__m2
        ADD     HL,BC
        CALL    C,rt_mat__ovf
rt_mat__m2:
        DEC     A
        JR      NZ,rt_mat__m1
        LD      A,(rt_mat__ov)
        RRCA
        RET
rt_mat__ovf:
        PUSH    AF
        LD      A,1
        LD      (rt_mat__ov),A
        POP     AF
        RET

; HL, DE (signed) -> CF set if HL < DE (HL changed)
rt_mat__lt:
        LD      A,H
        XOR     D
        JP      M,rt_mat__lt1
        OR      A
        SBC     HL,DE
        RET
rt_mat__lt1:
        LD      A,H
        RLA
        RET

; HL -> 32-bit value -> HL = the value as 16 bits, limited to -32768..32767
rt_mat__i16:
        INC     HL
        INC     HL
        INC     HL
        LD      A,(HL)
        DEC     HL
        LD      B,(HL)
        DEC     HL
        LD      D,(HL)
        DEC     HL
        LD      E,(HL)
        EX      DE,HL                   ; HL = low 16 bits, A:B = high 16 bits
        BIT     7,A
        JR      NZ,rt_mat__in
        OR      B
        JR      NZ,rt_mat__ip
        BIT     7,H
        RET     Z
rt_mat__ip:
        LD      HL,32767
        RET
rt_mat__in:
        AND     B
        INC     A
        JR      NZ,rt_mat__im           ; high word not -1
        BIT     7,H
        RET     NZ
rt_mat__im:
        LD      HL,-32768
        RET

; A = view number -> HL = view (keeps BC, DE)
rt_mat__vp:
        PUSH    DE
        LD      L,A
        LD      H,0
        ADD     HL,HL
        ADD     HL,HL
        LD      D,H
        LD      E,L
        ADD     HL,HL
        ADD     HL,DE                   ; 12 * n
        LD      DE,rt_mat_v
        ADD     HL,DE
        POP     DE
        RET

; HL = view, BC = i, DE = j -> HL = address of the element
rt_mat_el:
        PUSH    HL
        PUSH    BC
        LD      BC,8
        ADD     HL,BC
        LD      A,(HL)
        INC     HL
        LD      H,(HL)
        LD      L,A                     ; column step
        CALL    rt_mat__mul
        POP     BC
        EX      (SP),HL                 ; stack = j * cs, HL = view
        PUSH    HL
        LD      DE,6
        ADD     HL,DE
        LD      E,(HL)
        INC     HL
        LD      D,(HL)                  ; row step
        LD      H,B
        LD      L,C
        CALL    rt_mat__mul
        POP     DE
        EX      DE,HL                   ; HL = view, DE = i * rs
        LD      A,(HL)
        INC     HL
        LD      H,(HL)
        LD      L,A
        ADD     HL,DE
        POP     DE
        ADD     HL,DE
        RET

; A = view number, element (rt_mat__i, rt_mat__j) -> HL
rt_mat__eij:
        CALL    rt_mat__vp
        LD      BC,(rt_mat__i)
        LD      DE,(rt_mat__j)
        JR      rt_mat_el

; jump to the routine in rt_mat__fn (HL, DE, BC passed on)
rt_mat__call:
        PUSH    HL
        LD      HL,(rt_mat__fn)
        EX      (SP),HL
        RET

; HL = destination, DE = a, BC = b: fp_add, fp_sub, fp_mul, fp_div
rt_mat__add:
        JP      fp_add
rt_mat__sub:
        JP      fp_sub
rt_mat__fmul:
        JP      fp_mul

; DE = a, BC = b -> A = 1 if a > b, 0 if equal, &FF if less
rt_mat__cmp:
        LD      HL,rt_mat__cr
        CALL    fp_cmp
        LD      A,(rt_mat__cr)
        RET

; DE -> x: rt_mat__ab = |x|
rt_mat__abs:
        EX      DE,HL
        LD      DE,rt_mat__ab
        LD      BC,8
        LDIR
        LD      HL,rt_mat__ab+7
        RES     7,(HL)
        RET

; HL = destination: +0 or 1.0
rt_mat__zero:
        XOR     A
        JR      rt_mat__z0
rt_mat__one:
        LD      A,&3F
rt_mat__z0:
        LD      B,6
rt_mat__z1:
        LD      (HL),0
        INC     HL
        DJNZ    rt_mat__z1
        CP      &3F
        LD      (HL),0
        JR      NZ,rt_mat__z2
        LD      (HL),&F0
rt_mat__z2:
        INC     HL
        LD      (HL),A
        RET

; HL = source, DE = destination: 8 bytes
rt_mat__cp8:
        LD      BC,8
        LDIR
        RET

; A = error number (the result at rt_mat__res, if any, is 0)
rt_mat__err:
        PUSH    AF
        LD      HL,(rt_mat__res)
        LD      A,H
        OR      L
        CALL    NZ,rt_mat__zero
        POP     AF
        JP      rt_error

; HL = result of the operation (rt_mat__res)
rt_mat__setres:
        LD      (rt_mat__res),HL
        RET

; ---- views -----------------------------------------------------------------------------------------

; DE -> MAT BASE (0 or 1)
rt_mat_set_base:
        EX      DE,HL
        LD      A,(HL)
        INC     HL
        OR      (HL)
        INC     HL
        OR      (HL)
        INC     HL
        OR      (HL)
        LD      A,0
        JR      NZ,rt_mat__sb
        INC     A
rt_mat__sb:
        LD      (rt_mat__b0),A
        RET

; -> A = first index of a matrix: MAT BASE, at least OPTION BASE
rt_mat__lo:
        LD      A,(rt_mat__b0)
        LD      C,A
        LD      A,1
        SUB     C
        LD      C,A
        LD      A,(rt_arr_base)
        CP      C
        RET     NC
        LD      A,C
        RET

; DE = slot of a float array, BC -> view number, rt_argp -> 1: a 1-D array as column (n x 1)
rt_mat_view:
        LD      A,(BC)
        CALL    rt_mat__vp
        LD      (rt_mat__vw),HL
        LD      HL,(rt_argp)
        LD      A,(HL)
        LD      (rt_mat__col),A
        EX      DE,HL                   ; HL = slot
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        INC     HL
        LD      A,D
        OR      E
        LD      A,15                    ; array undimensioned
        JP      Z,rt_error
        LD      (rt_mat__blk),DE
        LD      A,(HL)                  ; dimensions
        CP      3
        LD      A,18                    ; wrong number of indices
        JP      NC,rt_error
        LD      A,(HL)
        LD      (rt_mat__nd),A
        LD      HL,(rt_mat__vw)
        LD      DE,10
        ADD     HL,DE
        LD      (HL),A
        CALL    rt_mat__lo
        LD      C,A
        LD      A,(rt_arr_base)
        LD      B,A
        LD      A,C
        SUB     B
        LD      (rt_mat__d2),A          ; lo - OPTION BASE (0 or 1)
        LD      A,(rt_mat__nd)          ; elements after the extents
        ADD     A,A
        ADD     A,A
        LD      E,A
        LD      D,0
        LD      HL,(rt_mat__blk)
        ADD     HL,DE
        LD      (rt_mat__el0),HL
        DEC     HL                      ; extent of the first dimension (stored last)
        DEC     HL
        DEC     HL
        LD      D,(HL)
        DEC     HL
        LD      E,(HL)
        LD      (rt_mat__x1),DE
        EX      DE,HL
        CALL    rt_mat__minus           ; HL = extent - d2, at least 0
        LD      (rt_mat__n1),HL
        LD      A,(rt_mat__nd)
        CP      1
        JR      NZ,rt_mat__v2
        LD      A,(rt_mat__d2)
        OR      A
        LD      HL,(rt_mat__el0)
        JR      Z,rt_mat__v1
        LD      DE,8
        ADD     HL,DE
rt_mat__v1:
        LD      (rt_mat__vt),HL         ; P
        LD      A,(rt_mat__col)
        OR      A
        LD      HL,1
        LD      DE,(rt_mat__n1)
        LD      BC,0
        LD      (rt_mat__vt+2),HL       ; row vector 1 x n: rs 0, cs 8
        LD      (rt_mat__vt+4),DE
        LD      (rt_mat__vt+6),BC
        LD      HL,8
        LD      (rt_mat__vt+8),HL
        JR      Z,rt_mat__vout
        LD      (rt_mat__vt+2),DE       ; column vector n x 1: rs 8, cs 0
        LD      DE,1
        LD      (rt_mat__vt+4),DE
        LD      (rt_mat__vt+6),HL
        LD      (rt_mat__vt+8),BC
        JR      rt_mat__vout
rt_mat__v2:
        LD      HL,(rt_mat__n1)
        LD      (rt_mat__vt+2),HL
        LD      HL,8
        LD      (rt_mat__vt+6),HL
        LD      HL,(rt_mat__x1)         ; column step: extent of the first dimension * 8
        ADD     HL,HL
        ADD     HL,HL
        ADD     HL,HL
        LD      (rt_mat__vt+8),HL
        LD      HL,(rt_mat__blk)        ; second dimension (stored first)
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        EX      DE,HL
        CALL    rt_mat__minus
        LD      (rt_mat__vt+4),HL
        LD      HL,(rt_mat__el0)
        LD      A,(rt_mat__d2)
        OR      A
        JR      Z,rt_mat__v3
        LD      DE,(rt_mat__vt+8)
        ADD     HL,DE
        LD      DE,8
        ADD     HL,DE
rt_mat__v3:
        LD      (rt_mat__vt),HL
rt_mat__vout:
        LD      HL,rt_mat__vt
        LD      DE,(rt_mat__vw)
        LD      BC,10
        LDIR
        RET

; HL = extent -> HL = extent - d2, at least 0
rt_mat__minus:
        LD      A,(rt_mat__d2)
        LD      E,A
        LD      D,0
        OR      A
        SBC     HL,DE
        RET     NC
        LD      HL,0
        RET

; DE -> view number | indices given << 8 | CPY << 10 | XCPY << 11, BC -> i, rt_argp -> j (BASIC
; indices): move the view to the offsets. DET/RANG: a 1-D array takes i as column offset, a 2-D one
; both or none; outside the matrix error 16. CPY/XCPY: a 1-D array takes i as row offset (XCPY:
; column), a 2-D one i and j (j missing: 0); a negative offset leaves an empty view.
rt_mat_at:
        LD      A,(DE)
        CALL    rt_mat__vp
        LD      (rt_mat__vw),HL
        INC     DE
        LD      A,(DE)
        LD      (rt_mat__fl),A
        PUSH    BC
        CALL    rt_mat__lo
        LD      E,A
        LD      D,0
        LD      (rt_mat__lw),DE
        POP     HL
        CALL    rt_mat__i16
        LD      DE,(rt_mat__lw)
        OR      A
        SBC     HL,DE
        LD      (rt_mat__oi),HL         ; i - lo
        LD      HL,(rt_argp)
        CALL    rt_mat__i16
        LD      DE,(rt_mat__lw)
        OR      A
        SBC     HL,DE
        LD      (rt_mat__oj),HL         ; j - lo
        LD      A,(rt_mat__fl)
        AND     3
        RET     Z                       ; no indices
        LD      (rt_mat__ng),A
        LD      HL,0
        LD      (rt_mat__r),HL
        LD      (rt_mat__c),HL
        LD      HL,(rt_mat__vw)
        LD      DE,10
        ADD     HL,DE
        LD      A,(HL)
        LD      (rt_mat__nd),A
        LD      A,(rt_mat__fl)
        BIT     2,A
        JR      NZ,rt_mat__acp
        LD      A,(rt_mat__nd)          ; DET/RANG
        CP      1
        JR      NZ,rt_mat__a2
        LD      HL,(rt_mat__oi)
        LD      (rt_mat__c),HL
        JR      rt_mat__ack
rt_mat__a2:
        LD      A,(rt_mat__ng)
        CP      2
        JR      NZ,rt_mat__ack
        LD      HL,(rt_mat__oi)
        LD      (rt_mat__r),HL
        LD      HL,(rt_mat__oj)
        LD      (rt_mat__c),HL
rt_mat__ack:
        LD      HL,(rt_mat__r)
        BIT     7,H
        JR      NZ,rt_mat__arng
        LD      HL,(rt_mat__c)
        BIT     7,H
        JR      NZ,rt_mat__arng
        LD      HL,(rt_mat__vw)         ; r > rows or c > columns: error 16
        INC     HL
        INC     HL
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        INC     HL
        PUSH    HL
        EX      DE,HL
        LD      DE,(rt_mat__r)
        CALL    rt_mat__lt
        POP     HL
        JR      C,rt_mat__arng
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        EX      DE,HL
        LD      DE,(rt_mat__c)
        CALL    rt_mat__lt
        JR      C,rt_mat__arng
        JR      rt_mat__amv
rt_mat__arng:
        LD      A,16                    ; array index out of range
        JP      rt_error
rt_mat__acp:                            ; CPY/XCPY
        LD      A,(rt_mat__nd)
        CP      1
        JR      NZ,rt_mat__ac2
        LD      HL,(rt_mat__oi)
        LD      A,(rt_mat__fl)
        BIT     3,A
        JR      NZ,rt_mat__acx
        LD      (rt_mat__r),HL
        JR      rt_mat__acc
rt_mat__acx:
        LD      (rt_mat__c),HL
        JR      rt_mat__acc
rt_mat__ac2:
        LD      HL,(rt_mat__oi)
        LD      (rt_mat__r),HL
        LD      A,(rt_mat__ng)
        CP      2
        JR      NZ,rt_mat__acc
        LD      HL,(rt_mat__oj)
        LD      (rt_mat__c),HL
rt_mat__acc:
        LD      HL,(rt_mat__r)
        LD      A,H
        LD      HL,(rt_mat__c)
        OR      H
        JP      P,rt_mat__amv
        LD      HL,(rt_mat__vw)         ; empty view
        INC     HL
        INC     HL
        XOR     A
        LD      B,4
rt_mat__ae:
        LD      (HL),A
        INC     HL
        DJNZ    rt_mat__ae
        RET
rt_mat__amv:                            ; rows -= r, columns -= c, p += r * rs + c * cs
        LD      HL,(rt_mat__vw)
        INC     HL
        INC     HL
        PUSH    HL
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        EX      DE,HL
        LD      DE,(rt_mat__r)
        OR      A
        SBC     HL,DE
        EX      DE,HL
        POP     HL
        LD      (HL),E
        INC     HL
        LD      (HL),D
        INC     HL
        PUSH    HL
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        EX      DE,HL
        LD      DE,(rt_mat__c)
        OR      A
        SBC     HL,DE
        EX      DE,HL
        POP     HL
        LD      (HL),E
        INC     HL
        LD      (HL),D
        LD      HL,(rt_mat__vw)
        LD      BC,(rt_mat__r)
        LD      DE,(rt_mat__c)
        CALL    rt_mat_el
        EX      DE,HL
        LD      HL,(rt_mat__vw)
        LD      (HL),E
        INC     HL
        LD      (HL),D
        RET

; DE -> view number, BC -> n: at most n x n (n >= 0)
rt_mat_limit:
        LD      A,(DE)
        CALL    rt_mat__vp
        PUSH    HL
        LD      H,B
        LD      L,C
        CALL    rt_mat__i16
        LD      (rt_mat__r),HL
        POP     HL
        LD      A,(rt_mat__r+1)
        BIT     7,A
        RET     NZ
        INC     HL
        INC     HL
        CALL    rt_mat__lim
        INC     HL
rt_mat__lim:                            ; (HL) = min((HL), n); HL to the high byte
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        PUSH    HL
        LD      HL,(rt_mat__r)
        EX      DE,HL
        CALL    rt_mat__lt              ; CF if (HL) < n
        POP     HL
        RET     C
        LD      DE,(rt_mat__r)
        LD      (HL),D
        DEC     HL
        LD      (HL),E
        INC     HL
        RET

; ---- operations ------------------------------------------------------------------------------------

; DE = slot, BC -> x: MAT CLR / MAT SET, the whole array (index 0 included)
rt_mat_fill:
        PUSH    BC
        EX      DE,HL
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        INC     HL
        LD      A,D
        OR      E
        JR      NZ,rt_mat__f1
        POP     BC
        LD      A,15
        JP      rt_error
rt_mat__f1:
        LD      B,(HL)                  ; B = dimensions, DE = block
        EX      DE,HL
        LD      DE,1
rt_mat__f2:                             ; elements: product of the extents
        PUSH    BC
        LD      C,(HL)
        INC     HL
        LD      B,(HL)
        INC     HL
        INC     HL
        INC     HL
        PUSH    HL
        LD      H,B
        LD      L,C
        CALL    rt_mat__mul
        EX      DE,HL
        POP     HL
        POP     BC
        DJNZ    rt_mat__f2
        EX      DE,HL                   ; HL = count, DE = elements
        POP     BC                      ; BC -> x
rt_mat__f3:
        LD      A,H
        OR      L
        RET     Z
        PUSH    HL
        PUSH    BC
        LD      H,B
        LD      L,C
        CALL    rt_mat__cp8
        POP     BC
        POP     HL
        DEC     HL
        JR      rt_mat__f3

; loop over all elements (i, j) of view 0: HL = routine for each element (called with i, j set)
rt_mat__each:
        LD      (rt_mat__body),HL
        LD      HL,0
        LD      (rt_mat__i),HL
rt_mat__ei:
        LD      HL,(rt_mat__i)
        LD      DE,(rt_mat__R0)
        CALL    rt_mat__lt
        RET     NC
        LD      HL,0
        LD      (rt_mat__j),HL
rt_mat__ej:
        LD      HL,(rt_mat__j)
        LD      DE,(rt_mat__C0)
        CALL    rt_mat__lt
        JR      NC,rt_mat__en
        LD      HL,rt_mat__ej1
        PUSH    HL
        LD      HL,(rt_mat__body)
        JP      (HL)
rt_mat__ej1:
        LD      HL,(rt_mat__j)
        INC     HL
        LD      (rt_mat__j),HL
        JR      rt_mat__ej
rt_mat__en:
        LD      HL,(rt_mat__i)
        INC     HL
        LD      (rt_mat__i),HL
        JR      rt_mat__ei

; MAT ONE: view 0 square (else 82) -> 1 on the diagonal, 0 elsewhere
rt_mat_one:
        LD      HL,0
        LD      (rt_mat__res),HL
        LD      HL,(rt_mat__R0)
        LD      DE,(rt_mat__C0)
        OR      A
        SBC     HL,DE
        LD      A,82
        JP      NZ,rt_mat__err
        LD      HL,rt_mat__onee
        JR      rt_mat__each
rt_mat__onee:
        XOR     A
        CALL    rt_mat__eij
        LD      DE,(rt_mat__i)
        PUSH    HL
        LD      HL,(rt_mat__j)
        OR      A
        SBC     HL,DE
        POP     HL
        JP      Z,rt_mat__one
        JP      rt_mat__zero

; DE -> 1: subtract. view 0 = view 1 +/- view 2 (same order, else 77)
rt_mat_addsub:
        LD      A,(DE)
        LD      HL,rt_mat__add
        OR      A
        JR      Z,rt_mat__as1
        LD      HL,rt_mat__sub
rt_mat__as1:
        LD      (rt_mat__fn),HL
        LD      HL,0
        LD      (rt_mat__res),HL
        LD      HL,(rt_mat__R0)
        LD      DE,(rt_mat__R1)
        CALL    rt_mat__ne
        LD      HL,(rt_mat__C0)
        LD      DE,(rt_mat__C1)
        CALL    rt_mat__ne
        LD      HL,(rt_mat__R1)
        LD      DE,(rt_mat__R2)
        CALL    rt_mat__ne
        LD      HL,(rt_mat__C1)
        LD      DE,(rt_mat__C2)
        CALL    rt_mat__ne
        LD      HL,rt_mat__ase
        JP      rt_mat__each
rt_mat__ase:
        LD      A,2
        CALL    rt_mat__eij
        PUSH    HL
        LD      A,1
        CALL    rt_mat__eij
        PUSH    HL
        XOR     A
        CALL    rt_mat__eij
        POP     DE
        POP     BC
        JP      rt_mat__call

; HL, DE: different -> error 77 (does not return)
rt_mat__ne:
        OR      A
        SBC     HL,DE
        RET     Z
        POP     HL
        LD      A,77
        JP      rt_mat__err

; DE -> x, BC -> op (0: a + x, 1: a - x, 2: a * x) on view 0
rt_mat_scalar:
        LD      A,(BC)
        PUSH    AF
        EX      DE,HL
        LD      DE,rt_mat__xs
        CALL    rt_mat__cp8
        POP     AF
        LD      HL,rt_mat__add
        OR      A
        JR      Z,rt_mat__sc1
        LD      HL,rt_mat__sub
        DEC     A
        JR      Z,rt_mat__sc1
        LD      HL,rt_mat__fmul
rt_mat__sc1:
        LD      (rt_mat__fn),HL
        LD      HL,rt_mat__sce
        JP      rt_mat__each
rt_mat__sce:
        XOR     A
        CALL    rt_mat__eij
        LD      D,H
        LD      E,L
        LD      BC,rt_mat__xs
        JP      rt_mat__call

; HL = rows, DE = columns -> rt_mat__w = work storage of rows * columns floats (error 8)
rt_mat__work:
        CALL    rt_mat__mul
        JR      C,rt_mat__wm
        ADD     HL,HL
        JR      C,rt_mat__wm
        ADD     HL,HL
        JR      C,rt_mat__wm
        ADD     HL,HL
        JR      C,rt_mat__wm
        LD      B,H
        LD      C,L
        CALL    rt_arr_work
        LD      (rt_mat__w),HL
        LD      (rt_mat__wp),HL
        RET
rt_mat__wm:
        LD      A,8
        JP      rt_mat__err

; HL = destination: s = sum over k < rt_mat__kn of view A1 (i, k) * view A2 (k, j)
; (rt_mat__va, rt_mat__vb hold the two view numbers; triple uses (0, j) for the second)
rt_mat__dotk:
        LD      (rt_mat__sd),HL
        CALL    rt_mat__zero
        LD      HL,0
        LD      (rt_mat__k),HL
rt_mat__dk1:
        LD      HL,(rt_mat__k)
        LD      DE,(rt_mat__kn)
        CALL    rt_mat__lt
        RET     NC
        LD      A,(rt_mat__va)
        CALL    rt_mat__vp
        LD      BC,(rt_mat__i)
        LD      DE,(rt_mat__k)
        CALL    rt_mat_el
        PUSH    HL
        LD      A,(rt_mat__vb)
        CALL    rt_mat__vp
        LD      BC,(rt_mat__k)
        LD      DE,(rt_mat__j)
        LD      A,(rt_mat__tr)          ; triple: element (0, k)
        OR      A
        JR      Z,rt_mat__dk2
        LD      D,B
        LD      E,C
        LD      BC,0
rt_mat__dk2:
        CALL    rt_mat_el
        LD      B,H
        LD      C,L
        POP     DE
        LD      HL,rt_mat__pr
        CALL    fp_mul
        LD      HL,(rt_mat__sd)
        LD      D,H
        LD      E,L
        LD      BC,rt_mat__pr
        CALL    fp_add
        LD      HL,(rt_mat__k)
        INC     HL
        LD      (rt_mat__k),HL
        JR      rt_mat__dk1

; view 0 = view 1 * view 2 (a may be b or c; order: else 77); two 1-D arrays: the first one as
; column (outer product)
rt_mat_mul:
        LD      HL,0
        LD      (rt_mat__res),HL
        LD      A,(rt_mat_v+rt_mat__VS+10)
        LD      C,A
        LD      A,(rt_mat_v+2*rt_mat__VS+10)
        AND     C
        DEC     A
        JR      NZ,rt_mat__mu0
        LD      HL,(rt_mat__R1)         ; view 1: rows <-> columns, steps too
        LD      DE,(rt_mat__C1)
        LD      (rt_mat__R1),DE
        LD      (rt_mat__C1),HL
        LD      HL,(rt_mat_v+rt_mat__VS+6)
        LD      DE,(rt_mat_v+rt_mat__VS+8)
        LD      (rt_mat_v+rt_mat__VS+6),DE
        LD      (rt_mat_v+rt_mat__VS+8),HL
rt_mat__mu0:
        LD      HL,(rt_mat__C1)
        LD      DE,(rt_mat__R2)
        CALL    rt_mat__ne
        LD      HL,(rt_mat__R0)
        LD      DE,(rt_mat__R1)
        CALL    rt_mat__ne
        LD      HL,(rt_mat__C0)
        LD      DE,(rt_mat__C2)
        CALL    rt_mat__ne
        LD      HL,(rt_mat__R0)
        LD      DE,(rt_mat__C0)
        CALL    rt_mat__work
        LD      HL,(rt_mat__C1)
        LD      (rt_mat__kn),HL
        LD      HL,1+256*2
        LD      (rt_mat__va),HL         ; va = 1, vb = 2
        XOR     A
        LD      (rt_mat__tr),A
        LD      HL,rt_mat__mle
        CALL    rt_mat__each
        LD      HL,(rt_mat__w)          ; the result into view 0
        LD      (rt_mat__wp),HL
        LD      HL,rt_mat__wback
        JP      rt_mat__each
rt_mat__mle:
        LD      HL,(rt_mat__wp)
        CALL    rt_mat__dotk
        JR      rt_mat__wnext

; element (i, j) of view 0 = the next work element
rt_mat__wback:
        XOR     A
        CALL    rt_mat__eij
        EX      DE,HL
        LD      HL,(rt_mat__wp)
        CALL    rt_mat__cp8
rt_mat__wnext:
        LD      HL,(rt_mat__wp)
        LD      DE,8
        ADD     HL,DE
        LD      (rt_mat__wp),HL
        RET

; HL = result: view 0 * view 1 for two vectors of the same length (1 x n or n x 1; else 77)
rt_mat_dot:
        LD      (rt_mat__res),HL
        LD      HL,(rt_mat__R0)
        LD      DE,(rt_mat__C0)
        CALL    rt_mat__vec
        LD      (rt_mat__kn),HL
        LD      HL,(rt_mat__R1)
        LD      DE,(rt_mat__C1)
        CALL    rt_mat__vec
        LD      DE,(rt_mat__kn)
        CALL    rt_mat__ne
        LD      HL,(rt_mat__res)
        CALL    rt_mat__zero
        LD      HL,0
        LD      (rt_mat__k),HL
rt_mat__dt1:
        LD      HL,(rt_mat__k)
        LD      DE,(rt_mat__kn)
        CALL    rt_mat__lt
        RET     NC
        LD      HL,rt_mat_v
        CALL    rt_mat__velk
        PUSH    HL
        LD      HL,rt_mat_v+rt_mat__VS
        CALL    rt_mat__velk
        LD      B,H
        LD      C,L
        POP     DE
        LD      HL,rt_mat__pr
        CALL    fp_mul
        LD      HL,(rt_mat__res)
        LD      D,H
        LD      E,L
        LD      BC,rt_mat__pr
        CALL    fp_add
        LD      HL,(rt_mat__k)
        INC     HL
        LD      (rt_mat__k),HL
        JR      rt_mat__dt1

; HL = rows, DE = columns: a vector -> HL = length; else error 77
rt_mat__vec:
        LD      A,H
        OR      A
        JR      NZ,rt_mat__vc1
        LD      A,L
        DEC     A
        JR      NZ,rt_mat__vc1
        EX      DE,HL                   ; 1 x n
        RET
rt_mat__vc1:
        LD      A,D
        OR      A
        JR      NZ,rt_mat__vc2
        LD      A,E
        DEC     A
        RET     Z                       ; n x 1
rt_mat__vc2:
        POP     HL
        LD      A,77
        JP      rt_mat__err

; HL = view (a vector): element k -> HL
rt_mat__velk:
        PUSH    HL
        INC     HL
        INC     HL
        LD      A,(HL)
        DEC     A
        INC     HL
        OR      (HL)
        POP     HL
        LD      BC,0
        LD      DE,(rt_mat__k)
        JP      Z,rt_mat_el             ; one row: (0, k)
        LD      B,D
        LD      C,E
        LD      DE,0
        JP      rt_mat_el

; HL = result: view 0 * view 1 * view 2 (vector, matrix, vector; else 79)
rt_mat_triple:
        LD      (rt_mat__res),HL
        LD      HL,(rt_mat__R0)
        LD      DE,1
        CALL    rt_mat__ne79
        LD      HL,(rt_mat__R2)
        LD      DE,1
        CALL    rt_mat__ne79
        LD      HL,(rt_mat__C0)
        LD      DE,(rt_mat__R1)
        CALL    rt_mat__ne79
        LD      HL,(rt_mat__C2)
        LD      DE,(rt_mat__C1)
        CALL    rt_mat__ne79
        LD      HL,(rt_mat__res)
        CALL    rt_mat__zero
        LD      HL,(rt_mat__C1)
        LD      (rt_mat__kn),HL
        LD      HL,1+256*2
        LD      (rt_mat__va),HL
        LD      A,1
        LD      (rt_mat__tr),A
        LD      HL,0
        LD      (rt_mat__i),HL
rt_mat__tp1:
        LD      HL,(rt_mat__i)
        LD      DE,(rt_mat__R1)
        CALL    rt_mat__lt
        RET     NC
        LD      HL,rt_mat__t                ; t = sum over j of b(i,j) * c(0,j)
        CALL    rt_mat__dotk
        LD      HL,rt_mat_v
        LD      BC,0
        LD      DE,(rt_mat__i)
        CALL    rt_mat_el
        EX      DE,HL
        LD      HL,rt_mat__pr
        LD      BC,rt_mat__t
        CALL    fp_mul                  ; a(0,i) * t
        LD      HL,(rt_mat__res)
        LD      D,H
        LD      E,L
        LD      BC,rt_mat__pr
        CALL    fp_add
        LD      HL,(rt_mat__i)
        INC     HL
        LD      (rt_mat__i),HL
        JR      rt_mat__tp1
rt_mat__ne79:
        OR      A
        SBC     HL,DE
        RET     Z
        POP     HL
        LD      A,79
        JP      rt_mat__err

; DE -> 1: by column. every row or column of view 0 divided by the root of its sum of squares
; (a zero line: error 0)
rt_mat_norm:
        LD      A,(DE)
        LD      (rt_mat__tr),A
        LD      HL,0
        LD      (rt_mat__res),HL
        LD      (rt_mat__i),HL          ; i = line
rt_mat__nl:
        LD      HL,(rt_mat__i)
        LD      DE,(rt_mat__R0)
        LD      A,(rt_mat__tr)
        OR      A
        JR      Z,rt_mat__nl1
        LD      DE,(rt_mat__C0)
rt_mat__nl1:
        CALL    rt_mat__lt
        RET     NC
        LD      HL,rt_mat__t
        CALL    rt_mat__zero
        LD      HL,0
        LD      (rt_mat__k),HL
rt_mat__nk:
        CALL    rt_mat__nlen
        JR      NC,rt_mat__nr
        CALL    rt_mat__nel
        LD      D,H
        LD      E,L
        LD      B,H
        LD      C,L
        LD      HL,rt_mat__pr
        CALL    fp_mul
        LD      HL,rt_mat__t
        LD      D,H
        LD      E,L
        LD      BC,rt_mat__pr
        CALL    fp_add
        LD      HL,(rt_mat__k)
        INC     HL
        LD      (rt_mat__k),HL
        JR      rt_mat__nk
rt_mat__nr:
        LD      DE,rt_mat__t
        LD      HL,rt_mat__zr
        CALL    rt_mat__zero
        LD      BC,rt_mat__zr
        CALL    rt_mat__cmp
        OR      A
        LD      A,0
        JP      Z,rt_mat__err           ; division by zero
        LD      HL,rt_mat__t
        LD      D,H
        LD      E,L
        CALL    fp_sqrt
        LD      HL,0
        LD      (rt_mat__k),HL
rt_mat__nd2:
        CALL    rt_mat__nlen
        JR      NC,rt_mat__nnx
        CALL    rt_mat__nel
        LD      D,H
        LD      E,L
        LD      BC,rt_mat__t
        CALL    fp_div
        LD      HL,(rt_mat__k)
        INC     HL
        LD      (rt_mat__k),HL
        JR      rt_mat__nd2
rt_mat__nnx:
        LD      HL,(rt_mat__i)
        INC     HL
        LD      (rt_mat__i),HL
        JP      rt_mat__nl
rt_mat__nlen:                           ; CF if k < length of a line
        LD      HL,(rt_mat__k)
        LD      DE,(rt_mat__C0)
        LD      A,(rt_mat__tr)
        OR      A
        JR      Z,rt_mat__nn1
        LD      DE,(rt_mat__R0)
rt_mat__nn1:
        JP      rt_mat__lt
rt_mat__nel:                            ; HL = element (line, k) or (k, line)
        LD      HL,rt_mat_v
        LD      BC,(rt_mat__i)
        LD      DE,(rt_mat__k)
        LD      A,(rt_mat__tr)
        OR      A
        JP      Z,rt_mat_el
        LD      BC,(rt_mat__k)
        LD      DE,(rt_mat__i)
        JP      rt_mat_el

; ---- square matrices (row-major copies in work storage) --------------------------------------------

; HL = BC = i, DE = j: element of the work copy at rt_mat__wb -> HL
rt_mat__wel:
        PUSH    DE
        LD      H,B
        LD      L,C
        LD      DE,(rt_mat__n)
        CALL    rt_mat__mul
        POP     DE
        ADD     HL,DE
        ADD     HL,HL
        ADD     HL,HL
        ADD     HL,HL
        LD      DE,(rt_mat__wb)
        ADD     HL,DE
        RET

; HL = view (square): n, the copy at rt_mat__w (twice the room if A = 2), tol = n * 2^-52 * max |a(i,j)|
rt_mat__square:
        LD      (rt_mat__vw),HL
        PUSH    AF
        INC     HL
        INC     HL
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        LD      (rt_mat__n),DE
        LD      H,D
        LD      L,E
        CALL    rt_mat__mul
        POP     AF
        DEC     A
        JR      Z,rt_mat__sq0
        ADD     HL,HL
rt_mat__sq0:
        LD      DE,1
        CALL    rt_mat__work            ; (n * n [* 2]) floats
        LD      HL,(rt_mat__w)
        LD      (rt_mat__wb),HL
        LD      HL,rt_mat__mx
        CALL    rt_mat__zero
        LD      HL,0
        LD      (rt_mat__i),HL
rt_mat__sq1:
        LD      HL,(rt_mat__i)
        LD      DE,(rt_mat__n)
        CALL    rt_mat__lt
        JR      NC,rt_mat__sq4
        LD      HL,0
        LD      (rt_mat__j),HL
rt_mat__sq2:
        LD      HL,(rt_mat__j)
        LD      DE,(rt_mat__n)
        CALL    rt_mat__lt
        JR      NC,rt_mat__sq3
        LD      HL,(rt_mat__vw)
        LD      BC,(rt_mat__i)
        LD      DE,(rt_mat__j)
        CALL    rt_mat_el
        PUSH    HL
        LD      DE,(rt_mat__wp)
        CALL    rt_mat__cp8
        POP     DE
        CALL    rt_mat__abs
        LD      DE,rt_mat__ab
        LD      BC,rt_mat__mx
        CALL    rt_mat__cmp
        DEC     A
        JR      NZ,rt_mat__sq5
        LD      HL,rt_mat__ab
        LD      DE,rt_mat__mx
        CALL    rt_mat__cp8
rt_mat__sq5:
        CALL    rt_mat__wnext
        LD      HL,(rt_mat__j)
        INC     HL
        LD      (rt_mat__j),HL
        JR      rt_mat__sq2
rt_mat__sq3:
        LD      HL,(rt_mat__i)
        INC     HL
        LD      (rt_mat__i),HL
        JR      rt_mat__sq1
rt_mat__sq4:
        LD      HL,rt_mat__tol          ; tol = n * (mx * 2^-52)
        LD      DE,rt_mat__mx
        LD      BC,rt_mat__e52
        CALL    fp_mul
        LD      HL,(rt_mat__n)
        LD      (rt_mat__pr),HL
        LD      HL,0
        LD      (rt_mat__pr+2),HL
        LD      HL,rt_mat__ab
        LD      DE,rt_mat__pr
        CALL    fp_from_i32
        LD      HL,rt_mat__tol
        LD      D,H
        LD      E,L
        LD      BC,rt_mat__ab
        JP      fp_mul

; rows A (rt_mat__ra) and B (rt_mat__rb) of the copy at rt_mat__wb exchanged
rt_mat__swap:
        LD      BC,(rt_mat__ra)
        LD      DE,0
        CALL    rt_mat__wel
        PUSH    HL
        LD      BC,(rt_mat__rb)
        LD      DE,0
        CALL    rt_mat__wel
        POP     DE
        LD      BC,(rt_mat__n)          ; n * 8 bytes
        SLA     C
        RL      B
        SLA     C
        RL      B
        SLA     C
        RL      B
rt_mat__sw1:
        LD      A,B
        OR      C
        RET     Z
        PUSH    BC
        LD      A,(DE)
        LD      C,(HL)
        LD      (HL),A
        LD      A,C
        LD      (DE),A
        INC     HL
        INC     DE
        POP     BC
        DEC     BC
        JR      rt_mat__sw1

; |w(i,k)| -> rt_mat__ab (rt_mat__wb), BC = i, DE = k
rt_mat__wab:
        CALL    rt_mat__wel
        EX      DE,HL
        JP      rt_mat__abs

; -> rt_mat__p = row with the largest |w(i,k)| for i >= rt_mat__fr (the first of equal ones), k = rt_mat__k
rt_mat__pivot:
        LD      HL,(rt_mat__fr)
        LD      (rt_mat__p),HL
rt_mat__pv1:
        INC     HL
        LD      (rt_mat__pi),HL
        LD      DE,(rt_mat__n)
        CALL    rt_mat__lt
        RET     NC
        LD      BC,(rt_mat__p)
        LD      DE,(rt_mat__k)
        CALL    rt_mat__wab
        LD      HL,rt_mat__ab
        LD      DE,rt_mat__ab2
        CALL    rt_mat__cp8
        LD      BC,(rt_mat__pi)
        LD      DE,(rt_mat__k)
        CALL    rt_mat__wab
        LD      DE,rt_mat__ab
        LD      BC,rt_mat__ab2
        CALL    rt_mat__cmp
        DEC     A
        LD      HL,(rt_mat__pi)
        JR      NZ,rt_mat__pv1
        LD      (rt_mat__p),HL
        JR      rt_mat__pv1

; Z set if |w(p,k)| <= tol (the pivot counts as 0)
rt_mat__small:
        LD      BC,(rt_mat__p)
        LD      DE,(rt_mat__k)
        CALL    rt_mat__wab
        LD      DE,rt_mat__ab
        LD      BC,rt_mat__tol
        CALL    rt_mat__cmp
        CP      1
        JR      Z,rt_mat__sm1
        XOR     A                       ; Z: up to tol
        RET
rt_mat__sm1:
        OR      A                       ; NZ: greater
        RET

; rows below r: row i -= (w(i,k) / w(r,k)) * row r, columns k+1 .. n-1; w(i,k) = 0
rt_mat__elim:
        LD      HL,(rt_mat__r)
rt_mat__el1:
        INC     HL
        LD      (rt_mat__i),HL
        LD      DE,(rt_mat__n)
        CALL    rt_mat__lt
        RET     NC
        LD      BC,(rt_mat__i)          ; f = w(i,k) / w(r,k)
        LD      DE,(rt_mat__k)
        CALL    rt_mat__wel
        PUSH    HL
        LD      BC,(rt_mat__r)
        LD      DE,(rt_mat__k)
        CALL    rt_mat__wel
        LD      B,H
        LD      C,L
        POP     DE
        LD      HL,rt_mat__f
        CALL    fp_div
        LD      HL,(rt_mat__k)
rt_mat__el2:
        INC     HL
        LD      (rt_mat__j),HL
        LD      DE,(rt_mat__n)
        CALL    rt_mat__lt
        JR      NC,rt_mat__el3
        LD      BC,(rt_mat__r)          ; w(i,j) -= f * w(r,j)
        LD      DE,(rt_mat__j)
        CALL    rt_mat__wel
        LD      B,H
        LD      C,L
        LD      DE,rt_mat__f
        LD      HL,rt_mat__pr
        CALL    fp_mul
        LD      BC,(rt_mat__i)
        LD      DE,(rt_mat__j)
        CALL    rt_mat__wel
        LD      D,H
        LD      E,L
        LD      BC,rt_mat__pr
        CALL    fp_sub
        LD      HL,(rt_mat__j)
        JR      rt_mat__el2
rt_mat__el3:
        LD      BC,(rt_mat__i)
        LD      DE,(rt_mat__k)
        CALL    rt_mat__wel
        CALL    rt_mat__zero
        LD      HL,(rt_mat__i)
        JR      rt_mat__el1

; HL = result, DE -> 1: QDET. MAT DET (quick 0): largest pivot, up to tol is 0; MAT QDET: the first
; pivot that is not 0. The determinant is the product of the pivots in order, then the sign.
rt_mat_det:
        LD      (rt_mat__res),HL
        LD      A,(DE)
        LD      (rt_mat__tr),A
        CALL    rt_mat__sq82
        LD      HL,rt_mat_v
        LD      A,1
        CALL    rt_mat__square
        LD      HL,(rt_mat__res)
        CALL    rt_mat__one
        XOR     A
        LD      (rt_mat__sg),A
        LD      HL,0
        LD      (rt_mat__k),HL
rt_mat__dk:
        LD      HL,(rt_mat__k)
        LD      DE,(rt_mat__n)
        CALL    rt_mat__lt
        JP      NC,rt_mat__dsg
        LD      A,(rt_mat__tr)
        OR      A
        JR      Z,rt_mat__dlg
        LD      HL,(rt_mat__k)          ; quick: the first pivot that is not 0
rt_mat__dq:
        LD      (rt_mat__p),HL
        LD      DE,(rt_mat__n)
        CALL    rt_mat__lt
        JR      NC,rt_mat__d0
        LD      BC,(rt_mat__p)
        LD      DE,(rt_mat__k)
        CALL    rt_mat__wel
        EX      DE,HL
        LD      HL,rt_mat__zr
        CALL    rt_mat__zero
        LD      BC,rt_mat__zr
        CALL    rt_mat__cmp
        OR      A
        JR      NZ,rt_mat__dsw
        LD      HL,(rt_mat__p)
        INC     HL
        JR      rt_mat__dq
rt_mat__dlg:
        LD      HL,(rt_mat__k)
        LD      (rt_mat__fr),HL
        CALL    rt_mat__pivot
        CALL    rt_mat__small
        JR      Z,rt_mat__d0
rt_mat__dsw:
        LD      HL,(rt_mat__p)
        LD      DE,(rt_mat__k)
        OR      A
        SBC     HL,DE
        JR      Z,rt_mat__dml
        LD      (rt_mat__ra),DE
        LD      HL,(rt_mat__p)
        LD      (rt_mat__rb),HL
        CALL    rt_mat__swap
        LD      A,(rt_mat__sg)
        XOR     1
        LD      (rt_mat__sg),A
rt_mat__dml:
        LD      BC,(rt_mat__k)          ; det *= w(k,k)
        LD      D,B
        LD      E,C
        CALL    rt_mat__wel
        LD      B,H
        LD      C,L
        LD      HL,(rt_mat__res)
        LD      D,H
        LD      E,L
        CALL    fp_mul
        LD      HL,(rt_mat__k)
        LD      (rt_mat__r),HL
        CALL    rt_mat__elim
        LD      HL,(rt_mat__k)
        INC     HL
        LD      (rt_mat__k),HL
        JP      rt_mat__dk
rt_mat__d0:
        LD      HL,(rt_mat__res)
        JP      rt_mat__zero
rt_mat__dsg:
        LD      A,(rt_mat__sg)
        OR      A
        RET     Z
        LD      DE,(rt_mat__res)
        LD      HL,rt_mat__zr
        CALL    rt_mat__zero
        LD      BC,rt_mat__zr
        CALL    rt_mat__cmp
        OR      A
        RET     Z
        LD      HL,(rt_mat__res)
        LD      DE,7
        ADD     HL,DE
        LD      A,(HL)
        XOR     &80
        LD      (HL),A
        RET

; view 0 not square: error 82
rt_mat__sq82:
        LD      HL,(rt_mat__R0)
        LD      DE,(rt_mat__C0)
        OR      A
        SBC     HL,DE
        RET     Z
        POP     HL
        LD      A,82
        JP      rt_mat__err

; HL = result: MAT RANG, row echelon form with the largest pivots; a column without a pivot above
; tol is skipped
rt_mat_rank:
        LD      (rt_mat__res),HL
        CALL    rt_mat__sq82
        LD      HL,rt_mat_v
        LD      A,1
        CALL    rt_mat__square
        LD      HL,0
        LD      (rt_mat__r),HL
        LD      (rt_mat__k),HL
rt_mat__rk:
        LD      HL,(rt_mat__k)
        LD      DE,(rt_mat__n)
        CALL    rt_mat__lt
        JR      NC,rt_mat__rk9
        LD      HL,(rt_mat__r)
        LD      DE,(rt_mat__n)
        CALL    rt_mat__lt
        JR      NC,rt_mat__rk9
        LD      HL,(rt_mat__r)
        LD      (rt_mat__fr),HL
        CALL    rt_mat__pivot
        CALL    rt_mat__small
        JR      Z,rt_mat__rkn
        LD      HL,(rt_mat__p)
        LD      DE,(rt_mat__r)
        OR      A
        SBC     HL,DE
        JR      Z,rt_mat__rk1
        LD      (rt_mat__ra),DE
        LD      HL,(rt_mat__p)
        LD      (rt_mat__rb),HL
        CALL    rt_mat__swap
rt_mat__rk1:
        CALL    rt_mat__elim
        LD      HL,(rt_mat__r)
        INC     HL
        LD      (rt_mat__r),HL
rt_mat__rkn:
        LD      HL,(rt_mat__k)
        INC     HL
        LD      (rt_mat__k),HL
        JR      rt_mat__rk
rt_mat__rk9:
        LD      HL,(rt_mat__r)
        LD      (rt_mat__pr),HL
        LD      HL,0
        LD      (rt_mat__pr+2),HL
        LD      HL,(rt_mat__res)
        LD      DE,rt_mat__pr
        JP      fp_from_i32

; MAT INV b()=a(): view 0 = b, view 1 = a. Gauss-Jordan with the largest pivots; singular: error 0
rt_mat_inv:
        LD      HL,0
        LD      (rt_mat__res),HL
        LD      HL,(rt_mat__R1)
        LD      DE,(rt_mat__C1)
        OR      A
        SBC     HL,DE
        LD      A,82
        JP      NZ,rt_mat__err
        LD      HL,(rt_mat__R0)
        LD      DE,(rt_mat__R1)
        OR      A
        SBC     HL,DE
        LD      A,83
        JP      NZ,rt_mat__err
        LD      HL,(rt_mat__C0)
        LD      DE,(rt_mat__R1)
        OR      A
        SBC     HL,DE
        JP      NZ,rt_mat__err
        LD      HL,rt_mat_v+rt_mat__VS
        LD      A,2
        CALL    rt_mat__square          ; w at rt_mat__w, v behind it
        LD      HL,(rt_mat__wp)
        LD      (rt_mat__wv),HL
        LD      (rt_mat__wb),HL         ; v = identity
        LD      HL,0
        LD      (rt_mat__i),HL
rt_mat__iv1:
        LD      HL,(rt_mat__i)
        LD      DE,(rt_mat__n)
        CALL    rt_mat__lt
        JR      NC,rt_mat__iv4
        LD      HL,0
        LD      (rt_mat__j),HL
rt_mat__iv2:
        LD      HL,(rt_mat__j)
        LD      DE,(rt_mat__n)
        CALL    rt_mat__lt
        JR      NC,rt_mat__iv3
        LD      BC,(rt_mat__i)
        LD      DE,(rt_mat__j)
        CALL    rt_mat__wel
        LD      DE,(rt_mat__i)
        PUSH    HL
        LD      HL,(rt_mat__j)
        OR      A
        SBC     HL,DE
        POP     HL
        CALL    Z,rt_mat__one
        CALL    NZ,rt_mat__zero
        LD      HL,(rt_mat__j)
        INC     HL
        LD      (rt_mat__j),HL
        JR      rt_mat__iv2
rt_mat__iv3:
        LD      HL,(rt_mat__i)
        INC     HL
        LD      (rt_mat__i),HL
        JR      rt_mat__iv1
rt_mat__iv4:
        LD      HL,0
        LD      (rt_mat__k),HL
rt_mat__ik:
        CALL    rt_mat__usew
        LD      HL,(rt_mat__k)
        LD      DE,(rt_mat__n)
        CALL    rt_mat__lt
        JP      NC,rt_mat__iout
        LD      HL,(rt_mat__k)
        LD      (rt_mat__fr),HL
        CALL    rt_mat__pivot
        CALL    rt_mat__small
        LD      A,0
        JP      Z,rt_mat__err           ; singular: division by zero
        LD      HL,(rt_mat__p)
        LD      DE,(rt_mat__k)
        OR      A
        SBC     HL,DE
        JR      Z,rt_mat__ik1
        LD      (rt_mat__ra),DE
        LD      HL,(rt_mat__p)
        LD      (rt_mat__rb),HL
        CALL    rt_mat__swap
        CALL    rt_mat__usev
        CALL    rt_mat__swap
        CALL    rt_mat__usew
rt_mat__ik1:
        LD      BC,(rt_mat__k)          ; piv = w(k,k)
        LD      D,B
        LD      E,C
        CALL    rt_mat__wel
        LD      DE,rt_mat__pv
        CALL    rt_mat__cp8
        LD      HL,(rt_mat__k)          ; w(k,j) /= piv for j > k
        LD      (rt_mat__r),HL
        INC     HL
        CALL    rt_mat__rdiv
        CALL    rt_mat__usev            ; v(k,j) /= piv for all j
        LD      HL,0
        CALL    rt_mat__rdiv
        CALL    rt_mat__usew
        LD      BC,(rt_mat__k)
        LD      D,B
        LD      E,C
        CALL    rt_mat__wel
        CALL    rt_mat__one
        LD      HL,0
        LD      (rt_mat__i),HL
rt_mat__ii:
        CALL    rt_mat__usew
        LD      HL,(rt_mat__i)
        LD      DE,(rt_mat__n)
        CALL    rt_mat__lt
        JR      NC,rt_mat__inx
        LD      HL,(rt_mat__i)
        LD      DE,(rt_mat__k)
        OR      A
        SBC     HL,DE
        JR      Z,rt_mat__iin
        LD      BC,(rt_mat__i)          ; f = w(i,k); 0: nothing to do
        LD      DE,(rt_mat__k)
        CALL    rt_mat__wel
        LD      DE,rt_mat__f
        CALL    rt_mat__cp8
        LD      DE,rt_mat__f
        LD      HL,rt_mat__zr
        CALL    rt_mat__zero
        LD      BC,rt_mat__zr
        CALL    rt_mat__cmp
        OR      A
        JR      Z,rt_mat__iin
        LD      HL,(rt_mat__k)          ; w(i,j) -= f * w(k,j) for j > k
        INC     HL
        CALL    rt_mat__rsub
        CALL    rt_mat__usev            ; v(i,j) -= f * v(k,j) for all j
        LD      HL,0
        CALL    rt_mat__rsub
        CALL    rt_mat__usew
        LD      BC,(rt_mat__i)
        LD      DE,(rt_mat__k)
        CALL    rt_mat__wel
        CALL    rt_mat__zero
rt_mat__iin:
        LD      HL,(rt_mat__i)
        INC     HL
        LD      (rt_mat__i),HL
        JR      rt_mat__ii
rt_mat__inx:
        LD      HL,(rt_mat__k)
        INC     HL
        LD      (rt_mat__k),HL
        JP      rt_mat__ik
rt_mat__iout:                           ; b = v
        LD      HL,(rt_mat__wv)
        LD      (rt_mat__wp),HL
        LD      HL,rt_mat__wback
        JP      rt_mat__each
rt_mat__usew:
        LD      HL,(rt_mat__w)
        LD      (rt_mat__wb),HL
        RET
rt_mat__usev:
        LD      HL,(rt_mat__wv)
        LD      (rt_mat__wb),HL
        RET

; HL = first column j: row r (rt_mat__r) of the copy: x(r,j) /= piv for j .. n-1
rt_mat__rdiv:
        LD      (rt_mat__j),HL
        LD      DE,(rt_mat__n)
        CALL    rt_mat__lt
        RET     NC
        LD      BC,(rt_mat__r)
        LD      DE,(rt_mat__j)
        CALL    rt_mat__wel
        LD      D,H
        LD      E,L
        LD      BC,rt_mat__pv
        CALL    fp_div
        LD      HL,(rt_mat__j)
        INC     HL
        JR      rt_mat__rdiv

; HL = first column j: x(i,j) -= f * x(k,j) for j .. n-1
rt_mat__rsub:
        LD      (rt_mat__j),HL
        LD      DE,(rt_mat__n)
        CALL    rt_mat__lt
        RET     NC
        LD      BC,(rt_mat__k)
        LD      DE,(rt_mat__j)
        CALL    rt_mat__wel
        LD      B,H
        LD      C,L
        LD      DE,rt_mat__f
        LD      HL,rt_mat__pr
        CALL    fp_mul
        LD      BC,(rt_mat__i)
        LD      DE,(rt_mat__j)
        CALL    rt_mat__wel
        LD      D,H
        LD      E,L
        LD      BC,rt_mat__pr
        CALL    fp_sub
        LD      HL,(rt_mat__j)
        INC     HL
        JR      rt_mat__rsub

; MAT TRANS a()=b(): view 0 = a, view 1 = b; a(j,i) = b(i,j); a must be cols(b) x rows(b) (error 83)
rt_mat_trans:
        LD      HL,0
        LD      (rt_mat__res),HL
        LD      HL,(rt_mat__R0)
        LD      DE,(rt_mat__C1)
        OR      A
        SBC     HL,DE
        LD      A,83
        JP      NZ,rt_mat__err
        LD      HL,(rt_mat__C0)
        LD      DE,(rt_mat__R1)
        OR      A
        SBC     HL,DE
        JP      NZ,rt_mat__err
        LD      HL,(rt_mat__R1)
        LD      DE,(rt_mat__C1)
        CALL    rt_mat__work
        LD      HL,(rt_mat__R1)         ; the copy of b, row-major
        LD      (rt_mat__hh),HL
        LD      HL,(rt_mat__C1)
        LD      (rt_mat__ww),HL
        LD      HL,rt_mat_v+rt_mat__VS
        LD      (rt_mat__vw),HL
        CALL    rt_mat__blk2w
        LD      A,1                     ; then a(j,i)
        LD      (rt_mat__tr),A
        LD      HL,rt_mat_v
        LD      (rt_mat__vw),HL
        JP      rt_mat__w2blk

; block of hh x ww of view rt_mat__vw (from (0,0)) -> work storage, row-major
rt_mat__blk2w:
        LD      HL,(rt_mat__w)
        LD      (rt_mat__wp),HL
        LD      HL,0
        LD      (rt_mat__i),HL
rt_mat__bw1:
        LD      HL,(rt_mat__i)
        LD      DE,(rt_mat__hh)
        CALL    rt_mat__lt
        RET     NC
        LD      HL,0
        LD      (rt_mat__j),HL
rt_mat__bw2:
        LD      HL,(rt_mat__j)
        LD      DE,(rt_mat__ww)
        CALL    rt_mat__lt
        JR      NC,rt_mat__bw3
        LD      HL,(rt_mat__vw)
        LD      BC,(rt_mat__i)
        LD      DE,(rt_mat__j)
        CALL    rt_mat_el
        LD      DE,(rt_mat__wp)
        CALL    rt_mat__cp8
        CALL    rt_mat__wnext
        LD      HL,(rt_mat__j)
        INC     HL
        LD      (rt_mat__j),HL
        JR      rt_mat__bw2
rt_mat__bw3:
        LD      HL,(rt_mat__i)
        INC     HL
        LD      (rt_mat__i),HL
        JR      rt_mat__bw1

; work storage (hh x ww, row-major) -> view rt_mat__vw at (r, c), or (c, r) if rt_mat__tr
rt_mat__w2blk:
        LD      HL,(rt_mat__w)
        LD      (rt_mat__wp),HL
        LD      HL,0
        LD      (rt_mat__i),HL
rt_mat__wb1:
        LD      HL,(rt_mat__i)
        LD      DE,(rt_mat__hh)
        CALL    rt_mat__lt
        RET     NC
        LD      HL,0
        LD      (rt_mat__j),HL
rt_mat__wb2:
        LD      HL,(rt_mat__j)
        LD      DE,(rt_mat__ww)
        CALL    rt_mat__lt
        JR      NC,rt_mat__wb3
        LD      HL,(rt_mat__vw)
        LD      BC,(rt_mat__i)
        LD      DE,(rt_mat__j)
        LD      A,(rt_mat__tr)
        OR      A
        JR      Z,rt_mat__wb4
        LD      BC,(rt_mat__j)
        LD      DE,(rt_mat__i)
rt_mat__wb4:
        CALL    rt_mat_el
        EX      DE,HL
        LD      HL,(rt_mat__wp)
        CALL    rt_mat__cp8
        CALL    rt_mat__wnext
        LD      HL,(rt_mat__j)
        INC     HL
        LD      (rt_mat__j),HL
        JR      rt_mat__wb2
rt_mat__wb3:
        LD      HL,(rt_mat__i)
        INC     HL
        LD      (rt_mat__i),HL
        JR      rt_mat__wb1

; MAT CPY / XCPY: DE -> h, BC -> w (< 0: as many as fit), rt_argp -> 1: transposed. Views 0
; (destination) and 1 (source) start at the offsets already; source (r, c) -> destination (r, c),
; transposed (c, r). Only elements that exist in both are copied.
rt_mat_cpy:
        PUSH    BC
        EX      DE,HL
        CALL    rt_mat__i16
        LD      (rt_mat__h),HL
        POP     HL
        CALL    rt_mat__i16
        LD      (rt_mat__wd),HL
        LD      HL,(rt_argp)
        LD      A,(HL)
        LD      (rt_mat__tr),A
        LD      HL,0
        LD      (rt_mat__res),HL
        LD      HL,(rt_mat__R1)         ; hh = rows of the source, ww = columns
        LD      (rt_mat__hh),HL
        LD      HL,(rt_mat__C1)
        LD      (rt_mat__ww),HL
        LD      DE,(rt_mat__R0)         ; limited by the destination
        LD      BC,(rt_mat__C0)
        LD      A,(rt_mat__tr)
        OR      A
        JR      Z,rt_mat__cp1
        LD      D,B
        LD      E,C
        LD      BC,(rt_mat__R0)
rt_mat__cp1:
        LD      HL,rt_mat__hh
        CALL    rt_mat__min
        LD      D,B
        LD      E,C
        LD      HL,rt_mat__ww
        CALL    rt_mat__min
        LD      DE,(rt_mat__h)          ; h, w >= 0: at most that
        BIT     7,D
        LD      HL,rt_mat__hh
        CALL    Z,rt_mat__min
        LD      DE,(rt_mat__wd)
        BIT     7,D
        LD      HL,rt_mat__ww
        CALL    Z,rt_mat__min
        LD      HL,(rt_mat__hh)         ; nothing: done
        CALL    rt_mat__pos
        RET     NC
        LD      HL,(rt_mat__ww)
        CALL    rt_mat__pos
        RET     NC
        LD      HL,(rt_mat__hh)
        LD      DE,(rt_mat__ww)
        CALL    rt_mat__work            ; the source may overlap the destination
        LD      HL,rt_mat_v+rt_mat__VS
        LD      (rt_mat__vw),HL
        CALL    rt_mat__blk2w
        LD      HL,rt_mat_v
        LD      (rt_mat__vw),HL
        JP      rt_mat__w2blk

; HL -> 16-bit value, DE: (HL) = min((HL), DE) (signed); keeps BC
rt_mat__min:
        PUSH    BC
        PUSH    HL
        LD      A,(HL)
        INC     HL
        LD      H,(HL)
        LD      L,A
        PUSH    DE
        CALL    rt_mat__lt
        POP     DE
        POP     HL
        POP     BC
        RET     C
        LD      (HL),E
        INC     HL
        LD      (HL),D
        RET

; HL -> CF set if HL > 0 (signed)
rt_mat__pos:
        BIT     7,H
        RET     NZ                      ; NC
        LD      A,H
        OR      L
        RET     Z                       ; NC
        SCF
        RET

;! section data
rt_mat__b0:
        DB      0
rt_mat__e52:                            ; 2^-52
        DB      0,0,0,0,0,0,&B0,&3C

;! section bss
rt_mat_v:
        DS      3*rt_mat__VS
rt_mat__vt:
        DS      10
rt_mat__vw:
        DS      2
rt_mat__col:
        DS      1
rt_mat__blk:
        DS      2
rt_mat__nd:
        DS      1
rt_mat__d2:
        DS      1
rt_mat__el0:
        DS      2
rt_mat__x1:
        DS      2
rt_mat__n1:
        DS      2
rt_mat__fl:
        DS      1
rt_mat__ng:
        DS      1
rt_mat__lw:
        DS      2
rt_mat__oi:
        DS      2
rt_mat__oj:
        DS      2
rt_mat__r:
        DS      2
rt_mat__c:
        DS      2
rt_mat__ov:
        DS      1
rt_mat__cr:
        DS      4
rt_mat__ab:
        DS      8
rt_mat__ab2:
        DS      8
rt_mat__res:
        DS      2
rt_mat__fn:
        DS      2
rt_mat__body:
        DS      2
rt_mat__i:
        DS      2
rt_mat__j:
        DS      2
rt_mat__k:
        DS      2
rt_mat__kn:
        DS      2
rt_mat__va:
        DS      1
rt_mat__vb:
        DS      1
rt_mat__tr:
        DS      1
rt_mat__sd:
        DS      2
rt_mat__w:
        DS      2
rt_mat__wp:
        DS      2
rt_mat__wb:
        DS      2
rt_mat__wv:
        DS      2
rt_mat__xs:
        DS      8
rt_mat__pr:
        DS      8
rt_mat__t:
        DS      8
rt_mat__f:
        DS      8
rt_mat__pv:
        DS      8
rt_mat__zr:
        DS      8
rt_mat__mx:
        DS      8
rt_mat__tol:
        DS      8
rt_mat__n:
        DS      2
rt_mat__p:
        DS      2
rt_mat__pi:
        DS      2
rt_mat__fr:
        DS      2
rt_mat__ra:
        DS      2
rt_mat__rb:
        DS      2
rt_mat__sg:
        DS      1
rt_mat__h:
        DS      2
rt_mat__wd:
        DS      2
rt_mat__hh:
        DS      2
rt_mat__ww:
        DS      2
