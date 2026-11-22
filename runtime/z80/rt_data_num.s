;! module    rt_data_num
;! provides  rt_data_num
;! requires  rt_data_item rt_data_pos rt_error rt_val rt_val_used
;! group     core
;! clobbers  AF BC DE HL
;! doc       READ into a numeric variable (own module: reading only strings needs no number conversion).

; HL = result: the value of the next item
rt_data_num:
        LD      (rt_data_num__dst),HL
        CALL    rt_data_item
        JR      C,rt_data_num__zero
        LD      (rt_data_num__it),HL
        LD      E,(HL)                  ; DE = characters, BC = length
        INC     HL
        LD      D,(HL)
        INC     HL
        LD      C,(HL)
        INC     HL
        LD      B,(HL)
        INC     HL
        LD      A,(HL)                  ; quoted or empty: not numeric
        OR      A
        JR      NZ,rt_data_num__nn
        LD      A,B
        OR      C
        JR      Z,rt_data_num__nn
        LD      HL,(rt_data_num__dst)
        CALL    rt_val
        LD      HL,(rt_data_num__it)        ; all characters must belong to the number
        INC     HL
        INC     HL
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        LD      HL,(rt_val_used)
        OR      A
        SBC     HL,DE
        JR      NZ,rt_data_num__nn
        LD      DE,(rt_data_pos)
        INC     DE
        LD      (rt_data_pos),DE
        RET
rt_data_num__nn:
        LD      A,35                    ; data not numeric
        CALL    rt_error
rt_data_num__zero:
        LD      HL,(rt_data_num__dst)
        LD      B,8
rt_data_num__z:
        LD      (HL),0
        INC     HL
        DJNZ    rt_data_num__z
        RET

;! section bss
rt_data_num__dst:
        DS      2
rt_data_num__it:
        DS      2
