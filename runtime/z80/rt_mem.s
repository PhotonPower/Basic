;! module    rt_mem
;! provides  rt_mem_chars rt_peek rt_dpeek rt_lpeek rt_poke rt_dpoke rt_lpoke rt_malloc rt_mfree rt_bmove
;! provides  rt_wpeek rt_float_get rt_float_set rt_single_get rt_single_set rt_double_get rt_double_set
;! provides  rt_char_get rt_char_set
;! requires  rt_arr_end rt_arr_top plat_arr plat_arr_end rt_argp rt_mkd rt_cvd rt_mks rt_cvs rt_str_newtemp
;! group     memory
;! clobbers  AF BC DE HL
;! doc       Group memory: PEEK/POKE (8/16/32 bit, low byte first), BYTE{} ... CHAR{}, V: of strings, MALLOC/MFREE, BMOVE.

; Addresses are the low 16 bits of the 32-bit argument. MALLOC blocks lie below the end of the
; array area, one after the other downwards; rt_arr_end (the limit for DIM) is the lowest of them.
; A freed block in between is only reused when all blocks below it are freed too.

rt_mem__MAX EQU 32                      ; MALLOC blocks at the same time

; HL = result, DE = string descriptor: V:a$ (the address of the characters)
rt_mem_chars:
        EX      DE,HL
        LD      A,(HL)
        INC     HL
        LD      H,(HL)
        LD      L,A
        EX      DE,HL
        JP      rt_mem__w16

; DE -> address -> DE = address (16 bit); keeps HL and BC
rt_mem__adr:
        EX      DE,HL
        LD      A,(HL)
        INC     HL
        LD      H,(HL)
        LD      L,A
        EX      DE,HL
        RET

; HL = result, DE -> address: PEEK (0..255), DPEEK (0..65535), INT{} (signed), LPEEK
rt_peek:
        CALL    rt_mem__adr
        LD      A,(DE)
        LD      E,A
        LD      D,0
rt_mem__w16:                            ; (HL) = DE zero-extended
        LD      (HL),E
        INC     HL
        LD      (HL),D
        INC     HL
        LD      (HL),0
        INC     HL
        LD      (HL),0
        RET
rt_dpeek:
        CALL    rt_mem__adr
        EX      DE,HL
        LD      A,(HL)
        INC     HL
        LD      H,(HL)
        LD      L,A
        EX      DE,HL
        JP      rt_mem__w16
rt_wpeek:
        CALL    rt_dpeek                ; HL -> byte 3 of the result
        DEC     HL
        DEC     HL
        LD      A,(HL)
        RLCA
        SBC     A,A
        INC     HL
        LD      (HL),A
        INC     HL
        LD      (HL),A
        RET
rt_lpeek:
        CALL    rt_mem__adr
        EX      DE,HL
        LD      BC,4
        LDIR
        RET

; DE -> address, BC -> value: POKE, DPOKE, LPOKE
rt_poke:
        LD      A,1
        JR      rt_mem__pk
rt_dpoke:
        LD      A,2
        JR      rt_mem__pk
rt_lpoke:
        LD      A,4
rt_mem__pk:
        PUSH    AF
        CALL    rt_mem__adr
        POP     AF
        LD      H,B
        LD      L,C
        LD      C,A
        LD      B,0
        LDIR
        RET

; HL = result, DE -> size, BC -> flags: MALLOC: the address of a cleared block, 0 if there is none
rt_malloc:
        PUSH    HL
        EX      DE,HL
        LD      C,(HL)
        INC     HL
        LD      B,(HL)
        INC     HL
        LD      A,(HL)
        INC     HL
        OR      (HL)
        JR      NZ,rt_mem__none         ; negative or 64 KB and more
        LD      A,B
        OR      C
        JR      Z,rt_mem__none
        LD      (rt_mem__sz),BC
        LD      HL,rt_mem__tab          ; a free entry
        LD      B,rt_mem__MAX
