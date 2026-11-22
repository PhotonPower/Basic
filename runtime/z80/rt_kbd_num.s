;! module    rt_kbd_num
;! provides  rt_kbd_num
;! requires  rt_kbd_next rt_kbd_read rt_kbd_prompt rt_kbd_plen rt_val rt_val_used
;! group     input
;! clobbers  AF BC DE HL
;! doc       INPUT into a numeric variable (own module: LINE INPUT, INKEY$ and INPUT into strings need no number conversion).

; HL = result: the next value as a number; not a number: the prompt and the input again
rt_kbd_num:
        LD      (rt_kbd_num__dst),HL
rt_kbd_num__again:
        CALL    rt_kbd_next
rt_kbd_num__tl:                             ; spaces at the end and at the front away
        LD      A,B
        OR      A
        JP      Z,rt_kbd_num__bad
        PUSH    HL
        LD      E,B
        LD      D,0
        ADD     HL,DE
        DEC     HL
        LD      A,(HL)
        POP     HL
        CP      ' '
        JP      NZ,rt_kbd_num__ld
        DEC     B
        JP      rt_kbd_num__tl
rt_kbd_num__ld:
        LD      A,(HL)
        CP      ' '
        JP      NZ,rt_kbd_num__val
        INC     HL
        DEC     B
        JP      rt_kbd_num__ld
rt_kbd_num__val:
        LD      A,B
        LD      (rt_kbd_num__vl),A
        EX      DE,HL
        LD      C,B
        LD      B,0
        LD      HL,(rt_kbd_num__dst)
        CALL    rt_val
        LD      HL,(rt_val_used)        ; all characters must belong to the number
        LD      A,(rt_kbd_num__vl)
        CP      L
        JP      NZ,rt_kbd_num__bad
        LD      A,H
        OR      A
        RET     Z
rt_kbd_num__bad:
        LD      DE,rt_kbd_prompt
        LD      BC,(rt_kbd_plen)
        CALL    rt_kbd_read
        JP      rt_kbd_num__again

;! section bss
rt_kbd_num__vl:
        DS      1
rt_kbd_num__dst:
        DS      2
