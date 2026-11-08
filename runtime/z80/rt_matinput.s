;! module    rt_matinput
;! provides  rt_mat_input
;! requires  rt_mat_v rt_mat_el rt_file_num
;! group     matrix
;! clobbers  AF BC DE HL
;! doc       MAT INPUT #c (D23, probe18): view 0 of rt_mat row by row, numbers like INPUT #.

; DE -> channel; after an error the values read so far stay (probe18)
rt_mat_input:
        LD      (rt_matinput__ch),DE
        LD      HL,0
        LD      (rt_matinput__i),HL
rt_matinput__ni:
        LD      HL,(rt_matinput__i)
        LD      DE,(rt_mat_v+2)
        OR      A
        SBC     HL,DE
        RET     NC
        LD      HL,0
        LD      (rt_matinput__j),HL
rt_matinput__nj:
        LD      HL,(rt_matinput__j)
        LD      DE,(rt_mat_v+4)
        OR      A
        SBC     HL,DE
        JR      NC,rt_matinput__xi
        LD      HL,rt_mat_v
        LD      BC,(rt_matinput__i)
        LD      DE,(rt_matinput__j)
        CALL    rt_mat_el
        PUSH    HL                      ; first into a cell: after an error the element stays (probe18 T5)
        LD      HL,rt_matinput__x
        LD      DE,(rt_matinput__ch)
        CALL    rt_file_num
        POP     DE
        LD      HL,rt_matinput__x
        LD      BC,8
        LDIR
        LD      HL,(rt_matinput__j)
        INC     HL
        LD      (rt_matinput__j),HL
        JR      rt_matinput__nj
rt_matinput__xi:
        LD      HL,(rt_matinput__i)
        INC     HL
        LD      (rt_matinput__i),HL
        JR      rt_matinput__ni

;! section bss
rt_matinput__ch:
        DS      2
rt_matinput__x:
        DS      8
rt_matinput__i:
        DS      2
rt_matinput__j:
        DS      2
