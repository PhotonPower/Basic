;! module    plat_file
;! provides  plat_fopen plat_fgetc plat_fputc plat_fclose plat_fsize plat_fexists plat_fdelete
;! group     file-seq
;! clobbers  AF BC DE HL
;! doc       cpc: files through AMSDOS (CAS IN/OUT): one input file (handle 1) and one output file (handle 2).

; AMSDOS allows one input and one output file at a time; a second one of a kind is error 202
; (object in use, ERR -54), as a file that is open in the other direction. Append (mode 2) copies the
; old file into the new one, so it needs the input free. Files written with CAS OUT CHAR are ASCII
; files: reading stops at the first &1A. LOF of an input file reads it once more. EXIST and KILL
; look for the file through CAS IN OPEN, so with an open input file only its own name and the output
; file are found, and KILL does not report a missing file. Disc error messages are switched off
; (&BE78), the names are kept in up to 16 characters.

plat_file__NAME EQU 16

; switch off the disc error messages of AMSDOS
plat_file__quiet:
        LD      A,&FF
        LD      (&BE78),A
        RET

; (plat_file__nm), (plat_file__nl) = name; HL -> stored name (length byte, characters) -> Z if equal
; (without regard to case)
plat_file__same:
        LD      A,(plat_file__nl+1)
        OR      A
        RET     NZ                      ; 256 characters and more: never
        LD      A,(plat_file__nl)
        CP      plat_file__NAME+1
        JR      C,plat_file__s0
        LD      A,plat_file__NAME
plat_file__s0:
        CP      (HL)
        RET     NZ
        OR      A
        RET     Z
        LD      B,A
        INC     HL
        LD      DE,(plat_file__nm)
plat_file__s1:
        LD      A,(DE)
        CALL    plat_file__up
        LD      C,A
        LD      A,(HL)
        CALL    plat_file__up
        CP      C
        RET     NZ
        INC     HL
        INC     DE
        DJNZ    plat_file__s1
        RET                             ; Z

plat_file__up:
        CP      'a'
        RET     C
        CP      'z'+1
        RET     NC
        SUB     32
        RET

; store the name (plat_file__nm, nl) at HL (length byte, characters)
plat_file__keep:
        LD      A,(plat_file__nl+1)
        OR      A
        LD      A,plat_file__NAME
        JR      NZ,plat_file__k0
        LD      A,(plat_file__nl)
        CP      plat_file__NAME+1
        JR      C,plat_file__k0
        LD      A,plat_file__NAME
plat_file__k0:
        LD      (HL),A
        INC     HL
        OR      A
        RET     Z
        EX      DE,HL
        LD      HL,(plat_file__nm)
        LD      C,A
        LD      B,0
        LDIR
        RET

; Z if the name is that of the open input (resp. output) file
plat_file__isin:
        LD      A,(plat_file__inop)
        OR      A
        JR      Z,plat_file__nz
        LD      HL,plat_file__inn
        JR      plat_file__same
plat_file__isout:
        LD      A,(plat_file__outop)
        OR      A
        JR      Z,plat_file__nz
        LD      HL,plat_file__outn
        JR      plat_file__same
plat_file__nz:
        OR      1
        RET

; CAS IN OPEN with the name (plat_file__nm, nl) -> CF set if it is open
plat_file__inopen:
        LD      HL,(plat_file__nl)
        LD      A,H
        OR      A
        RET     NZ                      ; (CF clear)
        LD      B,L
        LD      HL,(plat_file__nm)
        LD      DE,plat_file__inbuf
        JP      &BC77                   ; CAS IN OPEN

; DE = name, BC = its length, A = mode (0 read, 1 write, 2 append) -> CF clear: A = handle;
; CF set: A = error
plat_fopen:
        LD      (plat_file__mode),A
        LD      (plat_file__nm),DE
        LD      (plat_file__nl),BC
        CALL    plat_file__quiet
        LD      A,(plat_file__mode)
        OR      A
        JR      NZ,plat_file__ow
        LD      A,(plat_file__inop)     ; read: the input free, not the output file
        OR      A
        JP      NZ,plat_file__use
        CALL    plat_file__isout
        JP      Z,plat_file__use
        CALL    plat_file__inopen
        JP      NC,plat_file__nf
        LD      HL,plat_file__inn
        CALL    plat_file__keep
        LD      HL,0
        LD      (plat_file__inpos),HL
        LD      A,1
        LD      (plat_file__inop),A
        OR      A
        RET
