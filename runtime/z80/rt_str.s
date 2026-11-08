;! module    rt_str
;! provides  rt_str_alloc rt_str_gc rt_str_newtemp rt_str_mark rt_str_release rt_str_assign rt_str_free rt_str_relink
;! provides  rt_str_set_ret rt_str_take_ret rt_print_sdesc rt_str_ret rt_str_swap rt_str_relx
;! requires  rt_error rt_print_str plat_heap plat_heap_end
;! group     string
;! clobbers  AF BC DE HL
;! doc       String heap (D18): descriptors, blocks with a back reference, compaction, temporaries.

; Descriptor (4 bytes): +0 address of the characters (0: empty), +2 length. Block in the heap: +0
; owner (descriptor address, 0: garbage), +2 length, +4 characters, then a zero byte. Temporaries in rt_str__temps
; with stack discipline.

rt_str__NTEMP EQU 64

; BC = length (1..32767) -> HL = characters of a new block (owner 0); error 8 if there is no room
rt_str_alloc:
        LD      (rt_str__len),BC
        CALL    rt_str__room
        JR      NC,rt_str__ok
        CALL    rt_str_gc
        CALL    rt_str__room
        JR      NC,rt_str__ok
        LD      A,8                     ; out of memory
        CALL    rt_error
        LD      HL,0
        RET
rt_str__ok:
        LD      HL,(rt_str__top)        ; block: owner 0, length, then the characters
        LD      (HL),0
        INC     HL
        LD      (HL),0
        INC     HL
        LD      BC,(rt_str__len)
        LD      (HL),C
        INC     HL
        LD      (HL),B
        INC     HL
        PUSH    HL
        ADD     HL,BC
        LD      (HL),0                  ; a zero byte after the characters (CHAR{V:a$})
        INC     HL
        LD      (rt_str__top),HL
        POP     HL
        RET

; CF clear if a block for rt_str__len characters fits at the top
rt_str__room:
        LD      HL,(rt_str__top)
        LD      A,H
        OR      L
        JR      NZ,rt_str__r1
        LD      HL,plat_heap            ; first use
        LD      (rt_str__top),HL
rt_str__r1:
        LD      BC,(rt_str__len)
        ADD     HL,BC
        RET     C
        LD      BC,5
        ADD     HL,BC
        RET     C
        EX      DE,HL
        LD      HL,plat_heap_end
        OR      A
        SBC     HL,DE                   ; end - (top + size): CF if it does not fit
        RET

; compaction: all blocks with an owner move down, the owners get the new addresses
rt_str_gc:
        LD      HL,plat_heap            ; HL = source, DE = destination
        LD      D,H
        LD      E,L
rt_str__walk:
        PUSH    DE
        LD      DE,(rt_str__top)
        OR      A
        PUSH    HL
        SBC     HL,DE
        POP     HL
        POP     DE
        JR      NC,rt_str__done         ; source >= top
        PUSH    HL                      ; BC = size of the block (length + 5)
        INC     HL
        INC     HL
        LD      C,(HL)
        INC     HL
        LD      B,(HL)
        POP     HL
        INC     BC
        INC     BC
        INC     BC
        INC     BC
        INC     BC
        LD      A,(HL)
        INC     HL
        OR      (HL)
        DEC     HL
        JR      Z,rt_str__skip
        PUSH    HL                      ; the owner gets destination + 4
        PUSH    DE
        LD      A,(HL)
        INC     HL
        LD      H,(HL)
        LD      L,A                     ; HL = owner descriptor
        INC     DE
        INC     DE
        INC     DE
        INC     DE
        LD      (HL),E
        INC     HL
        LD      (HL),D
        POP     DE
        POP     HL
        LDIR                            ; the block down (DE <= HL)
        JR      rt_str__walk
rt_str__skip:
        ADD     HL,BC
        JR      rt_str__walk
rt_str__done:
        LD      (rt_str__top),DE
        RET

; HL = descriptor: its block becomes garbage if it owns one (the descriptor is not changed)
rt_str_free:
        CALL    rt_str__block           ; DE = block, Z: none or not owned
        RET     Z
        XOR     A
        LD      (DE),A
        INC     DE
        LD      (DE),A
        RET

; HL = descriptor: the back reference of its block points to it
rt_str_relink:
        PUSH    HL
        LD      A,(HL)
        INC     HL
        LD      H,(HL)
        LD      L,A
        CALL    rt_str__inheap
        POP     DE                      ; DE = descriptor
        RET     C
        DEC     HL
        DEC     HL
        DEC     HL
        DEC     HL
        LD      (HL),E
        INC     HL
        LD      (HL),D
        RET

