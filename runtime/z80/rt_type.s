;! module    rt_type
;! provides  rt_type rt_swapptr
;! requires  prg_vtypes prg_vtypes_count rt_swap8 rt_argp rt_error
;! group     memory
;! clobbers  AF BC DE HL
;! doc       TYPE(p) and SWAP *p,a() (probe19): the variable table of the program (address, TYPE number).

; DE -> address -> A = TYPE number of the variable there (probe19 T5), -1 if it is none
rt_type__find:
        EX      DE,HL
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        INC     HL
        LD      A,(HL)
        INC     HL
        OR      (HL)
        LD      A,-1
        RET     NZ                      ; beyond 64 KB
        LD      HL,(prg_vtypes_count)
        LD      B,H
        LD      C,L
        LD      HL,prg_vtypes
rt_type__l:
        LD      A,B
        OR      C
        LD      A,-1
        RET     Z
        LD      A,(HL)
        INC     HL
        CP      E
        JR      NZ,rt_type__n
        LD      A,(HL)
        CP      D
        JR      NZ,rt_type__n
        INC     HL
        LD      A,(HL)
        RET
rt_type__n:
        INC     HL
        INC     HL
        DEC     BC
        JR      rt_type__l

; HL = result, DE -> address: TYPE(p)
rt_type:
        PUSH    HL
        CALL    rt_type__find
        POP     HL
        LD      (HL),A
        RLCA
        SBC     A,A
        INC     HL
        LD      (HL),A
        INC     HL
        LD      (HL),A
        INC     HL
        LD      (HL),A
        RET

; DE -> p, BC -> TYPE number of the array a(), rt_argp = slot of a(): SWAP *p,a(); p must point
; to an array of the same type, else error 68 (var-type mismatch, ours: the original does not check)
rt_swapptr:
        LD      A,(BC)
        LD      (rt_type__want),A
        LD      A,(DE)                  ; the slot p points to (low 16 bits)
        LD      (rt_type__p),A
        INC     DE
        LD      A,(DE)
        LD      (rt_type__p+1),A
        DEC     DE
        CALL    rt_type__find
        LD      HL,rt_type__want
        CP      (HL)
        LD      A,68
        JP      NZ,rt_error
        LD      DE,(rt_type__p)
        LD      BC,(rt_argp)
        JP      rt_swap8

;! section bss
rt_type__want:
        DS      1
rt_type__p:
        DS      2
