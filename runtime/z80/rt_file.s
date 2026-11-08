;! module    rt_file
;! provides  rt_file_open rt_file_close rt_file_close_all rt_file_select rt_file_deselect rt_file_line rt_file_item rt_file_num
;! provides  rt_file_chars rt_file_eof rt_file_lof rt_file_exist rt_file_kill rt_file_inp rt_file_out
;! requires  rt_error rt_err_hook plat_exit_hook rt_print_out rt_print_col rt_str_newtemp rt_val rt_val_used rt_argp
;! requires  rt_arr_top rt_arr_end plat_arr plat_fopen plat_fgetc plat_fputc plat_fclose plat_fsize plat_fexists plat_fdelete
;! group     file-seq
;! clobbers  AF BC DE HL
;! doc       Sequential files (probe14): channels 0-99, PRINT # through the output vector of rt_print.

; Channels are 32-bit numbers (DE ->), names and modes descriptors. Modes I, O, A (U and R, random
; access, are not supported on the Z80: error 21). Channel record (8 bytes): +0 handle of the
; platform (0: closed), +1 flags (bit 0 readable, bit 1 writable, bit 2 a byte read ahead, bit 3 it
; is the end of the file), +2 that byte, +4 column (PRINT #). The platform buffers; it reads and
; writes single bytes (plat_fgetc, plat_fputc). Errors of the platform are AmigaDOS codes (205
; object not found, 202 object in use; ERR shows them as -51, -54). Text read (LINE INPUT #, INPUT #,
; INPUT$) is collected in the free room above the arrays (up to 32767 characters).

; ---- channels ---------------------------------------------------------------------------------------

; DE -> channel -> HL = record (also in rt_file__rec), CF clear; CF set after error 23
rt_file__chan:
        EX      DE,HL
        LD      E,(HL)
        INC     HL
        LD      A,(HL)
        INC     HL
        OR      (HL)
        INC     HL
        OR      (HL)
        JP      NZ,rt_file__c23
        LD      A,E
        CP      100
        JP      NC,rt_file__c23
        LD      L,A
        LD      H,0
        ADD     HL,HL
        ADD     HL,HL
        ADD     HL,HL
        LD      DE,rt_file__ch
        ADD     HL,DE
        LD      (rt_file__rec),HL
        OR      A
        RET
rt_file__c23:
        LD      A,23                    ; file # wrong
rt_file__err:
        CALL    rt_error
        SCF
        RET

; DE -> channel -> HL = open record, CF set after error 23 or 24
rt_file__open:
        CALL    rt_file__chan
        RET     C
        LD      A,(HL)
        OR      A
        RET     NZ
rt_file__e24:
        LD      A,24                    ; file not open
        JP      rt_file__err

; DE -> channel -> HL = record open for input, CF set after an error (24 for an output file)
rt_file__in:
        CALL    rt_file__open
        RET     C
        INC     HL
        BIT     0,(HL)
        DEC     HL
        RET     NZ
        JP      rt_file__e24

; DE = mode descriptor, BC -> channel, rt_argp = name descriptor, -> record length (not used)
rt_file_open:
        PUSH    DE
        LD      D,B
        LD      E,C
        CALL    rt_file__chan
        POP     HL                      ; HL = mode descriptor
        RET     C
        LD      E,(HL)                  ; DE = characters of the mode
        INC     HL
        LD      D,(HL)
        INC     HL
        LD      A,(HL)
        INC     HL
        OR      (HL)
        JP      Z,rt_file__badm
        LD      A,(DE)                  ; first character, upper case
        AND     &DF
        LD      C,0                     ; C = 0 input, 1 output, 2 append
        CP      'I'
        JP      Z,rt_file__m
        INC     C
        CP      'O'
        JP      Z,rt_file__m
        INC     C
        CP      'A'
        JP      Z,rt_file__m
rt_file__badm:
        LD      A,21                    ; open allows i o r a u
        JP      rt_error
rt_file__m:
        LD      HL,(rt_file__rec)
        LD      A,(HL)
        OR      A
        LD      A,22                    ; file already open
        JP      NZ,rt_error
        LD      A,C
        LD      (rt_file__mode),A
        LD      HL,(rt_argp)            ; the name
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        INC     HL
        LD      C,(HL)
        INC     HL
        LD      B,(HL)
        CALL    plat_fopen
        JP      C,rt_error
        LD      HL,(rt_file__rec)
        LD      (HL),A                  ; handle
        INC     HL
        LD      A,(rt_file__mode)
        OR      A
        LD      A,1                     ; I: readable
        JP      Z,rt_file__fl
        LD      A,2                     ; O, A: writable
rt_file__fl:
        LD      (HL),A
        INC     HL
        INC     HL
        INC     HL
        LD      (HL),0                  ; column 0
        INC     HL
        LD      (HL),0
        LD      HL,rt_file_close_all
        LD      (plat_exit_hook),HL
        LD      HL,rt_file_deselect
        LD      (rt_err_hook),HL
        RET

; DE -> channel: CLOSE #n (a closed channel: nothing)
rt_file_close:
        CALL    rt_file__chan
        RET     C
        LD      A,(HL)
        OR      A
        RET     Z
        LD      DE,(rt_file__cur)       ; still the output of a PRINT #?
        OR      A
        SBC     HL,DE
        ADD     HL,DE
        JP      NZ,rt_file__cl
        PUSH    HL
        CALL    rt_file_deselect
        POP     HL
rt_file__cl:
        LD      A,(HL)
        LD      (HL),0
        INC     HL
        LD      (HL),0
        JP      plat_fclose

; CLOSE: all channels
rt_file_close_all:
        XOR     A
rt_file__ca:
        LD      (rt_file__k),A
        LD      DE,rt_file__k
        CALL    rt_file_close
        LD      A,(rt_file__k)
        INC     A
        CP      100
        JP      C,rt_file__ca
        RET

; ---- PRINT #, OUT # ----------------------------------------------------------------------------------

; DE -> channel: the following output goes to the file (with the channel's column)
rt_file_select:
        CALL    rt_file__open
        RET     C
        INC     HL
        BIT     1,(HL)
        DEC     HL
        JP      Z,rt_file__e24          ; an input file: file not open (for output)
        PUSH    HL
        CALL    rt_file_deselect        ; (an interrupted PRINT # before)
        POP     HL
        LD      (rt_file__cur),HL
        LD      DE,(rt_print_out)
        LD      (rt_file__saveout),DE
        LD      DE,(rt_print_col)
        LD      (rt_file__savecol),DE
        LD      DE,4
        ADD     HL,DE
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        LD      (rt_print_col),DE
        LD      HL,rt_file__putc
        LD      (rt_print_out),HL
        RET

; end of PRINT #: column back to the channel, output to the screen again
rt_file_deselect:
        LD      HL,(rt_file__cur)
        LD      A,H
        OR      L
        RET     Z
        LD      DE,0
        LD      (rt_file__cur),DE
        LD      DE,4
        ADD     HL,DE
        LD      DE,(rt_print_col)
        LD      (HL),E
        INC     HL
        LD      (HL),D
        LD      HL,(rt_file__saveout)
        LD      (rt_print_out),HL
        LD      HL,(rt_file__savecol)
        LD      (rt_print_col),HL
        RET

; A = character to the file of the current PRINT # (keeps all registers)
rt_file__putc:
        PUSH    AF
        PUSH    BC
        PUSH    DE
        PUSH    HL
        LD      HL,(rt_file__cur)
        LD      C,(HL)
        CALL    plat_fputc
        CALL    C,rt_error
        POP     HL
        POP     DE
        POP     BC
        POP     AF
        RET

; DE -> channel, BC -> byte: OUT #n,b
rt_file_out:
        PUSH    BC
        CALL    rt_file__open
        POP     DE
        RET     C
        INC     HL
        BIT     1,(HL)
        DEC     HL
        JP      Z,rt_file__e24
        LD      C,(HL)
        LD      A,(DE)
        CALL    plat_fputc
        JP      C,rt_error
        RET

; ---- reading -----------------------------------------------------------------------------------------

; -> A = next byte of the channel in rt_file__rec, CF set at the end of the file (keeps BC DE HL)
rt_file__getb:
        PUSH    HL
        LD      HL,(rt_file__rec)
        INC     HL
        BIT     2,(HL)
        JP      Z,rt_file__gb
        RES     2,(HL)                  ; the byte read ahead
        BIT     3,(HL)
        INC     HL
        LD      A,(HL)
        POP     HL
        SCF
        RET     NZ
        CCF
        RET
rt_file__gb:
        DEC     HL
        LD      A,(HL)
        PUSH    DE
        PUSH    BC
        CALL    plat_fgetc
        POP     BC
        POP     DE
        POP     HL
        RET

; -> A = next byte without taking it, CF set at the end (keeps BC DE HL)
rt_file__peek:
        CALL    rt_file__getb
        PUSH    HL
        LD      HL,(rt_file__rec)
        INC     HL
        SET     2,(HL)
        RES     3,(HL)
        JP      NC,rt_file__pk
        SET     3,(HL)
rt_file__pk:
        INC     HL
        LD      (HL),A
        POP     HL
        RET

; the collection of characters starts (above the arrays)
rt_file__scr:
        LD      HL,(rt_arr_top)
        LD      A,H
        OR      L
        JP      NZ,rt_file__s1
        LD      HL,plat_arr
rt_file__s1:
        LD      (rt_file__sb),HL
        LD      HL,0
        LD      (rt_file__n),HL
        RET

; A = character: collect it (beyond 32767 characters or the free room: dropped); keeps A BC DE
rt_file__keep:
        PUSH    DE
        LD      E,A
        LD      HL,(rt_file__n)
        LD      A,H
        CP      &7F
        JP      NZ,rt_file__k1
        LD      A,L
        CP      &FF
        JP      Z,rt_file__k9
rt_file__k1:
        PUSH    BC
        LD      BC,(rt_file__sb)
        ADD     HL,BC
        LD      BC,(rt_arr_end)
        PUSH    HL
        OR      A
        SBC     HL,BC
        POP     HL
        POP     BC
        JP      NC,rt_file__k9
        LD      (HL),E
        LD      HL,(rt_file__n)
        INC     HL
        LD      (rt_file__n),HL
rt_file__k9:
        LD      A,E
        POP     DE
        RET

; nothing collected -> HL = ""
rt_file__empty:
        LD      HL,0
        LD      (rt_file__n),HL
; -> HL = temporary string with the collected characters
rt_file__temp:
        LD      BC,(rt_file__n)
        PUSH    BC
        CALL    rt_str_newtemp          ; HL = descriptor, DE = characters
        POP     BC
        LD      A,B
        OR      C
        RET     Z
        PUSH    HL
        LD      HL,(rt_file__sb)
        LDIR
        POP     HL
        RET

; DE -> channel -> HL = the next line without the line feed (LINE INPUT #); error 26 at the end
rt_file_line:
        CALL    rt_file__in
        JP      C,rt_file__empty
        CALL    rt_file__peek
        JP      C,rt_file__l26
        CALL    rt_file__scr
rt_file__ll:
        CALL    rt_file__getb
        JP      C,rt_file__temp
        CP      10
        JP      Z,rt_file__temp
        CALL    rt_file__keep
        JP      rt_file__ll
rt_file__l26:
        LD      A,26                    ; end of file
        CALL    rt_error
        JP      rt_file__empty

; the next INPUT # item collected; CF set after error 26. Up to a comma or the line end; in quotes
; everything up to the closing quote (then on to the comma); unquoted text keeps its spaces (probe14 T4)
rt_file__item0:
        CALL    rt_file__peek
        JP      NC,rt_file__go
        LD      A,26                    ; end of file
        JP      rt_file__err
rt_file__go:
        CALL    rt_file__scr
        LD      BC,0                    ; BC = leading spaces
rt_file__sp:
        CALL    rt_file__getb
        JP      C,rt_file__plain
        CP      ' '
        JP      NZ,rt_file__nsp
        INC     BC
        JP      rt_file__sp
rt_file__nsp:
        CP      '"'
        JP      NZ,rt_file__plain
rt_file__q:                             ; quoted
        CALL    rt_file__getb
        JP      C,rt_file__end
        CP      '"'
        JP      Z,rt_file__skip
        CP      10
        JP      Z,rt_file__end
        CALL    rt_file__keep
        JP      rt_file__q
rt_file__skip:                          ; on to the comma or the line end
        CALL    rt_file__getb
        JP      C,rt_file__end
        CP      ','
        JP      Z,rt_file__end
        CP      13
        JP      Z,rt_file__cr
        CP      10
        JP      NZ,rt_file__skip
        JP      rt_file__end
rt_file__plain:                         ; the leading spaces belong to the item (CF: end of the file)
        PUSH    AF
rt_file__lp:
        LD      A,B
        OR      C
        JP      Z,rt_file__ld
        LD      A,' '
        CALL    rt_file__keep
        DEC     BC
        JP      rt_file__lp
rt_file__ld:
        POP     AF
rt_file__pl:
        JP      C,rt_file__end
        CP      ','
        JP      Z,rt_file__end
        CP      13
        JP      Z,rt_file__cr
        CP      10
        JP      Z,rt_file__end
        CALL    rt_file__keep
        CALL    rt_file__getb
        JP      rt_file__pl
rt_file__cr:                            ; CR ends the item; a LF right after it belongs to it (probe18)
        CALL    rt_file__peek
        JP      C,rt_file__end
        CP      10
        JP      NZ,rt_file__end
        CALL    rt_file__getb
rt_file__end:
        OR      A
        RET

; DE -> channel -> HL = the next item as a string (INPUT # into a string variable)
rt_file_item:
        CALL    rt_file__in
        JP      C,rt_file__empty
        CALL    rt_file__item0
        JP      C,rt_file__empty
        JP      rt_file__temp

; HL = result (binary64), DE -> channel: the next item as a number (spaces around it allowed);
; error 35 if it is none
rt_file_num:
        LD      (rt_file__res),HL
        CALL    rt_file__in
        JP      C,rt_file__zero
        CALL    rt_file__item0
        JP      C,rt_file__zero
        LD      BC,(rt_file__n)
        LD      HL,(rt_file__sb)
rt_file__tl:                            ; spaces at the end away
        LD      A,B
        OR      C
        JP      Z,rt_file__bad
        PUSH    HL
        ADD     HL,BC
        DEC     HL
        LD      A,(HL)
        POP     HL
        CP      ' '
        JP      NZ,rt_file__lead
        DEC     BC
        JP      rt_file__tl
rt_file__lead:                          ; and at the front
        LD      A,(HL)
        CP      ' '
        JP      NZ,rt_file__val
        INC     HL
        DEC     BC
        JP      rt_file__lead
rt_file__val:
        PUSH    BC
        EX      DE,HL
        LD      HL,(rt_file__res)
        CALL    rt_val
        POP     BC
        LD      HL,(rt_val_used)        ; all characters must belong to the number
        OR      A
        SBC     HL,BC
        RET     Z
rt_file__bad:
        LD      A,35                    ; data not numeric
        CALL    rt_error
rt_file__zero:
        LD      HL,(rt_file__res)
        LD      B,8
rt_file__z1:
        LD      (HL),0
        INC     HL
        DJNZ    rt_file__z1
        RET

; DE -> n, BC -> channel -> HL = the next n bytes (INPUT$(n,#c)); error 26 at the end
rt_file_chars:
        PUSH    DE
        LD      D,B
        LD      E,C
        CALL    rt_file__in
        POP     HL
        JP      C,rt_file__empty
        LD      C,(HL)                  ; BC = n (negative: 0, from 64 KB on: 65535)
        INC     HL
        LD      B,(HL)
        INC     HL
        LD      A,(HL)
        INC     HL
        LD      D,(HL)
        BIT     7,D
        JP      NZ,rt_file__empty
        OR      D
        JP      Z,rt_file__n1
        LD      BC,&FFFF
rt_file__n1:
        CALL    rt_file__scr
rt_file__cc:
        LD      A,B
        OR      C
        JP      Z,rt_file__temp
        CALL    rt_file__getb
        JP      C,rt_file__l26
        CALL    rt_file__keep
        DEC     BC
        JP      rt_file__cc

; A = 0 or -1 -> (HL) = A as 32 bits
rt_file__bool:
        LD      (HL),A
        INC     HL
        LD      (HL),A
        INC     HL
        LD      (HL),A
        INC     HL
        LD      (HL),A
        RET

; HL = result, DE -> channel: EOF(#n): -1 at the end (and for an output file), else 0
rt_file_eof:
        PUSH    HL
        CALL    rt_file__open
        JP      C,rt_file__no
        INC     HL
        BIT     0,(HL)
        JP      Z,rt_file__yes
        CALL    rt_file__peek
        JP      C,rt_file__yes
rt_file__no:
        POP     HL
        XOR     A
        JP      rt_file__bool
rt_file__yes:
        POP     HL
        LD      A,-1
        JP      rt_file__bool

; HL = result, DE -> channel: LOF(#n), the length of the file
rt_file_lof:
        PUSH    HL
        CALL    rt_file__open
        JP      C,rt_file__no
        LD      A,(HL)
        POP     HL
        JP      plat_fsize

; HL = result, DE = name descriptor: EXIST(name$): -1 or 0
rt_file_exist:
        PUSH    HL
        CALL    rt_file__name
        CALL    plat_fexists
        SBC     A,A
        POP     HL
        JP      rt_file__bool

; DE = descriptor -> DE = characters, BC = length
rt_file__name:
        EX      DE,HL
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        INC     HL
        LD      C,(HL)
        INC     HL
        LD      B,(HL)
        RET

; DE = name descriptor: KILL
rt_file_kill:
        CALL    rt_file__name
        CALL    plat_fdelete
        JP      C,rt_error
        RET

; HL = result, DE -> channel: INP(#n): the next byte (error 26 at the end)
rt_file_inp:
        PUSH    HL
        CALL    rt_file__in
        JP      C,rt_file__no
        CALL    rt_file__getb
        JP      NC,rt_file__ib
        LD      A,26
        CALL    rt_error
        JP      rt_file__no
rt_file__ib:
        POP     HL
        LD      (HL),A
        XOR     A
        INC     HL
        LD      (HL),A
        INC     HL
        LD      (HL),A
        INC     HL
        LD      (HL),A
        RET

;! section data
rt_file__cur:                           ; record of the running PRINT # (0: none)
        DW      0

;! section bss
rt_file__rec:
        DS      2
rt_file__mode:
        DS      1
rt_file__k:
        DS      4
rt_file__saveout:
        DS      2
rt_file__savecol:
        DS      2
rt_file__sb:
        DS      2
rt_file__n:
        DS      2
rt_file__res:
        DS      2
rt_file__ch:
        DS      8*100