plat_file__ow:                          ; write, append: the output free, not the input file
        LD      A,(plat_file__outop)
        OR      A
        JP      NZ,plat_file__use
        CALL    plat_file__isin
        JP      Z,plat_file__use
        XOR     A
        LD      (plat_file__app),A
        LD      A,(plat_file__mode)
        CP      2
        JR      NZ,plat_file__o2
        LD      A,(plat_file__inop)     ; append: the old file through the input
        OR      A
        JP      NZ,plat_file__use
        CALL    plat_file__inopen
        JR      NC,plat_file__o2        ; none: a new file
        LD      A,1
        LD      (plat_file__app),A
plat_file__o2:
        LD      HL,(plat_file__nl)
        LD      A,H
        OR      A
        JP      NZ,plat_file__full
        LD      B,L
        LD      HL,(plat_file__nm)
        LD      DE,plat_file__outbuf
        CALL    &BC8C                   ; CAS OUT OPEN
        JR      NC,plat_file__oerr
        LD      HL,0
        LD      (plat_file__outn+plat_file__NAME+1),HL
        LD      A,(plat_file__app)
        OR      A
        JR      Z,plat_file__o4
plat_file__cp:                          ; copy the old file
        CALL    &BC80                   ; CAS IN CHAR
        JR      NC,plat_file__o3
        CALL    &BC95                   ; CAS OUT CHAR
        LD      HL,(plat_file__outn+plat_file__NAME+1)
        INC     HL
        LD      (plat_file__outn+plat_file__NAME+1),HL
        JR      plat_file__cp
plat_file__o3:
        CALL    &BC7A                   ; CAS IN CLOSE
plat_file__o4:
        LD      HL,plat_file__outn
        CALL    plat_file__keep
        LD      A,1
        LD      (plat_file__outop),A
        INC     A                       ; handle 2
        OR      A
        RET
plat_file__oerr:
        LD      A,(plat_file__app)
        OR      A
        CALL    NZ,&BC7D                ; CAS IN ABANDON
plat_file__full:
        LD      A,221                   ; disk full (or a bad name)
        SCF
        RET
plat_file__use:
        LD      A,202                   ; object in use
        SCF
        RET
plat_file__nf:
        LD      A,205                   ; object not found
        SCF
        RET

; A = handle -> CF clear: A = next byte; CF set at the end of the file
plat_fgetc:
        CALL    &BC80                   ; CAS IN CHAR: CF set if there is one
        CCF
        RET     C
        LD      HL,(plat_file__inpos)
        INC     HL
        LD      (plat_file__inpos),HL
        RET

; A = byte, C = handle -> CF set: A = error (disk full); keeps BC DE HL
plat_fputc:
        CALL    &BC95                   ; CAS OUT CHAR (keeps BC DE HL)
        JR      NC,plat_file__pf
        PUSH    HL
        LD      HL,(plat_file__outn+plat_file__NAME+1)
        INC     HL
        LD      (plat_file__outn+plat_file__NAME+1),HL
        POP     HL
        OR      A
        RET
plat_file__pf:
        LD      A,221
        SCF
        RET

; A = handle
plat_fclose:
        CP      1
        JR      NZ,plat_file__c2
        XOR     A
        LD      (plat_file__inop),A
        JP      &BC7A                   ; CAS IN CLOSE
plat_file__c2:
        XOR     A
        LD      (plat_file__outop),A
        JP      &BC8F                   ; CAS OUT CLOSE

; A = handle, HL = result -> (HL) = length of the file (32 bits)
plat_fsize:
        LD      (plat_file__res),HL
        CP      1
        JR      Z,plat_file__z1
        LD      DE,(plat_file__outn+plat_file__NAME+1)
        LD      BC,0
        JR      plat_file__zw
