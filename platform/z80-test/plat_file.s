;! module    plat_file
;! provides  plat_fopen plat_fgetc plat_fputc plat_fclose plat_fsize plat_fexists plat_fdelete
;! group     file-seq
;! clobbers  AF BC DE HL
;! doc       z80-test: files on the RAM disk of the test driver (port &10, request block).

; Request block: +0 command, +1 mode or handle, +2 status, +4 name address or byte, +6 name length;
; the results (32 bits) at +4. Errors as AmigaDOS (205 object not found, 202 object in use, 221
; disk full).

; A = command -> the request goes out; A = status, Z if it is 0, HL = block + 4
plat_file__req:
        LD      (plat_file__blk),A
        LD      HL,plat_file__blk
        LD      B,H
        LD      C,&10
        OUT     (C),L
        LD      A,(plat_file__blk+2)
        LD      HL,plat_file__blk+4
        OR      A
        RET

; DE = name, BC = its length, A = mode (0 read, 1 write, 2 append) -> CF clear: A = handle (not 0);
; CF set: A = error
plat_fopen:
        LD      (plat_file__blk+1),A
        LD      (plat_file__blk+4),DE
        LD      (plat_file__blk+6),BC
        LD      A,1
plat_file__res:                         ; status 0: A = the result byte, CF clear; else A = status, CF
        CALL    plat_file__req
        SCF
        RET     NZ
        LD      A,(HL)
        OR      A
        RET

; A = handle -> CF clear: A = next byte; CF set at the end of the file
plat_fgetc:
        LD      (plat_file__blk+1),A
        LD      A,2
        JR      plat_file__res

; A = byte, C = handle -> CF set: A = error (disk full); keeps BC DE HL
plat_fputc:
        PUSH    HL
        PUSH    DE
        PUSH    BC
        LD      (plat_file__blk+4),A
        LD      A,C
        LD      (plat_file__blk+1),A
        LD      A,3
        CALL    plat_file__res
        POP     BC
        POP     DE
        POP     HL
        RET

; A = handle
plat_fclose:
        LD      (plat_file__blk+1),A
        LD      A,4
        JR      plat_file__req

; A = handle, HL = result -> (HL) = length of the file (32 bits)
plat_fsize:
        PUSH    HL
        LD      (plat_file__blk+1),A
        LD      A,5
        CALL    plat_file__req
        POP     DE
        LD      BC,4
        LDIR
        RET

; DE = name, BC = its length -> CF set if the file exists
plat_fexists:
        LD      (plat_file__blk+4),DE
        LD      (plat_file__blk+6),BC
        LD      A,6
        CALL    plat_file__req
        LD      A,(HL)
        RRCA
        RET

; DE = name, BC = its length -> CF set: A = error
plat_fdelete:
        LD      (plat_file__blk+4),DE
        LD      (plat_file__blk+6),BC
        LD      A,7
        CALL    plat_file__req
        RET     Z
        SCF
        RET

;! section bss
plat_file__blk:
        DS      8
