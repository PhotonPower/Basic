;! module    plat_getc
;! provides  plat_getc plat_inkey plat_echo
;! group     core
;! clobbers  AF
;! doc       cpc: keyboard through KM WAIT CHAR (&BB06) and KM READ CHAR (&BB09), echo through TXT OUTPUT.

; -> A = character (Return is a line feed), CF set at the end of the input (only in the test
; harness: the firmware always returns with a character)
plat_getc:
        CALL    &BB06                   ; KM WAIT CHAR: A = character, CF set
        CCF
        RET     C
        CP      13
        RET     NZ
        LD      A,10
        OR      A
        RET

; -> A = character, CF set if no key is waiting (INKEY$)
plat_inkey:
        CALL    &BB09                   ; KM READ CHAR: CF set if there was one
        CCF
        RET

; A = character of an input line on the screen: 10 the end of the line, 8 a deleted character
plat_echo:
        CP      8
        JR      Z,plat_echo__del
        CP      10
        JP      NZ,&BB5A
        LD      A,13
        CALL    &BB5A
        LD      A,10
        JP      &BB5A
plat_echo__del:
        CALL    &BB5A
        LD      A,' '
        CALL    &BB5A
        LD      A,8
        JP      &BB5A
