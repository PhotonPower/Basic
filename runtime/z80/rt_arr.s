;! module    rt_arr
;! provides  rt_arr_dim rt_arr_elem rt_arr_count rt_arr_erase rt_arr_base rt_arr_set_base rt_swap8 rt_arr_work
;! requires  rt_error rt_argp rt_str_free plat_arr plat_arr_end
;! group     core
;! clobbers  AF BC DE HL
;! doc       Arrays: DIM, element address with checks (errors 14-18, 8), DIM?, ERASE, OPTION BASE.

; Array slot (8 bytes): +0 address of the block (0: not dimensioned), +2 number of dimensions,
; +4 element size. Block in the array area (plat_arr .. plat_arr_end, used like a stack): the
; extents (upper - OPTION BASE + 1, 4 bytes each, the last dimension first), then the elements
; (first index fastest, cleared at DIM; strings: descriptors in 8-byte elements).

; DE = slot, BC -> element size << 8 | dimensions (1-3), rt_argp -> upper bounds
rt_arr_dim:
        LD      (rt_arr__slot),DE
        LD      A,(BC)
        LD      (rt_arr__nd),A
        INC     BC
        LD      A,(BC)
        LD      (rt_arr__es),A
        EX      DE,HL                   ; dimensioned already: error 14
        LD      A,(HL)
        INC     HL
        OR      (HL)
        LD      A,14
        JP      NZ,rt_error
        LD      HL,1                    ; HL = number of elements (16 bit)
        LD      (rt_arr__n),HL
        LD      A,(rt_arr__nd)
        LD      B,A
        LD      HL,rt_argp
        LD      DE,rt_arr__ext
rt_arr__d1:
        PUSH    BC
        PUSH    HL
        LD      A,(HL)                  ; pointer to the bound
        INC     HL
        LD      H,(HL)
        LD      L,A
        PUSH    DE
        CALL    rt_arr__minus           ; HL = bound - base (16 bit), CF if negative or too large
        POP     DE
        JP      C,rt_arr__range2
        INC     HL                      ; extent
        LD      A,H
        OR      L
        JP      Z,rt_arr__range2
        EX      DE,HL                   ; store the extent (4 bytes)
        LD      (HL),E
        INC     HL
        LD      (HL),D
        INC     HL
        LD      (HL),0
        INC     HL
        LD      (HL),0
        INC     HL
        PUSH    HL
        LD      HL,(rt_arr__n)          ; n *= extent
        LD      B,D
        LD      C,E
        CALL    rt_arr__mul
        JP      C,rt_arr__range3
        LD      (rt_arr__n),HL
        POP     DE
        POP     HL
        INC     HL
        INC     HL
        POP     BC
        DJNZ    rt_arr__d1
        LD      HL,(rt_arr__n)          ; bytes = elements * size + 4 * dimensions
        LD      A,(rt_arr__es)
        LD      C,A
        LD      B,0
        CALL    rt_arr__mul
        JP      C,rt_arr__mem
        LD      A,(rt_arr__nd)
        ADD     A,A
        ADD     A,A
        LD      C,A
        LD      B,0
        ADD     HL,BC
        JP      C,rt_arr__mem
        LD      (rt_arr__size),HL
        LD      HL,(rt_arr__top)
        LD      A,H
        OR      L
        JR      NZ,rt_arr__t1
        LD      HL,plat_arr
rt_arr__t1:
        LD      (rt_arr__blk),HL
        LD      BC,(rt_arr__size)
        ADD     HL,BC
        JP      C,rt_arr__mem
        EX      DE,HL                   ; DE = new top
        LD      HL,plat_arr_end
        OR      A
        SBC     HL,DE
        JP      C,rt_arr__mem
        LD      (rt_arr__top),DE
        LD      HL,(rt_arr__slot)       ; the slot
        LD      DE,(rt_arr__blk)
        LD      (HL),E
        INC     HL
        LD      (HL),D
        INC     HL
        LD      A,(rt_arr__nd)
        LD      (HL),A
        INC     HL
        LD      (HL),0
        INC     HL
        LD      A,(rt_arr__es)
        LD      (HL),A
        INC     HL
        LD      (HL),0
        LD      A,(rt_arr__nd)          ; the extents, the last dimension first
        LD      B,A
        LD      HL,rt_arr__ext
        ADD     A,A
        ADD     A,A
        LD      E,A
        LD      D,0
        ADD     HL,DE                   ; HL = after the last extent
        LD      DE,(rt_arr__blk)
rt_arr__cx:
        PUSH    BC
        DEC     HL
        DEC     HL
        DEC     HL
        DEC     HL
        PUSH    HL
        LD      BC,4
        LDIR
        POP     HL
        POP     BC
        DJNZ    rt_arr__cx
        LD      HL,(rt_arr__top)        ; clear the elements
        OR      A
        SBC     HL,DE
        LD      B,H
        LD      C,L
        EX      DE,HL
