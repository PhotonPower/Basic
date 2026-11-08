;! module    rt_using
;! provides  rt_using_begin rt_using_num rt_using_str rt_using_end rt_using_numb rt_using_strb rt_using_endb
;! provides  rt_str_fmt rt_set_mode rt_mode
;! requires  rt_flt_dec rt_flt_dec_d rt_fmt_flt rt_print_str rt_str_newtemp rt_argp
;! group     print
;! clobbers  AF BC DE HL
;! doc       PRINT USING (runtime/ref/rt_using.c): number fields (# * $ , . ^^^^ + -), string fields (& ! \..\), _ escape.

; One PRINT USING: rt_using_begin (DE = descriptor of the format), for every value rt_using_num
; (DE -> binary64) or rt_using_str (DE = descriptor), then rt_using_end; each prints its text. The
; variants rt_using_numb, rt_using_strb, rt_using_endb return it instead (HL = characters, BC =
; count, at most 512). The state of an outer PRINT USING is kept on a small stack. The format is
; read through its descriptor on every call (the string heap may move it). rt_str_fmt: STR$(x,y,z).

rt_using__MAXDEC EQU 300
rt_using__NBUF   EQU 720
rt_using__DEPTH  EQU 8
rt_using__CAP    EQU 512

; DE -> MODE (0-3)
rt_set_mode:
        LD      A,(DE)
        AND     3
        LD      (rt_mode),A
        RET

; ---- entry points ------------------------------------------------------------------------------

; DE = descriptor of the format
rt_using_begin:
        LD      A,(rt_using__sp)        ; push the state of an outer PRINT USING
        CP      rt_using__DEPTH
        JR      NC,rt_using__full
        PUSH    DE
        LD      L,A
        LD      H,0
        ADD     HL,HL
        LD      B,H
        LD      C,L
        ADD     HL,HL
        ADD     HL,BC                   ; 6 * sp
        LD      DE,rt_using__stack
        ADD     HL,DE
        EX      DE,HL
        LD      HL,rt_using__fdesc
        LD      BC,6
        LDIR
        POP     DE
rt_using__full:
        LD      HL,rt_using__sp
        INC     (HL)
        LD      A,(rt_mode)
        JP      rt_using__init

rt_using_num:
        LD      A,1
        JR      rt_using__numx
rt_using_numb:
        XOR     A
rt_using__numx:
        LD      (rt_using__flush),A
        EX      DE,HL
        LD      DE,rt_using__x
        LD      BC,8
        LDIR
        CALL    rt_using__prep
        CALL    rt_using__numc
        JP      rt_using__fin

rt_using_str:
        LD      A,1
        JR      rt_using__strx
rt_using_strb:
        XOR     A
rt_using__strx:
        LD      (rt_using__flush),A
        LD      (rt_using__sdesc),DE
        CALL    rt_using__prep
        CALL    rt_using__lit1
        JR      Z,rt_using__snof
        LD      HL,(rt_using__pos)
        CALL    rt_using__parse
        LD      A,(rt_using__kind)
        CP      1
        JR      Z,rt_using__smis
        LD      HL,(rt_using__end)
        LD      (rt_using__pos),HL
        CALL    rt_using__string
        JP      rt_using__fin
rt_using__smis:
        CALL    rt_using__mismatch
rt_using__snof:                         ; no suitable field: "?" and the string
        LD      A,'?'
        CALL    rt_using__put
        CALL    rt_using__sget          ; HL = characters, BC = length
rt_using__sall:
        LD      A,B
        OR      C
        JP      Z,rt_using__fin
        LD      A,(HL)
        CALL    rt_using__put
        INC     HL
        DEC     BC
        JR      rt_using__sall

; the rest of the format as text (probe7 T11), then the outer state back
rt_using_end:
        LD      A,1
        JR      rt_using__endx
rt_using_endb:
        XOR     A
rt_using__endx:
        LD      (rt_using__flush),A
        CALL    rt_using__prep
        XOR     A
        LD      (rt_using__wrap),A
        INC     A
        LD      (rt_using__all),A
        CALL    rt_using__literal
        LD      HL,rt_using__sp
        DEC     (HL)
        LD      A,(HL)
        CP      rt_using__DEPTH
        JR      NC,rt_using__fin
        LD      L,A
        LD      H,0
        ADD     HL,HL
        LD      B,H
        LD      C,L
        ADD     HL,HL
        ADD     HL,BC
        LD      DE,rt_using__stack
        ADD     HL,DE
        LD      DE,rt_using__fdesc
        LD      BC,6
        LDIR
        ; fall through

; print the text (rt_using_num ...) or return it: HL = characters, BC = count
rt_using__fin:
        LD      A,(rt_using__flush)
        OR      A
        JP      NZ,rt_using__flushb
        LD      HL,rt_using__obuf
        LD      BC,(rt_using__on)
        RET

; HL = result, DE -> x, BC -> y, rt_argp -> z: STR$(x,y,z) = PRINT USING with y positions, z of
; them after the point -> HL = temporary
rt_str_fmt:
        PUSH    BC
        EX      DE,HL
        LD      DE,rt_using__x
        LD      BC,8
        LDIR
        LD      HL,(rt_using__fdesc)    ; keep a running PRINT USING intact
        PUSH    HL
        LD      HL,(rt_using__pos)
        PUSH    HL
        LD      HL,(rt_using__md)       ; md and has
        PUSH    HL
        XOR     A
        LD      (rt_using__flush),A
        LD      HL,0
        LD      (rt_using__on),HL
        LD      HL,6                    ; y: <= 0 gives "", above 255 is 255
        ADD     HL,SP
        LD      E,(HL)
        INC     HL
        LD      D,(HL)                  ; DE -> y
        EX      DE,HL
        INC     HL
        INC     HL
        INC     HL
        BIT     7,(HL)
        JP      NZ,rt_using__fret
        LD      A,(HL)
        DEC     HL
        OR      (HL)
        DEC     HL
        OR      (HL)
        DEC     HL
        LD      C,(HL)
        JR      Z,rt_using__f1
        LD      C,255
rt_using__f1:
        LD      A,C
        OR      A
        JP      Z,rt_using__fret
        LD      (rt_using__fy),A
        LD      HL,(rt_argp)            ; z: < 0 is 0, above 300 is 300
        INC     HL
        INC     HL
        INC     HL
        LD      DE,0
        BIT     7,(HL)
        JR      NZ,rt_using__f3
        LD      A,(HL)
        DEC     HL
        OR      (HL)
        DEC     HL
        LD      D,(HL)
        DEC     HL
        LD      E,(HL)
        JR      NZ,rt_using__f2
        PUSH    DE
        EX      DE,HL
        LD      DE,301
        OR      A
        SBC     HL,DE
        POP     DE
        JR      C,rt_using__f3
rt_using__f2:
        LD      DE,300
rt_using__f3:                           ; DE = z: build "###.##"
        LD      HL,rt_using__sfmt
        LD      A,D
        OR      E
        JR      Z,rt_using__f6
        PUSH    HL
        LD      A,(rt_using__fy)        ; y - z - 1 '#'
        LD      L,A
        LD      H,0
        OR      A
        SBC     HL,DE
        DEC     HL
        LD      B,H
        LD      C,L
        POP     HL
        BIT     7,B
        JR      NZ,rt_using__f4
rt_using__h1:
        LD      A,B
        OR      C
        JR      Z,rt_using__f4
        LD      (HL),'#'
        INC     HL
        DEC     BC
        JR      rt_using__h1
rt_using__f4:
        LD      A,(rt_mode)             ; the point of the mode
        CP      2
        LD      A,'.'
        JR      C,rt_using__f5
        LD      A,','
rt_using__f5:
        LD      (HL),A
        INC     HL
rt_using__h2:                           ; z '#' while the format is shorter than 258
        LD      A,D
        OR      E
        JR      Z,rt_using__f7
        PUSH    HL
        LD      BC,rt_using__sfmt+258
        OR      A
        SBC     HL,BC
        POP     HL
        JR      NC,rt_using__f7
        LD      (HL),'#'
        INC     HL
        DEC     DE
        JR      rt_using__h2
rt_using__f6:
        LD      A,(rt_using__fy)
        LD      B,A
rt_using__h3:
        LD      (HL),'#'
        INC     HL
        DJNZ    rt_using__h3
rt_using__f7:
        LD      DE,rt_using__sfmt       ; the static descriptor of the format
        LD      (rt_using__sdsc),DE
        OR      A
        SBC     HL,DE
        LD      (rt_using__sdsc+2),HL
        LD      DE,rt_using__sdsc
        LD      A,(rt_mode)
        CALL    rt_using__init
        CALL    rt_using__prep
        CALL    rt_using__numc
rt_using__fret:
        POP     HL
        LD      (rt_using__md),HL
        POP     HL
        LD      (rt_using__pos),HL
        POP     HL
        LD      (rt_using__fdesc),HL
        POP     BC
        LD      BC,(rt_using__on)       ; the text as a temporary
        PUSH    BC
        CALL    rt_str_newtemp          ; HL = descriptor, DE = characters
        POP     BC
        LD      A,B
        OR      C
        RET     Z
        PUSH    HL
        LD      HL,rt_using__obuf
        LDIR
        POP     HL
        RET

; ---- common --------------------------------------------------------------------------------------

; DE = format descriptor, A = mode: new state (pos 0, has_field)
rt_using__init:
        LD      (rt_using__fdesc),DE
        LD      (rt_using__md),A
        XOR     A
        LD      (rt_using__has),A
        LD      H,A
        LD      L,A
        LD      (rt_using__pos),HL
        CALL    rt_using__load
        LD      HL,0
rt_using__i1:
        CALL    rt_using__inf           ; index < length?
        RET     NC
        CALL    rt_using__chr
        CP      '_'
        JR      NZ,rt_using__i2
        INC     HL
        INC     HL
        JR      rt_using__i1
rt_using__i2:
        CALL    rt_using__start
        OR      A
        JR      NZ,rt_using__i3
        INC     HL
        JR      rt_using__i1
rt_using__i3:
        LD      A,1
        LD      (rt_using__has),A
        RET

; characters and length of the format from its descriptor; point and separator of the mode
rt_using__load:
        LD      HL,(rt_using__fdesc)
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        INC     HL
        LD      (rt_using__fp),DE
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        LD      (rt_using__flen),DE
        LD      A,(rt_using__md)
        CP      2
        LD      HL,'.' + 256*','
        JR      C,rt_using__l1
        LD      HL,',' + 256*'.'
rt_using__l1:
        LD      (rt_using__dpt),HL      ; dpt, then thc
        RET

; empty output
rt_using__prep:
        LD      HL,0
        LD      (rt_using__on),HL
        JR      rt_using__load

; A = character into the output; full: printed first (rt_using_num ...) or dropped. Keeps all.
rt_using__put:
        PUSH    HL
        PUSH    DE
        PUSH    BC
        LD      C,A
        LD      HL,(rt_using__on)
        LD      DE,rt_using__CAP
        OR      A
        SBC     HL,DE
        JR      C,rt_using__p1
        LD      A,(rt_using__flush)
        OR      A
        JR      Z,rt_using__p9
        PUSH    BC
        CALL    rt_using__flushb
        POP     BC
rt_using__p1:
        LD      HL,(rt_using__on)
        LD      DE,rt_using__obuf
        ADD     HL,DE
        LD      (HL),C
        LD      HL,(rt_using__on)
        INC     HL
        LD      (rt_using__on),HL
rt_using__p9:
        LD      A,C
        POP     BC
        POP     DE
        POP     HL
        RET

; print the output and empty it
rt_using__flushb:
        LD      BC,(rt_using__on)
        LD      A,B
        OR      C
        RET     Z
        LD      DE,rt_using__obuf
        CALL    rt_print_str
        LD      HL,0
        LD      (rt_using__on),HL
        RET

; A = character, unless characters are still to be skipped (the front of a too long number)
rt_using__emit:
        PUSH    HL
        LD      HL,(rt_using__skip)
        PUSH    AF
        LD      A,H
        OR      L
        JR      Z,rt_using__e1
        DEC     HL
        LD      (rt_using__skip),HL
        POP     AF
        POP     HL
        RET
rt_using__e1:
        POP     AF
        POP     HL
        JR      rt_using__put

; HL = index -> CF set if it is inside the format (keeps all but F)
rt_using__inf:
        PUSH    HL
        PUSH    DE
        LD      DE,(rt_using__flen)
        OR      A
        SBC     HL,DE
        POP     DE
        POP     HL
        RET

; HL = index (inside) -> A = the character there (keeps all but AF)
rt_using__chr:
        PUSH    HL
        PUSH    DE
        LD      DE,(rt_using__fp)
        ADD     HL,DE
        LD      A,(HL)
        POP     DE
        POP     HL
        RET

; HL = index, C = character -> Z set if the format has it there (keeps all but AF)
rt_using__at:
        CALL    rt_using__inf
        JR      NC,rt_using__an
        CALL    rt_using__chr
        CP      C
        RET
rt_using__an:
        XOR     A
        INC     A                       ; NZ
        RET

; HL = index -> Z set if '#', '$' or point and '#' are there (keeps all but AF)
rt_using__nstart:
        PUSH    BC
        LD      C,'#'
        CALL    rt_using__at
        JR      Z,rt_using__ns9
        LD      C,'$'
        CALL    rt_using__at
        JR      Z,rt_using__ns9
        CALL    rt_using__pdig
rt_using__ns9:
        POP     BC
        RET

; HL = index -> Z set if a point and '#' are there (keeps all but AF, C)
rt_using__pdig:
        LD      A,(rt_using__dpt)
        LD      C,A
        CALL    rt_using__at
        RET     NZ
        INC     HL
        LD      C,'#'
        CALL    rt_using__at
        DEC     HL
        RET

; HL = index -> A = kind of a field starting there (0 none, 1 number, 2 &, 3 !, 4 \..\); keeps
; all but AF
rt_using__start:
        PUSH    BC
        PUSH    HL
        CALL    rt_using__chr
        LD      C,A
        CP      '#'
        JR      Z,rt_using__s1
        LD      A,(rt_using__dpt)
        CP      C
        JR      NZ,rt_using__sa
        INC     HL
        LD      C,'#'
        CALL    rt_using__at
        JR      Z,rt_using__s1
        JR      rt_using__s0
rt_using__sa:
        LD      A,C
        CP      '+'
        JR      Z,rt_using__sgn
        CP      '-'
        JR      Z,rt_using__sgn
        CP      '*'
        JR      Z,rt_using__sst
        CP      '$'
        JR      Z,rt_using__sdl
        CP      '&'
        JR      Z,rt_using__s2
        CP      '!'
        JR      Z,rt_using__s3
        CP      92
        JR      Z,rt_using__sbs
rt_using__s0:
        XOR     A
        JR      rt_using__s9
rt_using__s1:
        LD      A,1
        JR      rt_using__s9
rt_using__s2:
        LD      A,2
        JR      rt_using__s9
rt_using__s3:
        LD      A,3
        JR      rt_using__s9
rt_using__s4:
        LD      A,4
rt_using__s9:
        POP     HL
        POP     BC
        RET
rt_using__sgn:                          ; '#', '*', '$' or point and '#' follow
        INC     HL
        LD      C,'*'
        CALL    rt_using__at
        JR      Z,rt_using__s1
        CALL    rt_using__nstart
        JR      Z,rt_using__s1
        JR      rt_using__s0
rt_using__sst:                          ; '*'s, then '#', '$' or point and '#'
        LD      C,'*'
        CALL    rt_using__at
        JR      NZ,rt_using__ss1
        INC     HL
        JR      rt_using__sst
rt_using__ss1:
        CALL    rt_using__nstart
        JR      Z,rt_using__s1
        JR      rt_using__s0
rt_using__sdl:                          ; '#' or '*' follow
        INC     HL
        LD      C,'#'
        CALL    rt_using__at
        JR      Z,rt_using__s1
        LD      C,'*'
        CALL    rt_using__at
        JR      Z,rt_using__s1
        JR      rt_using__s0
rt_using__sbs:                          ; spaces, then 92
        INC     HL
        LD      C,' '
        CALL    rt_using__at
        JR      Z,rt_using__sbs
        LD      C,92
        CALL    rt_using__at
        JR      Z,rt_using__s4
        JR      rt_using__s0

; HL = index of a field -> the field record (rt_using__kind ... rt_using__end)
rt_using__parse:
        LD      (rt_using__pi),HL
        CALL    rt_using__start
        LD      (rt_using__kind),A
        PUSH    AF
        LD      HL,rt_using__fl0        ; all other fields 0
        LD      B,rt_using__flen0
rt_using__pc:
        LD      (HL),0
        INC     HL
        DJNZ    rt_using__pc
        POP     AF
        LD      HL,(rt_using__pi)
        CP      4
        JR      Z,rt_using__p4
        CP      2
        JR      C,rt_using__pn
        INC     HL                      ; & and !: one character
        LD      (rt_using__end),HL
        RET
rt_using__p4:                           ; \..\: up to the next 92
        INC     HL
        CALL    rt_using__chr
        CP      92
        JR      NZ,rt_using__p4
        INC     HL
        LD      (rt_using__end),HL
        LD      DE,(rt_using__pi)
        OR      A
        SBC     HL,DE
        LD      (rt_using__width),HL
        RET
rt_using__pn:                           ; number
        CALL    rt_using__chr
        CP      '+'
        JR      Z,rt_using__pld
        CP      '-'
        JR      NZ,rt_using__pl
rt_using__pld:
        LD      (rt_using__lead),A
        INC     HL
rt_using__pl:
        CALL    rt_using__inf
        JR      NC,rt_using__pe
        CALL    rt_using__chr
        CP      '#'
        JR      Z,rt_using__pdg
        CP      '*'
        JR      NZ,rt_using__pnd
        LD      (rt_using__star),A
rt_using__pdg:
        LD      DE,(rt_using__ndig)
        INC     DE
        LD      (rt_using__ndig),DE
        JR      rt_using__pnx
rt_using__pnd:
        CP      '$'
        JR      NZ,rt_using__pth
        LD      A,(rt_using__dollar)
        OR      A
        JR      NZ,rt_using__pe
        INC     A
        LD      (rt_using__dollar),A
        JR      rt_using__pnx
rt_using__pth:
        LD      C,A
        LD      A,(rt_using__thc)
        CP      C
        JR      NZ,rt_using__pe
        LD      DE,(rt_using__ndig)
        LD      A,D
        OR      E
        JR      Z,rt_using__pe
        INC     HL
        LD      C,'#'
        CALL    rt_using__at
        DEC     HL
        JR      NZ,rt_using__pe
        LD      A,1
        LD      (rt_using__thou),A
rt_using__pnx:
        LD      DE,(rt_using__wint)
        INC     DE
        LD      (rt_using__wint),DE
        INC     HL
        JR      rt_using__pl
rt_using__pe:
        LD      A,(rt_using__dpt)
        LD      C,A
        CALL    rt_using__at
        JR      NZ,rt_using__pcr
        LD      A,1
        LD      (rt_using__point),A
        INC     HL
rt_using__pdl:
        LD      C,'#'
        CALL    rt_using__at
        JR      NZ,rt_using__pcr
        LD      DE,(rt_using__dec)
        INC     DE
        LD      (rt_using__dec),DE
        INC     HL
        JR      rt_using__pdl
rt_using__pcr:                          ; a run of at least 3 '^': exponent format
        PUSH    HL
        LD      DE,0
        LD      C,'^'
rt_using__pc1:
        CALL    rt_using__at
        JR      NZ,rt_using__pc2
        INC     HL
        INC     DE
        JR      rt_using__pc1
rt_using__pc2:
        POP     HL
        LD      A,D
        OR      A
        JR      NZ,rt_using__pc3
        LD      A,E
        CP      3
        JR      C,rt_using__ptr
rt_using__pc3:
        LD      (rt_using__carets),DE
        ADD     HL,DE
rt_using__ptr:                          ; a sign at the end
        LD      A,(rt_using__lead)
        OR      A
        JR      NZ,rt_using__pf
        LD      C,'-'
        CALL    rt_using__at
        JR      Z,rt_using__pt1
        LD      C,'+'
        CALL    rt_using__at
        JR      NZ,rt_using__pf
rt_using__pt1:
        LD      A,C
        LD      (rt_using__trail),A
        INC     HL
rt_using__pf:
        LD      (rt_using__end),HL
        RET

; literal text from pos up to the next field; wrap: the format starts over once at its end; all:
; fields are text too -> A = 1 and NZ if a field starts at pos
rt_using__lit1:                         ; wrap, not all
        LD      A,1
        LD      (rt_using__wrap),A
        XOR     A
        LD      (rt_using__all),A
rt_using__literal:
        LD      HL,(rt_using__pos)
rt_using__lt1:
        CALL    rt_using__inf
        JR      NC,rt_using__lte
        CALL    rt_using__chr
        CP      '_'
        JR      NZ,rt_using__lt2
        INC     HL                      ; '_': the next character as text
        LD      A,'_'
        CALL    rt_using__inf
        CALL    C,rt_using__chr
        CALL    rt_using__put
        INC     HL
        JR      rt_using__lt1
rt_using__lt2:
        LD      C,A
        LD      A,(rt_using__all)
        OR      A
        JR      NZ,rt_using__lt3
        CALL    rt_using__start
        OR      A
        JR      Z,rt_using__lt3
        LD      (rt_using__pos),HL
        LD      A,1
        OR      A
        RET
rt_using__lt3:
        LD      A,C
        CALL    rt_using__put
        INC     HL
        JR      rt_using__lt1
rt_using__lte:
        CALL    rt_using__inf           ; pos > len (after a '_' at the end): len
        LD      DE,(rt_using__flen)
        JR      C,rt_using__lt4
        EX      DE,HL
rt_using__lt4:
        LD      (rt_using__pos),HL
        LD      A,(rt_using__wrap)
        LD      C,A
        LD      A,(rt_using__has)
        AND     C
        RET     Z
        XOR     A
        LD      (rt_using__wrap),A
        LD      H,A
        LD      L,A
        JR      rt_using__lt1

; a field of the wrong kind as text (probe7 T17, T18)
rt_using__mismatch:
        LD      HL,(rt_using__pos)
rt_using__mm:
        PUSH    HL
        LD      DE,(rt_using__end)
        OR      A
        SBC     HL,DE
        POP     HL
        JR      NC,rt_using__mm9
        CALL    rt_using__chr
        CALL    rt_using__put
        INC     HL
        JR      rt_using__mm
rt_using__mm9:
        LD      (rt_using__pos),HL
        RET

; -> HL = characters, BC = length of the string rt_using__sdesc
rt_using__sget:
        LD      HL,(rt_using__sdesc)
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        INC     HL
        LD      C,(HL)
        INC     HL
        LD      B,(HL)
        EX      DE,HL
        RET

; string field with the string rt_using__sdesc
rt_using__string:
        CALL    rt_using__sget
        LD      A,(rt_using__kind)
        CP      3
        JR      Z,rt_using__st3
        JR      NC,rt_using__st4
rt_using__st2:                          ; &: all of it
        LD      A,B
        OR      C
        RET     Z
        LD      A,(HL)
        CALL    rt_using__put
        INC     HL
        DEC     BC
        JR      rt_using__st2
rt_using__st3:                          ; !: the first character, or a space (probe7 T19)
        LD      A,B
        OR      C
        LD      A,' '
        JP      Z,rt_using__put
        LD      A,(HL)
        JP      rt_using__put
rt_using__st4:                          ; \..\: width characters, filled with spaces
        LD      DE,(rt_using__width)
rt_using__s41:
        LD      A,D
        OR      E
        RET     Z
        LD      A,B
        OR      C
        LD      A,' '
        JR      Z,rt_using__s42
        LD      A,(HL)
        INC     HL
        DEC     BC
rt_using__s42:
        CALL    rt_using__put
        DEC     DE
        JR      rt_using__s41

; number field at the current position (or "?" and the number): x in rt_using__x
rt_using__numc:
        CALL    rt_using__lit1
        JR      Z,rt_using__unf
        LD      HL,(rt_using__pos)
        CALL    rt_using__parse
        LD      A,(rt_using__kind)
        CP      1
        JR      NZ,rt_using__nmis
        LD      HL,(rt_using__end)
        LD      (rt_using__pos),HL
        JP      rt_using__number
rt_using__nmis:
        CALL    rt_using__mismatch
rt_using__unf:                          ; "?" and the number as by PRINT
        LD      A,'?'
        CALL    rt_using__put
        LD      HL,rt_using__tmp
        LD      DE,rt_using__x
        LD      A,13
        CALL    rt_fmt_flt
        LD      HL,rt_using__tmp
rt_using__u1:
        LD      A,B
        OR      C
        RET     Z
        LD      A,(HL)
        CALL    rt_using__put
        INC     HL
        DEC     BC
        JR      rt_using__u1

; ---- numbers -------------------------------------------------------------------------------------

; HL / DE (unsigned) -> HL = quotient, DE = remainder
rt_using__div:
        LD      B,H
        LD      C,L
        LD      HL,0
        LD      A,16
rt_using__dv1:
        SLA     C
        RL      B
        ADC     HL,HL
        OR      A
        SBC     HL,DE
        JR      NC,rt_using__dv2
        ADD     HL,DE
        JR      rt_using__dv3
rt_using__dv2:
        INC     C
rt_using__dv3:
        DEC     A
        JR      NZ,rt_using__dv1
        EX      DE,HL
        LD      H,B
        LD      L,C
        RET

; HL -> HL = -HL
rt_using__neg:
        EX      DE,HL
        XOR     A
        LD      H,A
        LD      L,A
        SBC     HL,DE
        RET

; DE = dd (weight of ds[0] is 10^dd): rt_using__n = the digits of N = round(|x| * 10^dec) (ties
; away from 0), rt_using__nlen = their count (at least 1)
rt_using__scaled:
        LD      HL,(rt_using__dec)
        ADD     HL,DE                   ; idx: index in ds of the units digit of N
        BIT     7,H
        JR      Z,rt_using__sc2
        LD      A,H
        AND     L
        INC     A
        JR      NZ,rt_using__sc0        ; idx < -1: 0
        LD      A,(rt_using__ds)        ; idx = -1: 0 or 1
        CP      '5'
        LD      A,'0'
        JR      C,rt_using__sc1
        INC     A
        JR      rt_using__sc1
rt_using__sc0:
        LD      A,'0'
rt_using__sc1:
        LD      (rt_using__n),A
        LD      HL,1
        LD      (rt_using__nlen),HL
        RET
rt_using__sc2:
        PUSH    HL
        LD      DE,rt_using__NBUF-2
        OR      A
        SBC     HL,DE
        POP     HL
        JR      NC,rt_using__sc0
        LD      (rt_using__idx),HL
        INC     HL
        LD      (rt_using__nlen),HL
        LD      DE,rt_using__n
        PUSH    HL
        LD      BC,-14
        ADD     HL,BC
        POP     BC                      ; BC = count
        JR      NC,rt_using__sc3        ; count <= 13
        LD      HL,rt_using__ds         ; 13 digits, then '0'
        LD      BC,13
        LDIR
        LD      HL,(rt_using__nlen)
        LD      BC,-13
        ADD     HL,BC
        LD      B,H
        LD      C,L
        EX      DE,HL
rt_using__sz:
        LD      (HL),'0'
        INC     HL
        DEC     BC
        LD      A,B
        OR      C
        JR      NZ,rt_using__sz
        RET
rt_using__sc3:
        LD      HL,rt_using__ds
        LDIR
        LD      A,(rt_using__idx)       ; round with ds[idx + 1] if it exists
        CP      12
        RET     NC
        INC     A
        LD      E,A
        LD      D,0
        LD      HL,rt_using__ds
        ADD     HL,DE
        LD      A,(HL)
        CP      '5'
        RET     C
        LD      BC,(rt_using__nlen)
        LD      HL,rt_using__n-1
        ADD     HL,BC
rt_using__si:
        LD      A,(HL)
        CP      '9'
        JR      NZ,rt_using__si1
        LD      (HL),'0'
        DEC     HL
        DEC     BC
        LD      A,B
        OR      C
        JR      NZ,rt_using__si
        LD      BC,(rt_using__nlen)     ; all nines: a new '1' in front
        LD      HL,rt_using__n-1
        ADD     HL,BC
        LD      D,H
        LD      E,L
        INC     DE
        LDDR
        LD      A,'1'
        LD      (rt_using__n),A
        LD      HL,(rt_using__nlen)
        INC     HL
        LD      (rt_using__nlen),HL
        RET
rt_using__si1:
        INC     (HL)
        RET

; -> HL = digits before the point in rt_using__n (without leading zeros, at least 1)
rt_using__intdig:
        LD      HL,(rt_using__nlen)
        LD      DE,(rt_using__dec)
        OR      A
        SBC     HL,DE
        DEC     HL                      ; units
        BIT     7,H
        JR      NZ,rt_using__id1
        LD      B,H
        LD      C,L
        LD      DE,rt_using__n
        INC     HL                      ; count = units - first + 1
rt_using__id0:
        LD      A,B
        OR      C
        RET     Z
        LD      A,(DE)
        CP      '0'
        RET     NZ
        INC     DE
        DEC     BC
        DEC     HL
        JR      rt_using__id0
rt_using__id1:
        LD      HL,1
        RET

; A = sign position ('+' or '-') -> A = '-' if negative, '+' for '+', else ' '
rt_using__sign:
        LD      C,A
        LD      A,(rt_using__ng)
        OR      A
        LD      A,'-'
        RET     NZ
        LD      A,C
        CP      '+'
        RET     Z
        LD      A,' '
        RET

; number field (record) with x
rt_using__number:
        LD      HL,(rt_using__dec)      ; digits after the point: at most MAXDEC
        PUSH    HL
        LD      DE,rt_using__MAXDEC+1
        OR      A
        SBC     HL,DE
        POP     HL
        JR      C,rt_using__nm1
        LD      HL,rt_using__MAXDEC
rt_using__nm1:
        LD      (rt_using__dec),HL
        LD      HL,rt_using__ds
        LD      DE,rt_using__x
        LD      A,13
        CALL    rt_flt_dec
        LD      (rt_using__ng),A
        LD      HL,(rt_flt_dec_d)
        LD      (rt_using__d),HL
        LD      HL,(rt_using__carets)
        LD      A,H
        OR      L
        JR      NZ,rt_using__nex
        LD      DE,(rt_using__d)
        CALL    rt_using__scaled
        JP      rt_using__nfx
rt_using__nex:                          ; exponent format: a multiple of the digit positions
        LD      HL,0
        LD      (rt_using__e),HL
        LD      A,(rt_using__ds)
        CP      '0'
        JR      Z,rt_using__ne1
        LD      DE,(rt_using__ndig)
        LD      A,D
        OR      E
        JR      NZ,rt_using__nek
        LD      HL,(rt_using__d)        ; k = 0: e = d + 1
        INC     HL
        LD      (rt_using__e),HL
        JR      rt_using__ne1
rt_using__nek:                          ; e = floor(d / k) * k
        LD      HL,(rt_using__d)
        BIT     7,H
        JR      NZ,rt_using__nkn
        CALL    rt_using__div           ; DE = d mod k
        LD      HL,(rt_using__d)
        OR      A
        SBC     HL,DE
        LD      (rt_using__e),HL
        JR      rt_using__ne1
rt_using__nkn:                          ; d < 0: m = -d mod k; e = d + m - (m > 0 ? k : 0)
        CALL    rt_using__neg
        LD      DE,(rt_using__ndig)
        CALL    rt_using__div
        EX      DE,HL
        LD      A,H
        OR      L
        JR      Z,rt_using__nkz
        LD      DE,(rt_using__ndig)
        OR      A
        SBC     HL,DE
rt_using__nkz:
        LD      DE,(rt_using__d)
        ADD     HL,DE
        LD      (rt_using__e),HL
rt_using__ne1:
        CALL    rt_using__nsc
        LD      A,(rt_using__ds)        ; rounding carried into a new digit: once more
        CP      '0'
        JR      Z,rt_using__nfx
        LD      DE,(rt_using__ndig)
        LD      A,D
        OR      E
        JR      Z,rt_using__nk0
        PUSH    DE
        CALL    rt_using__intdig
        POP     DE
        EX      DE,HL
        OR      A
        SBC     HL,DE                   ; k - intdigits: CF if more digits than k
        JR      NC,rt_using__nfx
        LD      HL,(rt_using__e)
        LD      DE,(rt_using__ndig)
        ADD     HL,DE
        LD      (rt_using__e),HL
        CALL    rt_using__nsc
        JR      rt_using__nfx
rt_using__nk0:
        LD      HL,(rt_using__dec)
        LD      DE,(rt_using__nlen)
        OR      A
        SBC     HL,DE                   ; CF if nlen > dec
        JR      NC,rt_using__nfx
        LD      A,(rt_using__n)
        CP      '0'
        JR      Z,rt_using__nfx
        LD      HL,(rt_using__e)
        INC     HL
        LD      (rt_using__e),HL
        CALL    rt_using__nsc
        JR      rt_using__nfx
rt_using__nsc:                          ; scaled(d - e)
        LD      HL,(rt_using__d)
        LD      DE,(rt_using__e)
        OR      A
        SBC     HL,DE
        EX      DE,HL
        JP      rt_using__scaled
rt_using__nfx:                          ; at least one digit before the point
        LD      HL,(rt_using__dec)
        INC     HL
        LD      DE,(rt_using__nlen)
        OR      A
        SBC     HL,DE                   ; sh = dec + 1 - nlen
        JR      C,rt_using__np9
        JR      Z,rt_using__np9
        LD      (rt_using__sh),HL
        LD      BC,(rt_using__nlen)
        LD      HL,rt_using__n-1
        ADD     HL,BC
        PUSH    HL
        LD      DE,(rt_using__sh)
        ADD     HL,DE
        EX      DE,HL
        POP     HL
        LDDR
        LD      HL,rt_using__n
        LD      BC,(rt_using__sh)
rt_using__nz:
        LD      (HL),'0'
        INC     HL
        DEC     BC
        LD      A,B
        OR      C
        JR      NZ,rt_using__nz
        LD      HL,(rt_using__dec)
        INC     HL
        LD      (rt_using__nlen),HL
rt_using__np9:
        CALL    rt_using__intdig
        LD      (rt_using__idig),HL
        LD      HL,(rt_using__nlen)     ; units = nlen - dec - 1
        LD      DE,(rt_using__dec)
        OR      A
        SBC     HL,DE
        DEC     HL
        LD      (rt_using__units),HL
        INC     HL
        LD      DE,(rt_using__idig)
        OR      A
        SBC     HL,DE
        LD      (rt_using__first),HL
        LD      HL,(rt_using__idig)     ; ilen = digits + separators
        LD      (rt_using__ilen),HL
        LD      A,(rt_using__thou)
        OR      A
        JR      Z,rt_using__il1
        DEC     HL
        LD      DE,3
        CALL    rt_using__div
        LD      DE,(rt_using__ilen)
        ADD     HL,DE
        LD      (rt_using__ilen),HL
rt_using__il1:
        XOR     A
        LD      (rt_using__nodig),A
        LD      HL,(rt_using__ndig)     ; ".##": no units digit
        LD      A,H
        OR      L
        JR      NZ,rt_using__il2
        LD      HL,(rt_using__ilen)
        DEC     HL
        LD      A,H
        OR      L
        JR      NZ,rt_using__il2
        LD      HL,(rt_using__units)
        LD      DE,rt_using__n
        ADD     HL,DE
        LD      A,(HL)
        CP      '0'
        JR      NZ,rt_using__il2
        LD      A,1
        LD      (rt_using__nodig),A
        LD      HL,0
        LD      (rt_using__ilen),HL
rt_using__il2:
        LD      A,(rt_using__dollar)
        OR      A
        CALL    NZ,rt_using__ilinc
        XOR     A                       ; without a sign position the minus takes a digit
        LD      (rt_using__minus),A     ; position; never in the exponent format (probe8)
        LD      A,(rt_using__ng)
        OR      A
        JR      Z,rt_using__il4
        LD      A,(rt_using__lead)
        LD      C,A
        LD      A,(rt_using__trail)
        OR      C
        JR      NZ,rt_using__il4
        LD      HL,(rt_using__carets)
        LD      A,H
        OR      L
        JR      NZ,rt_using__il4
        INC     A
        LD      (rt_using__minus),A
        CALL    rt_using__ilinc
rt_using__il4:
        LD      A,(rt_using__lead)      ; fixed sign in front
        OR      A
        JR      Z,rt_using__il5
        CALL    rt_using__sign
        CALL    rt_using__put
rt_using__il5:                          ; integer field right-aligned, the front dropped if too long
        LD      HL,(rt_using__wint)
        LD      DE,(rt_using__ilen)
        OR      A
        SBC     HL,DE
        LD      DE,0
        JR      Z,rt_using__il7
        BIT     7,H
        JR      NZ,rt_using__il6
        LD      B,H
        LD      C,L
        LD      A,(rt_using__star)
        OR      A
        LD      A,' '
        JR      Z,rt_using__pd
        LD      A,'*'
rt_using__pd:
        CALL    rt_using__put
        DEC     BC
        LD      E,A
        LD      A,B
        OR      C
        LD      A,E
        JR      NZ,rt_using__pd
        LD      DE,0
        JR      rt_using__il7
rt_using__il6:
        CALL    rt_using__neg
        EX      DE,HL
rt_using__il7:
        LD      (rt_using__skip),DE
        LD      A,(rt_using__minus)
        OR      A
        LD      A,'-'
        CALL    NZ,rt_using__emit
        LD      A,(rt_using__dollar)
        OR      A
        LD      A,'$'
        CALL    NZ,rt_using__emit
        LD      A,(rt_using__nodig)
        OR      A
        JR      NZ,rt_using__dg9
        LD      HL,(rt_using__units)    ; cnt = units - i, separator after a digit with cnt % 3 = 0
        LD      DE,(rt_using__first)
        OR      A
        SBC     HL,DE
        LD      (rt_using__cnt),HL
        LD      DE,3
        CALL    rt_using__div
        LD      A,E
        LD      (rt_using__m3),A
        LD      HL,(rt_using__first)
rt_using__dg1:
        PUSH    HL
        LD      DE,rt_using__n
        ADD     HL,DE
        LD      A,(HL)
        POP     HL
        CALL    rt_using__emit
        LD      DE,(rt_using__cnt)
        LD      A,D
        OR      E
        JR      Z,rt_using__dg9
        LD      A,(rt_using__thou)
        OR      A
        JR      Z,rt_using__dg2
        LD      A,(rt_using__m3)
        OR      A
        JR      NZ,rt_using__dg2
        LD      A,(rt_using__thc)
        CALL    rt_using__emit
rt_using__dg2:
        LD      A,(rt_using__m3)
        OR      A
        JR      NZ,rt_using__dg3
        LD      A,3
rt_using__dg3:
        DEC     A
        LD      (rt_using__m3),A
        DEC     DE
        LD      (rt_using__cnt),DE
        INC     HL
        JR      rt_using__dg1
rt_using__dg9:
        LD      A,(rt_using__point)     ; point and the digits after it
        OR      A
        JR      Z,rt_using__pt9
        LD      A,(rt_using__dpt)
        CALL    rt_using__put
        LD      BC,(rt_using__dec)
        LD      HL,(rt_using__nlen)
        OR      A
        SBC     HL,BC
        LD      DE,rt_using__n
        ADD     HL,DE
rt_using__po1:
        LD      A,B
        OR      C
        JR      Z,rt_using__pt9
        LD      A,(HL)
        CALL    rt_using__put
        INC     HL
        DEC     BC
        JR      rt_using__po1
rt_using__pt9:
        LD      HL,(rt_using__carets)   ; exactly carets-2 exponent digits (at most 9); leading
        LD      A,H                     ; digits are dropped (1E100 -> E+00, probe7)
        OR      L
        JR      Z,rt_using__ex9
        LD      A,'E'
        CALL    rt_using__put
        LD      HL,(rt_using__e)
        LD      A,'+'
        BIT     7,H
        JR      Z,rt_using__ex1
        CALL    rt_using__neg
        LD      A,'-'
rt_using__ex1:
        CALL    rt_using__put
        LD      DE,rt_using__exb+8      ; 9 digits of |e|
        LD      B,9
rt_using__ex2:
        PUSH    BC
        PUSH    DE
        LD      DE,10
        CALL    rt_using__div
        LD      A,E
        POP     DE
        POP     BC
        ADD     A,'0'
        LD      (DE),A
        DEC     DE
        DJNZ    rt_using__ex2
        LD      HL,(rt_using__carets)
        DEC     HL
        DEC     HL
        LD      A,H
        OR      A
        JR      NZ,rt_using__ex3
        LD      A,L
        CP      10
        JR      C,rt_using__ex4
rt_using__ex3:
        LD      A,9
rt_using__ex4:
        LD      B,A                     ; the last `want` of them
        LD      A,9
        SUB     B
        LD      E,A
        LD      D,0
        LD      HL,rt_using__exb
        ADD     HL,DE
rt_using__ex5:
        LD      A,(HL)
        CALL    rt_using__put
        INC     HL
        DJNZ    rt_using__ex5
rt_using__ex9:
        LD      A,(rt_using__trail)     ; fixed sign at the end
        OR      A
        RET     Z
        CALL    rt_using__sign
        JP      rt_using__put

rt_using__ilinc:
        LD      HL,(rt_using__ilen)
        INC     HL
        LD      (rt_using__ilen),HL
        RET

;! section data
rt_using__sp:
        DB      0

;! section bss
rt_mode:
        DS      1
rt_using__fdesc:                        ; state: fdesc, pos, md, has (6 bytes, in this order)
        DS      2
rt_using__pos:
        DS      2
rt_using__md:
        DS      1
rt_using__has:
        DS      1
rt_using__stack:
        DS      6*rt_using__DEPTH
rt_using__fp:
        DS      2
rt_using__flen:
        DS      2
rt_using__dpt:                          ; point, then separator
        DS      1
rt_using__thc:
        DS      1
rt_using__flush:
        DS      1
rt_using__on:
        DS      2
rt_using__wrap:
        DS      1
rt_using__all:
        DS      1
rt_using__sdesc:
        DS      2
rt_using__pi:
        DS      2
rt_using__kind:                         ; field record: kind, then the rest (cleared together)
        DS      1
rt_using__fl0:
rt_using__width:
        DS      2
rt_using__lead:
        DS      1
rt_using__trail:
        DS      1
rt_using__dollar:
        DS      1
rt_using__star:
        DS      1
rt_using__thou:
        DS      1
rt_using__point:
        DS      1
rt_using__wint:
        DS      2
rt_using__ndig:
        DS      2
rt_using__dec:
        DS      2
rt_using__carets:
        DS      2
rt_using__end:
        DS      2
rt_using__flen0 EQU rt_using__end+2-rt_using__fl0
rt_using__x:
        DS      8
rt_using__ds:
        DS      13
rt_using__ng:
        DS      1
rt_using__d:
        DS      2
rt_using__e:
        DS      2
rt_using__idx:
        DS      2
rt_using__nlen:
        DS      2
rt_using__sh:
        DS      2
rt_using__idig:
        DS      2
rt_using__units:
        DS      2
rt_using__first:
        DS      2
rt_using__ilen:
        DS      2
rt_using__nodig:
        DS      1
rt_using__minus:
        DS      1
rt_using__skip:
        DS      2
rt_using__cnt:
        DS      2
rt_using__m3:
        DS      1
rt_using__exb:
        DS      9
rt_using__fy:
        DS      1
rt_using__sdsc:
        DS      4
rt_using__tmp:
        DS      24
rt_using__sfmt:
        DS      260
rt_using__n:
        DS      rt_using__NBUF
rt_using__obuf:
        DS      rt_using__CAP
