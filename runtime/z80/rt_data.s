;! module    rt_data
;! provides  rt_data_str rt_data_restore rt_data_item rt_data_pos
;! requires  rt_error prg_data prg_data_count
;! group     core
;! clobbers  AF BC DE HL
;! doc       DATA, READ, RESTORE: items in the program's table prg_data.

; prg_data: 6 bytes per item (address, length: a string descriptor; then 1 if quoted), prg_data_count
; (16 bit). Errors: 34 out of data, 35 data not numeric (the item stays the next one).

; -> HL = descriptor of the next item (for a string variable)
rt_data_str:
        CALL    rt_data_item
        RET     C
        LD      DE,(rt_data_pos)
        INC     DE
        LD      (rt_data_pos),DE
        RET

; DE -> index of the next item
rt_data_restore:
        EX      DE,HL
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        LD      (rt_data_pos),DE
        RET

; -> HL = the item at the read position; CF set (after error 34) if there is none (also for
; rt_data_num, the next one is rt_data_pos + 1)
rt_data_item:
        LD      HL,(rt_data_pos)
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
        LD      HL,(rt_data_pos)
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
rt_data_pos:
        DW      0
rt_data__empty:
        DB      0,0,0,0,0,0