rt_arr__cl:
        LD      A,B
        OR      C
        RET     Z
        LD      (HL),0
        INC     HL
        DEC     BC
        JR      rt_arr__cl
rt_arr__range3:
        POP     HL
rt_arr__range2:
        POP     HL
        POP     BC
        LD      A,17                    ; dim out of range
        JP      rt_error
rt_arr__mem:
        LD      A,8                     ; out of memory
        JP      rt_error

; HL -> 32-bit value: HL = value - OPTION BASE (16 bit); CF if negative or >= 65536
rt_arr__minus:
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        INC     HL
        LD      A,(HL)
        INC     HL
        OR      (HL)
        JR      Z,rt_arr__m1
        SCF                             ; negative or very large
        RET
rt_arr__m1:
        EX      DE,HL
        LD      A,(rt_arr_base)
        LD      E,A
        LD      D,0
        OR      A
        SBC     HL,DE
        RET

; HL = HL * BC (16 bit), CF if it does not fit
rt_arr__mul:
        LD      D,H
        LD      E,L
        LD      HL,0
        LD      A,16
rt_arr__mu:
        ADD     HL,HL
        JR      C,rt_arr__mo
        EX      DE,HL
        ADD     HL,HL
        EX      DE,HL
        JR      NC,rt_arr__mn
        ADD     HL,BC
        JR      C,rt_arr__mo
rt_arr__mn:
        DEC     A
        JR      NZ,rt_arr__mu
        OR      A
        RET
rt_arr__mo:
        SCF
        RET

; DE = slot, BC -> number of indices (1-3), rt_argp -> indices -> HL = address of the element
rt_arr_elem:
        LD      (rt_arr__slot),DE
        LD      A,(BC)
        LD      (rt_arr__nd),A
        EX      DE,HL
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        INC     HL
        LD      A,D
        OR      E
        LD      A,15                    ; array undimensioned
        JP      Z,rt_arr__eerr
        LD      (rt_arr__blk),DE
        LD      A,(rt_arr__nd)
        CP      (HL)
        LD      A,18                    ; wrong number of indices
        JP      NZ,rt_arr__eerr
        LD      HL,0
        LD      (rt_arr__n),HL          ; index
        INC     HL
        LD      (rt_arr__mult),HL       ; multiplier
        LD      A,(rt_arr__nd)
        LD      B,A
        LD      HL,rt_argp
        LD      (rt_arr__ap),HL
        ADD     A,A                     ; the extent of dimension k: block + 4 * (dims - 1 - k)
        ADD     A,A
        LD      E,A
        LD      D,0
        LD      HL,(rt_arr__blk)
        ADD     HL,DE
        LD      (rt_arr__xp),HL         ; after the extents
rt_arr__e1:
        PUSH    BC
        LD      HL,(rt_arr__ap)
        LD      A,(HL)
        INC     HL
        LD      H,(HL)
        LD      L,A
        CALL    rt_arr__minus           ; HL = i - base
        JR      C,rt_arr__erng
        PUSH    HL
        LD      HL,(rt_arr__xp)         ; the extent (16 bit)
        DEC     HL
        DEC     HL
        DEC     HL
        DEC     HL
        LD      (rt_arr__xp),HL
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        POP     HL
        PUSH    HL
        OR      A
        SBC     HL,DE                   ; i - base < extent
        POP     HL
        JR      NC,rt_arr__erng
        PUSH    DE
        LD      BC,(rt_arr__mult)       ; index += (i - base) * multiplier
        CALL    rt_arr__mul
        LD      DE,(rt_arr__n)
        ADD     HL,DE
        LD      (rt_arr__n),HL
        POP     BC                      ; multiplier *= extent
        LD      HL,(rt_arr__mult)
        CALL    rt_arr__mul
        LD      (rt_arr__mult),HL
        LD      HL,(rt_arr__ap)
        INC     HL
        INC     HL
        LD      (rt_arr__ap),HL
        POP     BC
        DJNZ    rt_arr__e1
        LD      HL,(rt_arr__slot)       ; address = block + 4 * dims + index * size
        LD      DE,4
        ADD     HL,DE
        LD      C,(HL)
        LD      B,0
        LD      HL,(rt_arr__n)
        CALL    rt_arr__mul
        LD      A,(rt_arr__nd)
        ADD     A,A
        ADD     A,A
        LD      E,A
        LD      D,0
        ADD     HL,DE
        LD      DE,(rt_arr__blk)
        ADD     HL,DE
        RET
rt_arr__erng:
        POP     BC
        LD      A,16                    ; array index out of range
rt_arr__eerr:
        CALL    rt_error
        LD      HL,rt_arr__dummy        ; if the error returns: a harmless element
        XOR     A
        LD      B,8
rt_arr__ed:
        LD      (HL),A
        INC     HL
        DJNZ    rt_arr__ed
        LD      HL,rt_arr__dummy
        RET

