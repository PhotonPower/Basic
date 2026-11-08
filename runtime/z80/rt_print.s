;! module    rt_print
;! provides  rt_print_str rt_print_int rt_print_nl rt_print_comma rt_print_apos rt_print_tab rt_print_spc rt_print_begin rt_print_out rt_print_col rt_pos
;! provides  rt_print_flt rt_set_defnum rt_defnum
;! provides  rt_print_at rt_print_locate rt_print_htab rt_print_vtab rt_print_cls rt_crscol rt_crslin
;! requires  plat_putc plat_locate plat_cls rt_fmt_flt fp_from_i32
;! group     print
;! clobbers  AF BC DE HL
;! doc       PRINT: strings, integers, separators; the column since the last line feed (POS), the zone of ',' and the cursor (as runtime/m68k).

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

; DE -> 32-bit integer: as PRINT writes a number (with DEFNUM below 13 digits: through the float
; form, 123456 with DEFNUM 4 is 123500); else its decimal digits, "-" in front of a negative value
rt_print_int:
        LD      A,(rt_defnum)
        CP      13
        JR      NC,rt_print__idig
        LD      HL,rt_print__fv
        CALL    fp_from_i32
        LD      DE,rt_print__fv
        JP      rt_print_flt
rt_print__idig:
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

; DE -> binary64: as PRINT writes it (rt_defnum digits)
rt_print_flt:
        LD      HL,rt_print__fbuf
        LD      A,(rt_defnum)
        CALL    rt_fmt_flt
        LD      DE,rt_print__fbuf
        JP      rt_print_str

; DE -> n: DEFNUM n (2..13)
rt_set_defnum:
        EX      DE,HL
        LD      A,(HL)
        INC     HL
        LD      C,(HL)
        INC     HL
        LD      B,(HL)
        INC     HL
        LD      D,(HL)
        BIT     7,D                     ; negative: 2
        JR      NZ,rt_print__dlo
        LD      E,A
        LD      A,C
        OR      B
        OR      D
        LD      A,13
        JR      NZ,rt_print__dset       ; large: 13
        LD      A,E
        CP      2
        JR      C,rt_print__dlo
        CP      14
        JR      C,rt_print__dset
        LD      A,13
        JR      rt_print__dset
rt_print__dlo:
        LD      A,2
rt_print__dset:
        LD      (rt_defnum),A
        RET

; -> HL -> POS, CRSCOL, CRSLIN: the value as 32 bits at (HL)
rt_pos:
        LD      DE,(rt_print_col)
rt_print__w32:
        LD      (HL),E
        INC     HL
        LD      (HL),D
        INC     HL
        LD      (HL),0
        INC     HL
        LD      (HL),0
        RET
rt_crscol:
        LD      DE,(rt_print__ccol)
        JR      rt_print__w32
rt_crslin:
        LD      DE,(rt_print__clin)
        JR      rt_print__w32

; DE -> 32-bit value -> HL = the value limited to 1..32767
rt_print__clamp:
        EX      DE,HL
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        INC     HL
        LD      A,(HL)
        INC     HL
        LD      H,(HL)
        BIT     7,H
        JR      NZ,rt_print__c1
        OR      H
        JR      NZ,rt_print__cmx
        BIT     7,D
        JR      NZ,rt_print__cmx
        LD      A,D
        OR      E
        JR      Z,rt_print__c1
        EX      DE,HL
        RET
rt_print__cmx:
        LD      HL,32767
        RET
rt_print__c1:
        LD      HL,1
        RET

; DE -> column, BC -> line: LOCATE (POS stays); the platform moves the cursor (DE = column, HL = line)
rt_print_locate:
        PUSH    BC
        CALL    rt_print__clamp
        LD      (rt_print__ccol),HL
        POP     DE
        CALL    rt_print__clamp
        LD      (rt_print__clin),HL
rt_print__goto:
        LD      DE,(rt_print__ccol)
        LD      HL,(rt_print__clin)
        JP      plat_locate

; DE -> column, BC -> line: PRINT AT (POS 0)
rt_print_at:
        CALL    rt_print_locate
        LD      HL,0
        LD      (rt_print_col),HL
        RET

; DE -> column: HTAB
rt_print_htab:
        CALL    rt_print__clamp
        LD      (rt_print__ccol),HL
        JR      rt_print__goto

; DE -> line: VTAB
rt_print_vtab:
        CALL    rt_print__clamp
        LD      (rt_print__clin),HL
        JR      rt_print__goto

; CLS: the platform clears the screen and puts the cursor home, POS 0
rt_print_cls:
        LD      HL,1
        LD      (rt_print__ccol),HL
        LD      (rt_print__clin),HL
        DEC     HL
        LD      (rt_print_col),HL
        JP      plat_cls

; -> Z if the output goes to the screen (A is kept)
rt_print__con:
        PUSH    DE
        LD      HL,(rt_print_out)
        LD      DE,plat_putc
        OR      A
        SBC     HL,DE
        POP     DE
        RET

; A = character: counts the column, the zone and on the screen the cursor, then to the output (all
; registers are kept)
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
        CALL    rt_print__con
        JR      NZ,rt_print__out
        LD      HL,(rt_print__ccol)
        INC     HL
        CP      13
        JR      NZ,rt_print__cc
        LD      HL,1
rt_print__cc:
        LD      (rt_print__ccol),HL
        JR      rt_print__out
rt_print__lf:
        LD      HL,0
        LD      (rt_print_col),HL
        LD      (rt_print__zone),HL
        CALL    rt_print__con
        JR      NZ,rt_print__out
        LD      HL,1
        LD      (rt_print__ccol),HL
        LD      HL,(rt_print__clin)
        INC     HL
        LD      (rt_print__clin),HL
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
        CALL    rt_print__con
        JR      NZ,rt_print__b3
        LD      HL,(rt_print__ccol)
        DEC     HL
        LD      A,H
        OR      L
        JR      Z,rt_print__b3
        LD      (rt_print__ccol),HL
rt_print__b3:
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
rt_print__ccol:
        DW      1
rt_print__clin:
        DW      1
rt_defnum:
        DB      13

;! section bss
rt_print__fv:
        DS      8
rt_print__fbuf:
        DS      32
rt_print__num:
        DS      4
rt_print__dp:
        DS      2
rt_print__dig:
        DS      10