; HL = descriptor -> DE = its block if it lies in the heap and is owned by it (NZ), else Z
rt_str__block:
        PUSH    HL
        LD      A,(HL)
        INC     HL
        LD      H,(HL)
        LD      L,A
        CALL    rt_str__inheap
        POP     BC                      ; BC = descriptor
        JR      C,rt_str__nob
        DEC     HL
        DEC     HL
        DEC     HL
        DEC     HL
        LD      D,H
        LD      E,L
        LD      A,(HL)                  ; owner = descriptor?
        CP      C
        JR      NZ,rt_str__nob
        INC     HL
        LD      A,(HL)
        CP      B
        JR      NZ,rt_str__nob
        OR      1                       ; NZ
        RET
rt_str__nob:
        XOR     A                       ; Z
        RET

; HL = address -> CF clear if it lies in plat_heap .. plat_heap_end (and is not 0)
rt_str__inheap:
        PUSH    DE
        EX      DE,HL
        LD      HL,plat_heap-1
        OR      A
        SBC     HL,DE                   ; heap-1 - a: CF if a >= heap
        CCF
        JR      C,rt_str__ih            ; below
        LD      HL,plat_heap_end-1
        OR      A
        SBC     HL,DE                   ; CF if a > end-1
rt_str__ih:
        EX      DE,HL
        POP     DE
        RET

; BC = length -> HL = new temporary descriptor owning a new block, DE = its characters
rt_str_newtemp:
        LD      (rt_str__nl),BC
        LD      A,(rt_str__ttop)
        CP      rt_str__NTEMP
        JR      C,rt_str__nt
        LD      A,8
        CALL    rt_error
        LD      A,rt_str__NTEMP-1
        LD      (rt_str__ttop),A
rt_str__nt:
        LD      L,A
        INC     A
        LD      (rt_str__ttop),A
        LD      H,0
        ADD     HL,HL
        ADD     HL,HL
        LD      DE,rt_str__temps
        ADD     HL,DE                   ; HL = descriptor
        LD      (rt_str__nd),HL
        XOR     A
        LD      (HL),A
        INC     HL
        LD      (HL),A
        INC     HL
        LD      (HL),C
        INC     HL
        LD      (HL),B
        LD      DE,0
        LD      A,B
        OR      C
        JR      Z,rt_str__nt2
        CALL    rt_str_alloc            ; HL = characters
        EX      DE,HL                   ; DE = characters
        LD      HL,(rt_str__nd)
        LD      (HL),E
        INC     HL
        LD      (HL),D
        DEC     HL
        PUSH    DE
        CALL    rt_str_relink
        POP     DE
rt_str__nt2:
        LD      HL,(rt_str__nd)
        RET

; HL = result: the current number of temporaries (32 bits)
rt_str_mark:
        LD      A,(rt_str__ttop)
        LD      (HL),A
        INC     HL
        XOR     A
        LD      (HL),A
        INC     HL
        LD      (HL),A
        INC     HL
        LD      (HL),A
        RET

; compiled code: the mark in the frame slot IX + d (d = the byte after the CALL)
rt_str_relx:
        POP     HL
        LD      A,(HL)
        INC     HL
        PUSH    HL
        PUSH    IX
        POP     DE
        ADD     A,E
        LD      E,A
        JR      C,rt_str_release
        DEC     D

; DE -> mark: release all temporaries above it
rt_str_release:
        LD      A,(DE)
        LD      (rt_str__mk),A
rt_str__rl:
        LD      A,(rt_str__mk)
        LD      C,A
        LD      A,(rt_str__ttop)
        CP      C
        RET     Z
        RET     C
        DEC     A
        LD      (rt_str__ttop),A
        LD      L,A
        LD      H,0
        ADD     HL,HL
        ADD     HL,HL
        LD      DE,rt_str__temps
        ADD     HL,DE
        PUSH    HL
        CALL    rt_str_free
        POP     HL
        XOR     A
        LD      (HL),A
        INC     HL
        LD      (HL),A
        INC     HL
        LD      (HL),A
        INC     HL
        LD      (HL),A
        JR      rt_str__rl

; DE = destination descriptor, BC = source descriptor: dest$ = source$. A temporary that owns its
; block hands it over; variables and constants are copied.
rt_str_assign:
        LD      A,D
        CP      B
        JR      NZ,rt_str__as
        LD      A,E
        CP      C
        RET     Z
