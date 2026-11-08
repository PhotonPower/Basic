;! module    rt_fmt_int
;! provides  rt_fmt_int
;! group     core
;! clobbers  AF BC DE HL
;! doc       A 32-bit integer as decimal text ("-" in front of a negative one), as PRINT and STR$ write it with DEFNUM 13.

; HL = buffer (11 bytes), DE -> 32-bit integer -> BC = length
rt_fmt_int:
        LD      (rt_fmt_int__out),HL
        LD      (rt_fmt_int__beg),HL
        EX      DE,HL
        LD      DE,rt_fmt_int__num
        LD      BC,4
        LDIR
        LD      A,(rt_fmt_int__num+3)
        BIT     7,A
        JR      Z,rt_fmt_int__pos
        LD      HL,rt_fmt_int__num      ; 0 - n (-2^31 stays as unsigned 2^31)
        LD      B,4
        OR      A
rt_fmt_int__ng:
        LD      A,0
        SBC     A,(HL)
        LD      (HL),A
        INC     HL
        DJNZ    rt_fmt_int__ng
        LD      HL,(rt_fmt_int__out)
        LD      (HL),'-'
        INC     HL
        LD      (rt_fmt_int__out),HL
rt_fmt_int__pos:
        LD      HL,rt_fmt_int__dig+10   ; digits from the right
        LD      (rt_fmt_int__dp),HL
rt_fmt_int__dv:
        LD      HL,rt_fmt_int__num+3    ; num /= 10, the remainder in A
        LD      B,4
        XOR     A
rt_fmt_int__d1:
        LD      C,(HL)                  ; A:C / 10 -> quotient byte, remainder A
        PUSH    BC
        LD      B,8
        LD      E,C
rt_fmt_int__d2:                         ; 16/8 division by shifting: A = rest, E = bits
        SLA     E
        RLA
        CP      10
        JR      C,rt_fmt_int__d3
        SUB     10
        INC     E
rt_fmt_int__d3:
        DJNZ    rt_fmt_int__d2
        LD      (HL),E
        POP     BC
        DEC     HL
        DJNZ    rt_fmt_int__d1
        ADD     A,'0'
        LD      HL,(rt_fmt_int__dp)
        DEC     HL
        LD      (HL),A
        LD      (rt_fmt_int__dp),HL
        LD      HL,rt_fmt_int__num
        LD      A,(HL)
        INC     HL
        OR      (HL)
        INC     HL
        OR      (HL)
        INC     HL
        OR      (HL)
        JR      NZ,rt_fmt_int__dv
        LD      HL,rt_fmt_int__dig+10   ; copy the digits behind the sign
        LD      DE,(rt_fmt_int__dp)
        OR      A
        SBC     HL,DE
        LD      B,H
        LD      C,L
        EX      DE,HL
        LD      DE,(rt_fmt_int__out)
        LDIR
        EX      DE,HL                   ; HL = end
        LD      DE,(rt_fmt_int__beg)
        OR      A
        SBC     HL,DE
        LD      B,H
        LD      C,L
        RET

;! section bss
rt_fmt_int__out:
        DS      2
rt_fmt_int__beg:
        DS      2
rt_fmt_int__dp:
        DS      2
rt_fmt_int__num:
        DS      4
rt_fmt_int__dig:
        DS      10
