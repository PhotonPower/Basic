;! module    rt_kbd
;! provides  rt_kbd_begin rt_kbd_field rt_kbd_num rt_kbd_line rt_kbd_end rt_kbd_inkey rt_kbd_chars
;! requires  plat_getc rt_print_str rt_print_col rt_str_newtemp rt_val rt_val_used rt_error
;! group     input
;! clobbers  AF BC DE HL
;! doc       Keyboard: INPUT, LINE INPUT, INKEY$, INPUT$(n) (semantics of brt::Keyboard; lines up to 255 characters).

; INPUT/LINE INPUT read lines through plat_getc. Prompt: "text? " (mode 1, "text";), "text" (mode 2,
; "text",), "? " (mode 0, no text). INPUT values are separated by commas; missing values come from
; further lines ("? "); a value that is not a number repeats the prompt and the input. INKEY$ and
; INPUT$(n) read characters, a line feed counts as Return (13). At the end: error 26. A line keeps
; 255 characters (D30), the rest is passed over.

rt_kbd__MAX EQU 255

; DE = prompt text (descriptor), BC -> mode: print the prompt, read a line
rt_kbd_begin:
        LD      A,(BC)
        LD      (rt_kbd__mode),A
        LD      HL,rt_kbd__prompt       ; the prompt into its own buffer (for a repetition)
        LD      (rt_kbd__pp),HL
        OR      A
        JP      Z,rt_kbd__q
        EX      DE,HL
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        INC     HL
        LD      C,(HL)
        INC     HL
        LD      B,(HL)                  ; DE = text, BC = length (at most 250)
        LD      A,B
        OR      A
        JP      NZ,rt_kbd__cut
        LD      A,C
        CP      251
        JP      C,rt_kbd__lk
rt_kbd__cut:
        LD      BC,250
rt_kbd__lk:
        LD      HL,rt_kbd__prompt
        EX      DE,HL
        LD      A,B
        OR      C
        JP      Z,rt_kbd__l0
        LDIR
rt_kbd__l0:
        LD      (rt_kbd__pp),DE
        LD      A,(rt_kbd__mode)
        CP      2
        JP      Z,rt_kbd__pd
rt_kbd__q:
        LD      HL,(rt_kbd__pp)
        LD      (HL),'?'
        INC     HL
        LD      (HL),' '
        INC     HL
        LD      (rt_kbd__pp),HL
rt_kbd__pd:
        LD      HL,(rt_kbd__pp)
        LD      DE,rt_kbd__prompt
        OR      A
        SBC     HL,DE
        LD      (rt_kbd__plen),HL
        LD      DE,rt_kbd__prompt
        LD      B,H
        LD      C,L
        JP      rt_kbd__read

; DE = prompt, BC = length: print it, read a line into the buffer (error 26 at the end)
rt_kbd__read:
        CALL    rt_print_str
        LD      B,0                     ; B = length
rt_kbd__rl:
        PUSH    BC
        CALL    plat_getc
        POP     BC
        JP      C,rt_kbd__eof
        CP      10
        JP      Z,rt_kbd__eol
        LD      C,A
        LD      A,B
        CP      rt_kbd__MAX
        JP      NC,rt_kbd__rl
        LD      E,B
        LD      D,0
        LD      HL,rt_kbd__buf
        ADD     HL,DE
        LD      (HL),C
        INC     B
        JP      rt_kbd__rl
rt_kbd__eof:
        LD      A,B                     ; the end before any character: error 26
        OR      A
        JP      NZ,rt_kbd__eol
        PUSH    BC
        LD      A,26
        CALL    rt_error
        POP     BC
rt_kbd__eol:
        LD      A,B                     ; a CR before the LF away
        OR      A
        JP      Z,rt_kbd__set
        LD      E,B
        LD      D,0
        LD      HL,rt_kbd__buf-1
        ADD     HL,DE
        LD      A,(HL)
        CP      13
        JP      NZ,rt_kbd__set
        DEC     B
rt_kbd__set:
        LD      A,B
        LD      (rt_kbd__len),A
        XOR     A
        LD      (rt_kbd__pos),A
        INC     A
        LD      (rt_kbd__whole),A
        LD      (rt_kbd__have),A
        RET

; -> HL = start, B = length of the next field (a new line with "? " if none is left)
rt_kbd__next:
        LD      A,(rt_kbd__have)
        OR      A
        JP      NZ,rt_kbd__go
        LD      DE,rt_kbd__qm
        LD      BC,2
        CALL    rt_kbd__read
rt_kbd__go:
        XOR     A
        LD      (rt_kbd__whole),A
        LD      A,(rt_kbd__pos)
        LD      E,A
        LD      D,0
        LD      HL,rt_kbd__buf
        ADD     HL,DE                   ; HL = start of the field
        LD      A,(rt_kbd__len)
        SUB     E
        LD      C,A                     ; C = characters left
        LD      B,0
rt_kbd__f:
        LD      A,B
        CP      C
        JP      NC,rt_kbd__last
        PUSH    HL
        LD      E,B
        LD      D,0
        ADD     HL,DE
        LD      A,(HL)
        POP     HL
        CP      ','
        JP      Z,rt_kbd__comma
        INC     B
        JP      rt_kbd__f
rt_kbd__last:
        XOR     A                       ; no field left after this one
        LD      (rt_kbd__have),A
        RET
rt_kbd__comma:
        LD      A,(rt_kbd__pos)
        ADD     A,B
        INC     A
        LD      (rt_kbd__pos),A
        RET