rt_str__as:
        LD      (rt_str__dst),DE
        LD      (rt_str__src),BC
        LD      H,B                     ; a temporary that owns its block?
        LD      L,C
        LD      DE,rt_str__temps
        OR      A
        SBC     HL,DE
        JR      C,rt_str__copy
        LD      DE,4*rt_str__NTEMP
        OR      A
        SBC     HL,DE
        JR      NC,rt_str__copy
        LD      HL,(rt_str__src)
        CALL    rt_str__block
        JR      Z,rt_str__copy
        LD      HL,(rt_str__dst)        ; hand over
        CALL    rt_str_free
        LD      HL,(rt_str__src)
        LD      DE,(rt_str__dst)
        LD      BC,4
        LDIR
        LD      HL,(rt_str__dst)
        CALL    rt_str_relink
        LD      HL,(rt_str__src)
        XOR     A
        LD      (HL),A
        INC     HL
        LD      (HL),A
        INC     HL
        LD      (HL),A
        INC     HL
        LD      (HL),A
        RET
rt_str__copy:
        LD      HL,(rt_str__src)
        INC     HL
        INC     HL
        LD      C,(HL)
        INC     HL
        LD      B,(HL)
        LD      A,B
        OR      C
        JR      NZ,rt_str__alloc
        LD      HL,(rt_str__dst)        ; empty
        PUSH    HL
        CALL    rt_str_free
        POP     HL
        XOR     A
        LD      (HL),A
        INC     HL
        LD      (HL),A
        INC     HL
        LD      (HL),A
        INC     HL
        LD      (HL),A
        RET
rt_str__alloc:
        LD      (rt_str__cl),BC
        CALL    rt_str_alloc            ; may move blocks: read the addresses afterwards
        PUSH    HL
        LD      HL,(rt_str__dst)
        CALL    rt_str_free
        POP     DE                      ; DE = new characters
        LD      HL,(rt_str__dst)
        LD      (HL),E
        INC     HL
        LD      (HL),D
        INC     HL
        LD      BC,(rt_str__cl)
        LD      (HL),C
        INC     HL
        LD      (HL),B
        PUSH    DE
        LD      HL,(rt_str__dst)
        CALL    rt_str_relink
        POP     DE
        LD      HL,(rt_str__src)        ; copy the characters
        LD      A,(HL)
        INC     HL
        LD      H,(HL)
        LD      L,A
        LD      BC,(rt_str__cl)
        LDIR
        RET

; DE = descriptor: becomes the result of a string function
rt_str_set_ret:
        LD      B,D
        LD      C,E
        LD      DE,rt_str_ret
        JP      rt_str_assign

; -> HL = new temporary that takes over the result of a string function
rt_str_take_ret:
        LD      BC,0
        CALL    rt_str_newtemp
        PUSH    HL
        EX      DE,HL
        LD      HL,rt_str_ret
        LD      BC,4
        LDIR
        LD      HL,rt_str_ret
        XOR     A
        LD      (HL),A
        INC     HL
        LD      (HL),A
        INC     HL
        LD      (HL),A
        INC     HL
        LD      (HL),A
        POP     HL
        PUSH    HL
        CALL    rt_str_relink
        POP     HL
        RET

; DE, BC = descriptors: exchange the strings (SWAP)
rt_str_swap:
        PUSH    BC
        PUSH    DE
        LD      H,B
        LD      L,C
        LD      B,4
rt_str__sw:
        LD      A,(DE)
        LD      C,(HL)
        LD      (HL),A
        LD      A,C
        LD      (DE),A
        INC     HL
        INC     DE
        DJNZ    rt_str__sw
        POP     HL
        CALL    rt_str_relink
        POP     HL
        JP      rt_str_relink

; DE = descriptor: PRINT
rt_print_sdesc:
        EX      DE,HL
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        INC     HL
        LD      C,(HL)
        INC     HL
        LD      B,(HL)
        JP      rt_print_str

;! section data
rt_str__top:
        DW      0
rt_str__ttop:
        DB      0

;! section bss
rt_str_ret:
        DS      4
rt_str__len:
        DS      2
rt_str__nl:
        DS      2
rt_str__nd:
        DS      2
rt_str__mk:
        DS      1
rt_str__dst:
        DS      2
rt_str__src:
        DS      2
rt_str__cl:
        DS      2
rt_str__temps:
        DS      4*rt_str__NTEMP