; HL = result, DE = slot -> number of elements (0: not dimensioned)
rt_arr_count:
        PUSH    HL
        CALL    rt_arr__cnt
        POP     DE
        EX      DE,HL
        LD      (HL),E
        INC     HL
        LD      (HL),D
        INC     HL
        LD      (HL),0
        INC     HL
        LD      (HL),0
        RET

; DE = slot -> HL = number of elements
rt_arr__cnt:
        EX      DE,HL
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        INC     HL
        LD      A,D
        OR      E
        JR      NZ,rt_arr__c0
        LD      HL,0
        RET
rt_arr__c0:
        LD      A,(HL)                  ; dimensions
        LD      B,A
        EX      DE,HL                   ; HL = block
        LD      DE,1
rt_arr__c1:
        PUSH    BC
        LD      C,(HL)
        INC     HL
        LD      B,(HL)
        INC     HL
        INC     HL
        INC     HL
        PUSH    HL
        EX      DE,HL
        CALL    rt_arr__mul
        EX      DE,HL
        POP     HL
        POP     BC
        DJNZ    rt_arr__c1
        EX      DE,HL
        RET

; DE = slot, BC -> 1 for string arrays: the array becomes undimensioned
rt_arr_erase:
        LD      (rt_arr__slot),DE
        LD      A,(BC)
        LD      (rt_arr__str),A
        LD      H,D
        LD      L,E
        LD      A,(HL)
        INC     HL
        OR      (HL)
        RET     Z
        CALL    rt_arr__cnt             ; HL = elements
        LD      (rt_arr__n),HL
        LD      HL,(rt_arr__slot)
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        LD      (rt_arr__blk),DE
        INC     HL
        LD      A,(HL)
        ADD     A,A
        ADD     A,A
        LD      (rt_arr__nd),A          ; 4 * dimensions
        INC     HL
        INC     HL
        LD      A,(HL)
        LD      (rt_arr__es),A
        LD      A,(rt_arr__str)         ; string blocks of the elements become garbage
        OR      A
        JR      Z,rt_arr__area
        LD      HL,(rt_arr__blk)
        LD      A,(rt_arr__nd)
        LD      E,A
        LD      D,0
        ADD     HL,DE
        LD      BC,(rt_arr__n)
rt_arr__sf:
        LD      A,B
        OR      C
        JR      Z,rt_arr__area
        PUSH    BC
        PUSH    HL
        CALL    rt_str_free
        POP     HL
        LD      DE,8
        ADD     HL,DE
        POP     BC
        DEC     BC
        JR      rt_arr__sf
rt_arr__area:                           ; the most recent block: give the room back
        LD      HL,(rt_arr__n)
        LD      A,(rt_arr__es)
        LD      C,A
        LD      B,0
        CALL    rt_arr__mul
        LD      A,(rt_arr__nd)
        LD      E,A
        LD      D,0
        ADD     HL,DE
        LD      DE,(rt_arr__blk)
        ADD     HL,DE
        LD      DE,(rt_arr__top)
        OR      A
        SBC     HL,DE
        JR      NZ,rt_arr__keep
        LD      HL,(rt_arr__blk)
        LD      (rt_arr__top),HL
rt_arr__keep:
        LD      HL,(rt_arr__slot)       ; not dimensioned
        LD      B,8
rt_arr__k1:
        LD      (HL),0
        INC     HL
        DJNZ    rt_arr__k1
        RET

; BC = bytes -> HL = free room above the arrays (valid until the next DIM; MAT work storage);
; error 8 if it is too small
rt_arr_work:
        LD      HL,(rt_arr__top)
        LD      A,H
        OR      L
        JR      NZ,rt_arr__w1
        LD      HL,plat_arr
rt_arr__w1:
        PUSH    HL
        ADD     HL,BC
        JR      C,rt_arr__w2
        EX      DE,HL
        LD      HL,plat_arr_end
        OR      A
        SBC     HL,DE
rt_arr__w2:
        POP     HL
        RET     NC
        JP      rt_arr__mem

; DE -> 0 or 1: OPTION BASE
rt_arr_set_base:
        LD      A,(DE)
        LD      (rt_arr_base),A
        RET

; DE, BC = two 8-byte slots: exchange their contents (SWAP of numbers and whole arrays)
rt_swap8:
        LD      H,B
        LD      L,C
        LD      B,8
rt_arr__sw:
        LD      A,(DE)
        LD      C,(HL)
        LD      (HL),A
        LD      A,C
        LD      (DE),A
        INC     HL
        INC     DE
        DJNZ    rt_arr__sw
        RET

;! section data
rt_arr_base:
        DB      0,0,0,0
rt_arr__top:
        DW      0

;! section bss
rt_arr__slot:
        DS      2
rt_arr__blk:
        DS      2
rt_arr__nd:
        DS      1
rt_arr__es:
        DS      1
rt_arr__str:
        DS      1
rt_arr__n:
        DS      2
rt_arr__mult:
        DS      2
rt_arr__ap:
        DS      2
rt_arr__xp:
        DS      2
rt_arr__size:
        DS      2
rt_arr__ext:
        DS      12
rt_arr__dummy:
        DS      8
