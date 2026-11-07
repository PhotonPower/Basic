;! module    plat_file
;! provides  plat_fopen plat_fread plat_fwrite plat_fclose plat_fsize plat_fdelete plat_fexists
;! requires  plat_dosbase
;! group     file-seq
;! clobbers  D0-D3/A0-A1
;! doc       amiga: files through dos.library (Open, Read, Write, Seek, Close, DeleteFile, Lock); errors from IoErr().

; dos.library: Open -30, Close -36, Read -42, Write -48, Seek -66, DeleteFile -72, Lock -84,
; UnLock -90, IoErr -132. Errors are the AmigaDOS codes (205 object not found, 202 object in use ...).

; A0 = name, D0 = length -> D1 = the name as a C string (at most 255 characters)
plat_file__cname:
        LEA     plat_file__nb,A1
        CMP.L   #255,D0
        BLS     .n
        MOVE.L  #255,D0
.n:     MOVE.L  A1,D1
        BRA     .cn
.c:     MOVE.B  (A0)+,(A1)+
.cn:    SUBQ.L  #1,D0
        BPL     .c
        CLR.B   (A1)
        RTS

; A0 = name, D0 = length, D1 = mode (0 read, 1 write, 2 append) -> D0 = handle, or 0 with D1 = error
plat_fopen:
        MOVEM.L D4/A6,-(SP)
        MOVE.L  D1,D4
        BSR     plat_file__cname
        MOVE.L  plat_dosbase,A6
        MOVE.L  #1006,D2                ; MODE_NEWFILE
        CMP.L   #1,D4
        BEQ     .open
        MOVE.L  #1005,D2                ; MODE_OLDFILE
.open:  JSR     -30(A6)
        TST.L   D0
        BNE     .opened
        CMP.L   #2,D4                   ; append to a file that does not exist: create it
        BNE     .err
        JSR     -132(A6)
        CMP.L   #205,D0
        BNE     .err0
        MOVE.L  #plat_file__nb,D1
        MOVE.L  #1006,D2
        JSR     -30(A6)
        TST.L   D0
        BEQ     .err
        BRA     .out
.opened:
        CMP.L   #2,D4                   ; append: to the end
        BNE     .out
        MOVE.L  D0,D4
        MOVE.L  D0,D1
        MOVEQ   #0,D2
        MOVEQ   #1,D3                   ; OFFSET_END
        JSR     -66(A6)
        MOVE.L  D4,D0
        BRA     .out
.err:   JSR     -132(A6)                ; IoErr
.err0:  MOVE.L  D0,D1
        MOVEQ   #0,D0
.out:   MOVEM.L (SP)+,D4/A6
        RTS

; D0 = handle, A0 = buffer, D1 = count -> D0 = bytes read (0 at the end)
plat_fread:
        MOVE.L  A6,-(SP)
        MOVE.L  D1,D3
        MOVE.L  A0,D2
        MOVE.L  D0,D1
        MOVE.L  plat_dosbase,A6
        JSR     -42(A6)
        TST.L   D0
        BPL     .out
        MOVEQ   #0,D0
.out:   MOVE.L  (SP)+,A6
        RTS

; D0 = handle, A0 = buffer, D1 = count -> D0 = bytes written
plat_fwrite:
        MOVE.L  A6,-(SP)
        MOVE.L  D1,D3
        MOVE.L  A0,D2
        MOVE.L  D0,D1
        MOVE.L  plat_dosbase,A6
        JSR     -48(A6)
        TST.L   D0
        BPL     .out
        MOVEQ   #0,D0
.out:   MOVE.L  (SP)+,A6
        RTS

; D0 = handle
plat_fclose:
        MOVE.L  A6,-(SP)
        MOVE.L  D0,D1
        MOVE.L  plat_dosbase,A6
        JSR     -36(A6)
        MOVE.L  (SP)+,A6
        RTS

; D0 = handle -> D0 = length (Seek to the end returns the old position, Seek back the end)
plat_fsize:
        MOVEM.L D4/A6,-(SP)
        MOVE.L  D0,D4
        MOVE.L  plat_dosbase,A6
        MOVE.L  D4,D1
        MOVEQ   #0,D2
        MOVEQ   #1,D3                   ; OFFSET_END
        JSR     -66(A6)
        MOVE.L  D0,D2
        MOVE.L  D4,D1
        MOVEQ   #-1,D3                  ; OFFSET_BEGINNING
        JSR     -66(A6)
        MOVEM.L (SP)+,D4/A6
        RTS

; A0 = name, D0 = length -> D0 = 0, or the error
plat_fdelete:
        MOVE.L  A6,-(SP)
        BSR     plat_file__cname
        MOVE.L  plat_dosbase,A6
        JSR     -72(A6)
        TST.L   D0
        BEQ     .err
        MOVEQ   #0,D0
        BRA     .out
.err:   JSR     -132(A6)
.out:   MOVE.L  (SP)+,A6
        RTS

; A0 = name, D0 = length -> D0 = -1 if it exists, else 0
plat_fexists:
        MOVE.L  A6,-(SP)
        BSR     plat_file__cname
        MOVE.L  plat_dosbase,A6
        MOVEQ   #-2,D2                  ; ACCESS_READ
        JSR     -84(A6)
        TST.L   D0
        BEQ     .out
        MOVE.L  D0,D1
        JSR     -90(A6)                 ; UnLock
        MOVEQ   #-1,D0
.out:   MOVE.L  (SP)+,A6
        RTS

;! section bss
plat_file__nb:
        DS.B    256
