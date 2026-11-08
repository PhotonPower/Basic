;! module    rt_write
;! provides  rt_write_str
;! requires  rt_print_str rt_print_sdesc
;! group     print
;! clobbers  AF BC DE HL
;! doc       WRITE: DE = string descriptor, printed in quotes.

rt_write_str:
        PUSH    DE
        LD      DE,rt_write__q
        LD      BC,1
        CALL    rt_print_str
        POP     DE
        CALL    rt_print_sdesc
        LD      DE,rt_write__q
        LD      BC,1
        JP      rt_print_str

;! section data
rt_write__q:
        DB      34
