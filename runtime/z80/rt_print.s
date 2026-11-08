;! module    rt_print
;! provides  rt_print_str rt_print_int rt_print_nl rt_print_comma rt_print_apos rt_print_tab rt_print_spc rt_print_begin rt_print_out rt_print_col rt_pos
;! requires  plat_putc
;! group     print
;! clobbers  AF BC DE HL
;! doc       PRINT: strings, integers, separators; the column since the last line feed (POS) and the zone of ',' (as runtime/m68k).

; rt_print_str: DE = text, BC = length. rt_print_int, rt_print_spc, rt_print_tab: DE -> 32-bit value.
; Every character goes to the routine in rt_print_out (plat_putc); A = character, it keeps all registers.

; DE = text, BC = length
rt_print_str:
        LD      A,B
        OR      C
        RET     Z
        LD      A,(DE)
        CALL    rt_print__putc
        INC     DE
        DEC     BC
        JR      rt_print_str

rt_print_nl:
        LD      A,10
        JP      rt_print__putc

rt_print_apos:
        LD      A,' '
        JP      rt_print__putc

rt_print_begin:
        LD      HL,0
        LD      (rt_print__zone),HL
        RET

; ',': spaces up to the next multiple of 16 characters of this PRINT statement
rt_print_comma:
        LD      A,(rt_print__zone)
        AND     15
        LD      B,A
        LD      A,16
        SUB     B
        LD      B,A
rt_print__sp:
        LD      A,' '
        CALL    rt_print__putc
        DJNZ    rt_print__sp
        RET

; DE -> n: n spaces (n <= 0: none)
rt_print_spc:
        EX      DE,HL
        LD      C,(HL)
        INC     HL
        LD      B,(HL)
        INC     HL
        LD      A,(HL)
        INC     HL
        OR      (HL)
        RET     NZ                      ; >= 65536 or negative: not more than the high bytes allow
        BIT     7,B
        RET     NZ
rt_print__spl:
        LD      A,B
        OR      C
        RET     Z
        LD      A,' '
        CALL    rt_print__putc
        DEC     BC
        JR      rt_print__spl

; DE -> n: TAB(n): spaces up to column n; a new line first if the cursor is beyond it
rt_print_tab:
        EX      DE,HL
        LD      E,(HL)
        INC     HL
        LD      D,(HL)                  ; DE = n (16 bit)
        LD      HL,(rt_print_col)
        OR      A
        SBC     HL,DE
        JR      C,rt_print__tp
        JR      Z,rt_print__tp
        CALL    rt_print_nl
rt_print__tp:
        LD      HL,(rt_print_col)
        OR      A
        SBC     HL,DE
        RET     NC
        LD      A,' '
        CALL    rt_print__putc
        JR      rt_print__tp

; DE -> 32-bit integer: its decimal digits, "-" in front of a negative value
rt_print_int:
        EX      DE,HL
        LD      DE,rt_print__num
        LD      BC,4
        LDIR
        LD      A,(rt_print__num+3)
        BIT     7,A
        JR      Z,rt_print__pos
        LD      HL,rt_print__num        ; 0 - n (-2^31 stays as unsigned 2^31)
        LD      B,4
        OR      A
rt_print__ng:
        LD      A,0
        SBC     A,(HL)
        LD      (HL),A
        INC     HL
        DJNZ    rt_print__ng
        LD      A,'-'
        CALL    rt_print__putc
rt_print__pos:
        LD      HL,rt_print__dig+10     ; digits from the right
        LD      (rt_print__dp),HL
rt_print__dv:
        LD      HL,rt_print__num+3      ; num /= 10, the remainder in A
        LD      B,4
        XOR     A
rt_print__d1:
        LD      C,(HL)                  ; A:C / 10 -> quotient byte, remainder A
        PUSH    BC
        LD      B,8
        LD      E,C
rt_print__d2:                           ; 16/8 division by shifting: A = rest, E = bits
        SLA     E
        RLA
        CP      10
        JR      C,rt_print__d3
        SUB     10
        INC     E
rt_print__d3:
        DJNZ    rt_print__d2
        LD      (HL),E
        POP     BC
        DEC     HL
        DJNZ    rt_print__d1
        ADD     A,'0'
        LD      HL,(rt_print__dp)
        DEC     HL
        LD      (HL),A
        LD      (rt_print__dp),HL
        LD      HL,rt_print__num
        LD      A,(HL)
        INC     HL
        OR      (HL)
        INC     HL
        OR      (HL)
        INC     HL
        OR      (HL)
        JR      NZ,rt_print__dv
        LD      HL,(rt_print__dp)       ; print them
rt_print__pr:
        LD      DE,rt_print__dig+10
        OR      A
        PUSH    HL
        SBC     HL,DE
        POP     HL
        RET     Z
        LD      A,(HL)
        CALL    rt_print__putc
        INC     HL
        JR      rt_print__pr

; -> HL -> POS: the column as 32 bits at (HL)
rt_pos:
        LD      DE,(rt_print_col)
        LD      (HL),E
        INC     HL
        LD      (HL),D
        INC     HL
        LD      (HL),0
        INC     HL
        LD      (HL),0
        RET

; A = character: counts the column and the zone, then to the output (all registers are kept)
rt_print__putc:
        PUSH    HL
        CP      10
        JR      Z,rt_print__lf
        CP      8
        JR      Z,rt_print__bs
        LD      HL,(rt_print_col)
        INC     HL
        LD      (rt_print_col),HL
        LD      HL,(rt_print__zone)
        INC     HL
        LD      (rt_print__zone),HL
        JR      rt_print__out
rt_print__lf:
        LD      HL,0
        LD      (rt_print_col),HL
        LD      (rt_print__zone),HL
        JR      rt_print__out
rt_print__bs:
        LD      HL,(rt_print_col)
        LD      A,H
        OR      L
        JR      Z,rt_print__b1
        DEC     HL
        LD      (rt_print_col),HL
rt_print__b1:
        LD      HL,(rt_print__zone)
        LD      A,H
        OR      L
        JR      Z,rt_print__b2
        DEC     HL
        LD      (rt_print__zone),HL
rt_print__b2:
        LD      A,8
rt_print__out:
        LD      HL,(rt_print_out)       ; jump to the output routine (registers unchanged)
        EX      (SP),HL
        RET

;! section data
rt_print_out:
        DW      plat_putc
rt_print_col:
        DW      0
rt_print__zone:
        DW      0

;! section bss
rt_print__num:
        DS      4
rt_print__dp:
        DS      2
rt_print__dig:
        DS      10