plat_file__z1:                          ; input: once more from the start, then back to the position
        CALL    &BC7A                   ; CAS IN CLOSE
        CALL    plat_file__reopen
        LD      BC,0                    ; BC:DE = count
        LD      D,B
        LD      E,C
plat_file__zc:
        CALL    &BC80
        JR      NC,plat_file__ze
        INC     DE
        LD      A,D
        OR      E
        JR      NZ,plat_file__zc
        INC     BC
        JR      plat_file__zc
plat_file__ze:
        PUSH    BC
        PUSH    DE
        CALL    &BC7A
        CALL    plat_file__reopen
        LD      BC,(plat_file__inpos)
plat_file__zs:
        LD      A,B
        OR      C
        JR      Z,plat_file__zd
        CALL    &BC80
        DEC     BC
        JR      plat_file__zs
plat_file__zd:
        POP     DE
        POP     BC
plat_file__zw:
        LD      HL,(plat_file__res)
        LD      (HL),E
        INC     HL
        LD      (HL),D
        INC     HL
        LD      (HL),C
        INC     HL
        LD      (HL),B
        RET

; CAS IN OPEN of the stored input name
plat_file__reopen:
        LD      HL,plat_file__inn
        LD      B,(HL)
        INC     HL
        LD      DE,plat_file__inbuf
        JP      &BC77

; DE = name, BC = its length -> CF set if the file exists
plat_fexists:
        LD      (plat_file__nm),DE
        LD      (plat_file__nl),BC
        CALL    plat_file__quiet
        CALL    plat_file__isin
        JR      Z,plat_file__yes
        CALL    plat_file__isout
        JR      Z,plat_file__yes
        LD      A,(plat_file__inop)
        OR      A
        RET     NZ                      ; (CF clear) not to be found while the input is open
        CALL    plat_file__inopen
        RET     NC
        CALL    &BC7D                   ; CAS IN ABANDON
plat_file__yes:
        SCF
        RET

; DE = name, BC = its length -> CF set: A = error
plat_fdelete:
        LD      (plat_file__nm),DE
        LD      (plat_file__nl),BC
        CALL    plat_file__isin
        JP      Z,plat_file__use
        CALL    plat_file__isout
        JP      Z,plat_file__use
        LD      A,(plat_file__inop)
        OR      A
        JR      NZ,plat_file__era       ; cannot look: delete without a check
        LD      DE,(plat_file__nm)
        LD      BC,(plat_file__nl)
        CALL    plat_fexists
        JP      NC,plat_file__nf
plat_file__era:
        LD      HL,(plat_file__nl)      ; the string descriptor for |ERA: length, address
        LD      A,H
        OR      A
        JP      NZ,plat_file__nf
        LD      A,L
        LD      (plat_file__sd),A
        LD      HL,(plat_file__nm)
        LD      (plat_file__sd+1),HL
        LD      HL,plat_file__sd
        LD      (plat_file__par),HL
        LD      HL,plat_file__era_n
        CALL    &BCD4                   ; KL FIND COMMAND: HL = routine, C = ROM
        JP      NC,plat_file__nf
        PUSH    IX
        LD      IX,plat_file__par
        LD      DE,(plat_file__par)
        LD      A,1
        CALL    &001B                   ; KL FAR PCHL: |ERA,name$
        POP     IX
        OR      A
        RET

;! section data
plat_file__era_n:
        DB      "ER",193                ; "A" with bit 7

;! section bss
plat_file__mode:
        DS      1
plat_file__app:
        DS      1
plat_file__nm:
        DS      2
plat_file__nl:
        DS      2
plat_file__res:
        DS      2
plat_file__inop:
        DS      1
plat_file__outop:
        DS      1
plat_file__inpos:
        DS      2
plat_file__inn:                         ; length, characters
        DS      plat_file__NAME+1
plat_file__outn:                        ; length, characters, then the bytes written (2)
        DS      plat_file__NAME+3
plat_file__sd:
        DS      3
plat_file__par:
        DS      2
plat_file__inbuf:
        DS      2048
plat_file__outbuf:
        DS      2048
