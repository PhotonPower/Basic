;! module    rt_matprint
;! provides  rt_mat_print
;! requires  rt_mat_v rt_mat_el rt_print_flt rt_print_str rt_print_nl rt_print_sdesc rt_str_fmt rt_str_mark rt_str_release rt_argp
;! group     matrix
;! clobbers  AF BC DE HL
;! doc       MAT PRINT (D23, probe17): view 0 of rt_mat, elements with ",", lines CR LF.

; DE -> 1 with format, BC -> g, rt_argp -> n: STR$(x,g,n); elements with ',', lines CR LF
rt_mat_print:
        LD      A,(DE)
        LD      (rt_matprint__f),A
        LD      H,B
        LD      L,C
        LD      DE,rt_matprint__g
        LD      BC,4
        LDIR
        LD      HL,(rt_argp)
        LD      DE,rt_matprint__n
        LD      BC,4
        LDIR
        LD      HL,0
        LD      (rt_matprint__i),HL
rt_matprint__ni:
        LD      HL,(rt_matprint__i)
        LD      DE,(rt_mat_v+2)
        OR      A
        SBC     HL,DE
        RET     NC
        LD      HL,0
        LD      (rt_matprint__j),HL
rt_matprint__nj:
        LD      HL,(rt_matprint__j)
        LD      DE,(rt_mat_v+4)
        OR      A
        SBC     HL,DE
        JR      NC,rt_matprint__xi
        LD      HL,(rt_matprint__j)
        LD      A,H
        OR      L
        JR      Z,rt_matprint__x
        LD      DE,rt_matprint__comma
        LD      BC,1
        CALL    rt_print_str
rt_matprint__x:
        LD      HL,rt_mat_v
        LD      BC,(rt_matprint__i)
        LD      DE,(rt_matprint__j)
        CALL    rt_mat_el
        EX      DE,HL                   ; DE -> element
        LD      A,(rt_matprint__f)
        OR      A
        JR      NZ,rt_matprint__fmt
        CALL    rt_print_flt
        JR      rt_matprint__nx
rt_matprint__fmt:
        PUSH    DE
        LD      HL,rt_matprint__mk
        CALL    rt_str_mark
        LD      HL,rt_matprint__n
        LD      (rt_argp),HL
        POP     DE
        LD      BC,rt_matprint__g
        CALL    rt_str_fmt              ; HL = temporary
        EX      DE,HL
        CALL    rt_print_sdesc
        LD      DE,rt_matprint__mk
        CALL    rt_str_release
rt_matprint__nx:
        LD      HL,(rt_matprint__j)
        INC     HL
        LD      (rt_matprint__j),HL
        JR      rt_matprint__nj
rt_matprint__xi:
        LD      DE,rt_matprint__cr
        LD      BC,1
        CALL    rt_print_str
        CALL    rt_print_nl
        LD      HL,(rt_matprint__i)
        INC     HL
        LD      (rt_matprint__i),HL
        JP      rt_matprint__ni

;! section data
rt_matprint__comma:
        DB      ','
rt_matprint__cr:
        DB      13

;! section bss
rt_matprint__f:
        DS      1
rt_matprint__g:
        DS      4
rt_matprint__n:
        DS      4
rt_matprint__mk:
        DS      4
rt_matprint__i:
        DS      2
rt_matprint__j:
        DS      2
