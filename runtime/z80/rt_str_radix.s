;! module    rt_str_radix
;! provides  rt_str_radix
;! requires  rt_str_newtemp rt_argp
;! group     strings
;! clobbers  AF BC DE HL
;! doc       HEX$, BIN$, OCT$ with an optional width (zeros in front, or digits cut off at the front).

; DE -> value (32 bits, unsigned digits), BC -> base (2, 8, 16), rt_argp -> width (<= 0: as needed;
; above 32767: 32767) -> HL = temporary
rt_str_radix:
        EX      DE,HL                   ; the value into rt_str_radix__v
        LD      DE,rt_str_radix__v
        PUSH    BC
        LD      BC,4
        LDIR
        POP     BC
        LD      A,(BC)                  ; shift per digit: 1, 3 or 4
        LD      E,4
        CP      16
        JR      Z,rt_str_radix__s
        LD      E,3
        CP      8
        JR      Z,rt_str_radix__s
        LD      E,1
rt_str_radix__s:
        DEC     A
        LD      D,A                     ; D = mask, E = shift
        LD      HL,rt_str_radix__buf+32 ; digits from the end of the buffer
        LD      C,0                     ; C = digit count
rt_str_radix__dig:
        LD      A,(rt_str_radix__v)
        AND     D
        PUSH    HL
        PUSH    DE
        LD      E,A
        LD      D,0
        LD      HL,rt_str_radix__digits
        ADD     HL,DE
        LD      A,(HL)
        POP     DE
        POP     HL
        DEC     HL
        LD      (HL),A
        INC     C
        LD      B,E                     ; v >>= shift
rt_str_radix__sh:
        PUSH    HL
        LD      HL,rt_str_radix__v+3
        SRL     (HL)
        DEC     HL
        RR      (HL)
        DEC     HL
        RR      (HL)
        DEC     HL
        RR      (HL)
        POP     HL
        DJNZ    rt_str_radix__sh
        PUSH    HL
        LD      HL,(rt_str_radix__v)
        LD      A,H
        OR      L
        LD      HL,(rt_str_radix__v+2)
        OR      H
        OR      L
        POP     HL
        JR      NZ,rt_str_radix__dig
        LD      (rt_str_radix__p),HL    ; first digit
        LD      A,C
        LD      (rt_str_radix__n),A
        LD      HL,(rt_argp)            ; width
        INC     HL
        INC     HL
        INC     HL
        BIT     7,(HL)
        JR      NZ,rt_str_radix__nat    ; negative
        LD      A,(HL)
        DEC     HL
        OR      (HL)
        DEC     HL
        LD      B,(HL)
        DEC     HL
        LD      C,(HL)                  ; BC = low 16 bits
        JR      NZ,rt_str_radix__big
        BIT     7,B
        JR      Z,rt_str_radix__w
rt_str_radix__big:
        LD      BC,32767
rt_str_radix__w:
        LD      A,B
        OR      C
        JR      NZ,rt_str_radix__wid
rt_str_radix__nat:
        LD      A,(rt_str_radix__n)
        LD      C,A
        LD      B,0
rt_str_radix__wid:                      ; BC = length of the result
        PUSH    BC
        CALL    rt_str_newtemp          ; HL = descriptor, DE = characters
        POP     BC
        PUSH    HL
        LD      A,(rt_str_radix__n)     ; zeros in front while length > digit count
rt_str_radix__z:
        LD      H,B
        LD      L,C
        PUSH    DE
        LD      E,A
        LD      D,0
        OR      A
        SBC     HL,DE
        POP     DE
        JR      Z,rt_str_radix__cp
        JR      C,rt_str_radix__cut
        LD      L,A
        LD      A,'0'
        LD      (DE),A
        INC     DE
        LD      A,L
        DEC     BC
        JR      rt_str_radix__z
rt_str_radix__cut:                      ; fewer than the digits: the last BC of them
        LD      HL,rt_str_radix__buf+32
        OR      A
        SBC     HL,BC
        JR      rt_str_radix__c1
rt_str_radix__cp:
        LD      HL,(rt_str_radix__p)
rt_str_radix__c1:
        LD      A,B
        OR      C
        JR      Z,rt_str_radix__end
        LDIR
rt_str_radix__end:
        POP     HL
        RET

;! section data
rt_str_radix__digits:
        DB      "0123456789ABCDEF"

;! section bss
rt_str_radix__v:
        DS      4
rt_str_radix__p:
        DS      2
rt_str_radix__n:
        DS      1
rt_str_radix__buf:
        DS      32
