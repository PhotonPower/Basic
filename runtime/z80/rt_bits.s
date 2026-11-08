;! module    rt_bits
;! provides  rt_btst rt_bclr rt_bset rt_bchg rt_shift rt_swap rt_word rt_card rt_byte
;! requires  rt_argp
;! group     core
;! clobbers  AF BC DE HL
;! doc       Bit functions: BTST/BCLR/BSET/BCHG, SHL/SHR/ROL/ROR in 8, 16, 32 bits, SWAP, WORD, CARD, BYTE.

; HL = result, DE -> value: the value into rt_bits__v, HL = rt_bits__dst
rt_bits__in:
        LD      (rt_bits__dst),HL
        EX      DE,HL
        LD      DE,rt_bits__v
        PUSH    BC
        LD      BC,4
        LDIR
        POP     BC
        RET

; rt_bits__v -> result
rt_bits__out:
        LD      HL,rt_bits__v
        LD      DE,(rt_bits__dst)
        LD      BC,4
        LDIR
        RET

; BC -> bit number (mod 32): HL = the byte in rt_bits__v, A = its mask
rt_bits__at:
        LD      A,(BC)
        AND     31
        LD      C,A
        RRCA
        RRCA
        RRCA
        AND     3
        LD      E,A
        LD      D,0
        LD      HL,rt_bits__v
        ADD     HL,DE
        LD      A,C
        AND     7
        LD      B,A
        LD      A,1
        RET     Z
rt_bits__m:
        ADD     A,A
        DJNZ    rt_bits__m
        RET

; HL = result, DE -> value, BC -> bit number: -1 if the bit is set, else 0
rt_btst:
        CALL    rt_bits__in
        CALL    rt_bits__at
        AND     (HL)
        JR      Z,rt_bits__t0
        LD      A,&FF
rt_bits__t0:
        LD      HL,(rt_bits__dst)
        LD      B,4
rt_bits__t1:
        LD      (HL),A
        INC     HL
        DJNZ    rt_bits__t1
        RET

rt_bclr:
        CALL    rt_bits__in
        CALL    rt_bits__at
        CPL
        AND     (HL)
        LD      (HL),A
        JR      rt_bits__out
rt_bset:
        CALL    rt_bits__in
        CALL    rt_bits__at
        OR      (HL)
        LD      (HL),A
        JR      rt_bits__out
rt_bchg:
        CALL    rt_bits__in
        CALL    rt_bits__at
        XOR     (HL)
        LD      (HL),A
        JR      rt_bits__out

; HL = result, DE -> value, BC -> count (mod 32), rt_argp -> width (8, 16, 32), -> kind (0 SHL,
; 1 SHR, 2 ROL, 3 ROR): the value in the width (16 bits sign-extended, 8 bits unsigned)
rt_shift:
        CALL    rt_bits__in
        LD      A,(BC)
        AND     31
        LD      (rt_bits__n),A
        LD      HL,(rt_argp)
        LD      A,(HL)
        LD      (rt_bits__w),A
        LD      HL,(rt_argp+2)
        LD      A,(HL)
        LD      (rt_bits__k),A
        CALL    rt_bits__mask
        LD      A,(rt_bits__k)
        CP      2
        JR      NC,rt_bits__rot
        LD      A,(rt_bits__w)          ; shift: count >= width gives 0
        LD      C,A
        LD      A,(rt_bits__n)
        CP      C
        JR      C,rt_bits__sh
        LD      HL,0
        LD      (rt_bits__v),HL
        LD      (rt_bits__v+2),HL
        JR      rt_bits__done
rt_bits__sh:
        OR      A
        JR      Z,rt_bits__done
        LD      B,A
        LD      A,(rt_bits__k)
        OR      A
        JR      NZ,rt_bits__shr
rt_bits__l1:
        CALL    rt_bits__shl1
        DJNZ    rt_bits__l1
        CALL    rt_bits__mask
        JR      rt_bits__done
