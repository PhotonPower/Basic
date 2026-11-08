;! module    rt_data
;! provides  rt_data_str rt_data_num rt_data_restore
;! requires  rt_error rt_val rt_val_used prg_data prg_data_count
;! group     core
;! clobbers  AF BC DE HL
;! doc       DATA, READ, RESTORE: items in the program's table prg_data.

; prg_data: 6 bytes per item (address, length: a string descriptor; then 1 if quoted), prg_data_count
; (16 bit). Errors: 34 out of data, 35 data not numeric (the item stays the next one).

; -> HL = descriptor of the next item (for a string variable)
rt_data_str:
        CALL    rt_data__item
        RET     C
        LD      DE,(rt_data__pos)
        INC     DE
        LD      (rt_data__pos),DE
        RET

; HL = result: the value of the next item
rt_data_num:
        LD      (rt_data__dst),HL
        CALL    rt_data__item
        JR      C,rt_data__zero
        LD      (rt_data__it),HL
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
        JR      NZ,rt_data__nn
        LD      A,B
        OR      C
        JR      Z,rt_data__nn
        LD      HL,(rt_data__dst)
        CALL    rt_val
        LD      HL,(rt_data__it)        ; all characters must belong to the number
        INC     HL
        INC     HL
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        LD      HL,(rt_val_used)
        OR      A
        SBC     HL,DE
        JR      NZ,rt_data__nn
        LD      DE,(rt_data__pos)
        INC     DE
        LD      (rt_data__pos),DE
        RET
rt_data__nn:
        LD      A,35                    ; data not numeric
        CALL    rt_error
rt_data__zero:
        LD      HL,(rt_data__dst)
        LD      B,8
rt_data__z:
        LD      (HL),0
        INC     HL
        DJNZ    rt_data__z
        RET

; DE -> index of the next item
rt_data_restore:
        EX      DE,HL
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        LD      (rt_data__pos),DE
        RET

; -> HL = the item at the read position; CF set (after error 34) if there is none
rt_data__item:
        LD      HL,(rt_data__pos)
        LD      DE,(prg_data_count)
        OR      A
        SBC     HL,DE
        JR      C,rt_data__ok
        LD      A,34                    ; out of data
        CALL    rt_error
        LD      HL,rt_data__empty
        SCF
        RET
rt_data__ok:
        LD      HL,(rt_data__pos)
        LD      D,H
        LD      E,L
        ADD     HL,HL
        ADD     HL,DE
        ADD     HL,HL                   ; 6 * pos
        LD      DE,prg_data
        ADD     HL,DE
        OR      A
        RET

;! section data
rt_data__pos:
        DW      0
rt_data__empty:
        DB      0,0,0,0,0,0

;! section bss
rt_data__dst:
        DS      2
rt_data__it:
        DS      2