rt_mem__f:
        LD      A,(HL)
        INC     HL
        OR      (HL)
        DEC     HL
        JR      Z,rt_mem__fe
        INC     HL
        INC     HL
        INC     HL
        INC     HL
        DJNZ    rt_mem__f
        JR      rt_mem__none
rt_mem__fe:
        LD      (rt_mem__ent),HL
        LD      BC,(rt_mem__sz)
rt_mem__got:                            ; BC = size: new block = rt_arr_end - size >= top of the arrays
        LD      HL,(rt_arr_end)
        OR      A
        SBC     HL,BC
        JR      C,rt_mem__none
        PUSH    HL
        EX      DE,HL                   ; DE = new block
        LD      HL,(rt_arr_top)
        LD      A,H
        OR      L
        JR      NZ,rt_mem__g1
        LD      HL,plat_arr
rt_mem__g1:
        EX      DE,HL                   ; HL = new block, DE = top of the arrays
        OR      A
        SBC     HL,DE
        POP     HL
        JR      C,rt_mem__none
        LD      (rt_arr_end),HL
        LD      DE,(rt_mem__ent)        ; the entry: address, size
        EX      DE,HL
        LD      (HL),E
        INC     HL
        LD      (HL),D
        INC     HL
        LD      (HL),C
        INC     HL
        LD      (HL),B
        EX      DE,HL                   ; clear the block
        PUSH    HL
        LD      (HL),0
        DEC     BC
        LD      A,B
        OR      C
        JR      Z,rt_mem__g2
        LD      D,H
        LD      E,L
        INC     DE
        LDIR
rt_mem__g2:
        POP     DE
        POP     HL
        JP      rt_mem__w16
rt_mem__none:
        POP     HL
        LD      DE,0
        JP      rt_mem__w16

; HL = result, DE -> address, BC -> size: MFREE: 0, or -1 if it is no MALLOC block
rt_mfree:
        PUSH    HL
        EX      DE,HL
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        INC     HL
        LD      A,(HL)
        INC     HL
        OR      (HL)
        JR      NZ,rt_mem__bad
        LD      A,D
        OR      E
        JR      Z,rt_mem__bad
        LD      HL,rt_mem__tab
        LD      B,rt_mem__MAX
rt_mem__m1:
        LD      A,(HL)
        INC     HL
        CP      E
        JR      NZ,rt_mem__m2
        LD      A,(HL)
        CP      D
        JR      Z,rt_mem__m3
rt_mem__m2:
        INC     HL
        INC     HL
        INC     HL
        DJNZ    rt_mem__m1
rt_mem__bad:
        POP     HL
        LD      DE,-1
        LD      (HL),E
        INC     HL
        LD      (HL),D
        INC     HL
        LD      (HL),E
        INC     HL
        LD      (HL),D
        RET
rt_mem__m3:
        LD      (HL),0                  ; free the entry
        DEC     HL
        LD      (HL),0
        LD      DE,plat_arr_end         ; the new limit: the lowest block still used
        LD      HL,rt_mem__tab
        LD      B,rt_mem__MAX
rt_mem__m4:
        LD      A,(HL)
        INC     HL
        PUSH    HL
        LD      H,(HL)
        LD      L,A
        OR      H
        JR      Z,rt_mem__m5
        OR      A
        SBC     HL,DE
        ADD     HL,DE
        JR      NC,rt_mem__m5
        EX      DE,HL
rt_mem__m5:
        POP     HL
        INC     HL
        INC     HL
        INC     HL
        DJNZ    rt_mem__m4
        LD      (rt_arr_end),DE
        POP     HL
        LD      DE,0
        JP      rt_mem__w16

; DE -> from, BC -> to, rt_argp -> count: BMOVE (overlapping allowed)
rt_bmove:
        CALL    rt_mem__adr
        PUSH    DE
        LD      D,B
        LD      E,C
        CALL    rt_mem__adr
        LD      HL,(rt_argp)            ; BC = count (nothing if <= 0)
        LD      C,(HL)
        INC     HL
        LD      B,(HL)
        INC     HL
        LD      A,(HL)
        INC     HL
        OR      (HL)
        POP     HL                      ; HL = from, DE = to
        RET     NZ
        LD      A,B
        OR      C
        RET     Z
        PUSH    HL
        OR      A
        SBC     HL,DE
        POP     HL
        JR      C,rt_mem__bw
        LDIR                            ; to below from: forwards
        RET