rt_bits__shr:
        LD      HL,rt_bits__v+3
        OR      A
        RR      (HL)
        DEC     HL
        RR      (HL)
        DEC     HL
        RR      (HL)
        DEC     HL
        RR      (HL)
        DJNZ    rt_bits__shr
        JR      rt_bits__done
rt_bits__rot:
        LD      A,(rt_bits__w)          ; count mod width
        LD      C,A
        LD      A,(rt_bits__n)
rt_bits__r0:
        CP      C
        JR      C,rt_bits__r1
        SUB     C
        JR      rt_bits__r0
rt_bits__r1:
        OR      A
        JR      Z,rt_bits__done
        LD      B,A
        LD      A,(rt_bits__k)
        CP      2
        JR      Z,rt_bits__rl
        LD      A,C                     ; ROR n = ROL (width - n)
        SUB     B
        LD      B,A
rt_bits__rl:
        CALL    rt_bits__shl1           ; CF = bit 31
        SBC     A,A
        LD      C,A                     ; C = &FF if bit 31 left
        LD      A,(rt_bits__w)
        CP      32
        JR      NZ,rt_bits__r8
        LD      A,C
        RLCA                            ; CF = that bit
        JR      rt_bits__rc
rt_bits__r8:                            ; width 8/16: the bit that left is bit 0 of byte width/8
        RRCA
        RRCA
        RRCA
        LD      E,A
        LD      D,0
        LD      HL,rt_bits__v
        ADD     HL,DE
        LD      A,(HL)
        RRCA                            ; CF = that bit
rt_bits__rc:
        JR      NC,rt_bits__r2
        LD      HL,rt_bits__v
        SET     0,(HL)
rt_bits__r2:
        PUSH    BC
        CALL    rt_bits__mask
        POP     BC
        DJNZ    rt_bits__rl
rt_bits__done:
        LD      A,(rt_bits__w)
        CP      16
        JP      NZ,rt_bits__out
        LD      A,(rt_bits__v+1)
        RLCA
        SBC     A,A
        LD      (rt_bits__v+2),A
        LD      (rt_bits__v+3),A
        JP      rt_bits__out

; rt_bits__v <<= 1, CF = bit 31
rt_bits__shl1:
        LD      HL,rt_bits__v
        OR      A
        RL      (HL)
        INC     HL
        RL      (HL)
        INC     HL
        RL      (HL)
        INC     HL
        RL      (HL)
        RET

; rt_bits__v &= the mask of the width
rt_bits__mask:
        LD      A,(rt_bits__w)
        CP      32
        RET     Z
        XOR     A
        LD      (rt_bits__v+2),A
        LD      (rt_bits__v+3),A
        LD      A,(rt_bits__w)
        CP      16
        RET     Z
        XOR     A
        LD      (rt_bits__v+1),A
        RET

; HL = result, DE -> value: the two 16-bit halves swapped
rt_swap:
        CALL    rt_bits__in
        LD      HL,(rt_bits__v)
        LD      DE,(rt_bits__v+2)
        LD      (rt_bits__v),DE
        LD      (rt_bits__v+2),HL
        JP      rt_bits__out
; the low 16 bits sign-extended
rt_word:
        CALL    rt_bits__in
        LD      A,16
        LD      (rt_bits__w),A
        JR      rt_bits__done
; the low 16 bits
rt_card:
        CALL    rt_bits__in
        LD      HL,0
        LD      (rt_bits__v+2),HL
        JP      rt_bits__out
; the low 8 bits
rt_byte:
        CALL    rt_bits__in
        LD      HL,0
        LD      (rt_bits__v+2),HL
        LD      A,H
        LD      (rt_bits__v+1),A
        JP      rt_bits__out

;! section bss
rt_bits__dst:
        DS      2
rt_bits__v:
        DS      4
rt_bits__n:
        DS      1
rt_bits__w:
        DS      1
rt_bits__k:
        DS      1
