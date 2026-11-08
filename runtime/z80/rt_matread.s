;! module    rt_matread
;! provides  rt_mat_read
;! requires  rt_mat_v rt_mat_el rt_data_num
;! group     matrix
;! clobbers  AF BC DE HL
;! doc       MAT READ (D23): view 0 of rt_mat row by row from DATA.

rt_mat_read:
        LD      HL,0
        LD      (rt_matread__i),HL
rt_matread__ni:
        LD      HL,(rt_matread__i)
        LD      DE,(rt_mat_v+2)
        OR      A
        SBC     HL,DE
        RET     NC
        LD      HL,0
        LD      (rt_matread__j),HL
rt_matread__nj:
        LD      HL,(rt_matread__j)
        LD      DE,(rt_mat_v+4)
        OR      A
        SBC     HL,DE
        JR      NC,rt_matread__xi
        LD      HL,rt_mat_v
        LD      BC,(rt_matread__i)
        LD      DE,(rt_matread__j)
        CALL    rt_mat_el
        CALL    rt_data_num
        LD      HL,(rt_matread__j)
        INC     HL
        LD      (rt_matread__j),HL
        JR      rt_matread__nj
rt_matread__xi:
        LD      HL,(rt_matread__i)
        INC     HL
        LD      (rt_matread__i),HL
        JR      rt_matread__ni

;! section bss
rt_matread__i:
        DS      2
rt_matread__j:
        DS      2