; HL = characters, B = count -> HL = temporary string with them
rt_kbd__temp:
        PUSH    HL
        LD      C,B
        LD      B,0
        PUSH    BC
        CALL    rt_str_newtemp          ; HL = descriptor, DE = characters
        POP     BC
        EX      (SP),HL                 ; HL = source, stack = descriptor
        LD      A,B
        OR      C
        JP      Z,rt_kbd__t9
        LDIR
rt_kbd__t9:
        POP     HL
        RET

; -> HL = the next value as a string (INPUT into a string variable)
rt_kbd_field:
        CALL    rt_kbd__next
        JP      rt_kbd__temp

; HL = result: the next value as a number; not a number: the prompt and the input again
rt_kbd_num:
        LD      (rt_kbd__dst),HL
rt_kbd__again:
        CALL    rt_kbd__next
rt_kbd__tl:                             ; spaces at the end and at the front away
        LD      A,B
        OR      A
        JP      Z,rt_kbd__bad
        PUSH    HL
        LD      E,B
        LD      D,0
        ADD     HL,DE
        DEC     HL
        LD      A,(HL)
        POP     HL
        CP      ' '
        JP      NZ,rt_kbd__ld
        DEC     B
        JP      rt_kbd__tl
rt_kbd__ld:
        LD      A,(HL)
        CP      ' '
        JP      NZ,rt_kbd__val
        INC     HL
        DEC     B
        JP      rt_kbd__ld
rt_kbd__val:
        LD      A,B
        LD      (rt_kbd__vl),A
        EX      DE,HL
        LD      C,B
        LD      B,0
        LD      HL,(rt_kbd__dst)
        CALL    rt_val
        LD      HL,(rt_val_used)        ; all characters must belong to the number
        LD      A,(rt_kbd__vl)
        CP      L
        JP      NZ,rt_kbd__bad
        LD      A,H
        OR      A
        RET     Z
rt_kbd__bad:
        LD      DE,rt_kbd__prompt
        LD      BC,(rt_kbd__plen)
        CALL    rt_kbd__read
        JP      rt_kbd__again

; -> HL = the whole line (LINE INPUT; for further variables a new line with "? ")
rt_kbd_line:
        LD      A,(rt_kbd__whole)
        OR      A
        JP      NZ,rt_kbd__take
        LD      DE,rt_kbd__qm
        LD      BC,2
        CALL    rt_kbd__read
rt_kbd__take:
        XOR     A
        LD      (rt_kbd__whole),A
        LD      (rt_kbd__have),A
        LD      HL,rt_kbd__buf
        LD      A,(rt_kbd__len)
        LD      B,A
        JP      rt_kbd__temp

; end of INPUT: the Return moved the cursor to the start of the next line
rt_kbd_end:
        LD      HL,0
        LD      (rt_print_col),HL
        RET

; -> HL = INKEY$: one character, or "" at the end of the input
rt_kbd_inkey:
        CALL    plat_getc
        LD      B,0
        JP      C,rt_kbd__in0
        CALL    rt_kbd__cr
        LD      (rt_kbd__ch),A
        LD      B,1
rt_kbd__in0:
        LD      HL,rt_kbd__ch
        JP      rt_kbd__temp

; DE -> n: INPUT$(n): the next n characters (error 26 at the end; at most 255)
rt_kbd_chars:
        EX      DE,HL
        LD      C,(HL)
        INC     HL
        LD      A,(HL)
        INC     HL
        OR      (HL)
        INC     HL
        OR      (HL)
        JP      NZ,rt_kbd__cmax
        LD      A,C
        JP      rt_kbd__cn
rt_kbd__cmax:
        LD      A,(HL)                  ; negative: none, large: 255
        BIT     7,A
        LD      A,0
        JP      NZ,rt_kbd__cn
        LD      A,rt_kbd__MAX
rt_kbd__cn:
        LD      (rt_kbd__cnt),A
        LD      B,0
rt_kbd__c1:
        LD      A,(rt_kbd__cnt)
        CP      B
        JP      Z,rt_kbd__cd
        PUSH    BC
        CALL    plat_getc
        POP     BC
        JP      C,rt_kbd__ceof
        CALL    rt_kbd__cr
        LD      E,B
        LD      D,0
        LD      HL,rt_kbd__buf
        ADD     HL,DE
        LD      (HL),A
        INC     B
        JP      rt_kbd__c1
rt_kbd__ceof:
        PUSH    BC
        LD      A,26
        CALL    rt_error
        POP     BC
rt_kbd__cd:
        LD      HL,rt_kbd__buf
        JP      rt_kbd__temp

; A = character: a line feed becomes Return (13)
rt_kbd__cr:
        CP      10
        RET     NZ
        LD      A,13
        RET

;! section data
rt_kbd__have:
        DB      0
rt_kbd__qm:
        DB      "? "

;! section bss
rt_kbd__mode:
        DS      1
rt_kbd__pp:
        DS      2
rt_kbd__plen:
        DS      2
rt_kbd__len:
        DS      1
rt_kbd__pos:
        DS      1
rt_kbd__whole:
        DS      1
rt_kbd__ch:
        DS      1
rt_kbd__cnt:
        DS      1
rt_kbd__vl:
        DS      1
rt_kbd__dst:
        DS      2
rt_kbd__prompt:
        DS      254
rt_kbd__buf:
        DS      256