rt_mem__bw:
        ADD     HL,BC                   ; to above from: backwards
        DEC     HL
        EX      DE,HL
        ADD     HL,BC
        DEC     HL
        EX      DE,HL
        LDDR
        RET

; DE -> address, A = count -> DE = a descriptor of that many bytes there (for CVD/CVS); keeps HL
rt_mem__desc:
        PUSH    AF
        CALL    rt_mem__adr
        POP     AF
        LD      (rt_mem__d),DE
        LD      (rt_mem__d+2),A
        XOR     A
        LD      (rt_mem__d+3),A
        LD      DE,rt_mem__d
        RET

; HL = result, DE -> address: FLOAT{} (GFA 8-byte format), SINGLE{} (IEEE single, big-endian as
; MKS$), DOUBLE{} (the bytes of a # variable)
rt_float_get:
        LD      A,8
        CALL    rt_mem__desc
        JP      rt_cvd
rt_single_get:
        LD      A,4
        CALL    rt_mem__desc
        JP      rt_cvs
rt_double_get:
        CALL    rt_mem__adr
        EX      DE,HL
        LD      BC,8
        LDIR
        RET

; DE -> address, BC -> value
rt_float_set:
        CALL    rt_mem__adr
        PUSH    DE
        LD      D,B
        LD      E,C
        CALL    rt_mkd                  ; HL = temporary with the 8 bytes
        LD      BC,8
        JR      rt_mem__put
rt_single_set:
        CALL    rt_mem__adr
        PUSH    DE
        LD      D,B
        LD      E,C
        CALL    rt_mks
        LD      BC,4
rt_mem__put:                            ; (SP) = address, HL = descriptor, BC = count
        LD      A,(HL)
        INC     HL
        LD      H,(HL)
        LD      L,A
        POP     DE
        LDIR
        RET
rt_double_set:
        CALL    rt_mem__adr
        LD      H,B
        LD      L,C
        LD      BC,8
        LDIR
        RET

; DE -> address -> HL = temporary with the characters up to the zero byte (at most 32767)
rt_char_get:
        CALL    rt_mem__adr
        PUSH    DE
        LD      H,D
        LD      L,E
        LD      BC,0
rt_mem__cn:
        LD      A,(HL)
        OR      A
        JR      Z,rt_mem__cl
        INC     HL
        INC     BC
        BIT     7,B
        JR      Z,rt_mem__cn
        DEC     BC
rt_mem__cl:
        PUSH    BC
        CALL    rt_str_newtemp          ; HL = descriptor, DE = characters
        POP     BC
        EX      (SP),HL                 ; HL = source
        LD      A,B
        OR      C
        JR      Z,rt_mem__c0
        LDIR
rt_mem__c0:
        POP     HL
        RET

; DE -> address, BC = descriptor: CHAR{a}=s$ (the characters and a zero byte)
rt_char_set:
        CALL    rt_mem__adr
        LD      H,B
        LD      L,C
        LD      A,(HL)
        INC     HL
        PUSH    AF
        LD      A,(HL)
        INC     HL
        LD      C,(HL)
        INC     HL
        LD      B,(HL)
        LD      H,A
        POP     AF
        LD      L,A                     ; HL = characters, BC = length
        LD      A,B
        OR      C
        JR      Z,rt_mem__s0
        LDIR
rt_mem__s0:
        XOR     A
        LD      (DE),A
        RET

;! section bss
rt_mem__tab:                            ; per block: address (0: free), size
        DS      4*rt_mem__MAX
rt_mem__ent:
        DS      2
rt_mem__sz:
        DS      2
rt_mem__d:
        DS      4
