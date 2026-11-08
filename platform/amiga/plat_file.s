;! module    plat_file
;! provides  plat_fopen plat_fread plat_fwrite plat_fclose plat_fsize plat_fdelete plat_fexists plat_fseek plat_ftell plat_frename
;! provides  plat_mkdir plat_chdir plat_dir_open plat_dir_next
;! requires  plat_dosbase plat_exit_dir
;! group     file-seq
;! clobbers  D0-D3/A0-A1
;! doc       amiga: files through dos.library (Open, Read, Write, Seek, Close, DeleteFile, Lock); errors from IoErr().

; dos.library: Open -30, Close -36, Read -42, Write -48, Seek -66, DeleteFile -72, Lock -84,
; UnLock -90, IoErr -132, Rename -78. Errors are the AmigaDOS codes (205 object not found, 202 object in use ...).

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

; A0 = name, D0 = length, D1 = mode (0 read, 1 write, 2 append, 3 read and write) -> D0 = handle, or
; 0 with D1 = error. Modes 2 and 3 open an existing file (MODE_OLDFILE) or create it.
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
        CMP.L   #2,D4                   ; append, read/write: a file that does not exist is created
        BCS     .err
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

; D0 = handle, D1 = position (within the file)
plat_fseek:
        MOVE.L  A6,-(SP)
        MOVE.L  D1,D2
        MOVE.L  D0,D1
        MOVEQ   #-1,D3                  ; OFFSET_BEGINNING
        MOVE.L  plat_dosbase,A6
        JSR     -66(A6)
        MOVEQ   #0,D0
        MOVE.L  (SP)+,A6
        RTS

; D0 = handle -> D0 = position
plat_ftell:
        MOVE.L  A6,-(SP)
        MOVE.L  D0,D1
        MOVEQ   #0,D2
        MOVEQ   #0,D3                   ; OFFSET_CURRENT: returns the position
        MOVE.L  plat_dosbase,A6
        JSR     -66(A6)
        MOVE.L  (SP)+,A6
        RTS

; A0 = old name, D0 = length, A1 = new name, D1 = length -> D0 = 0, or the error
plat_frename:
        MOVEM.L D4/A6,-(SP)
        MOVE.L  A1,-(SP)
        MOVE.L  D1,-(SP)
        BSR     plat_file__cname        ; old name into plat_file__nb
        MOVE.L  (SP)+,D0
        MOVE.L  (SP)+,A0
        LEA     plat_file__nb2,A1       ; new name into plat_file__nb2
        CMP.L   #255,D0
        BLS     .n
        MOVE.L  #255,D0
.n:     MOVE.L  A1,D2
        BRA     .cn
.c:     MOVE.B  (A0)+,(A1)+
.cn:    SUBQ.L  #1,D0
        BPL     .c
        CLR.B   (A1)
        MOVE.L  #plat_file__nb,D1
        MOVE.L  plat_dosbase,A6
        JSR     -78(A6)                 ; Rename
        TST.L   D0
        BEQ     .err
        MOVEQ   #0,D0
        BRA     .out
.err:   JSR     -132(A6)
.out:   MOVEM.L (SP)+,D4/A6
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

; ---- directories (probe25): CreateDir -120, Lock -84, UnLock -90, CurrentDir -126, Examine -102,
; ExNext -108. RMDIR is DeleteFile (plat_fdelete): 216 for a full directory, a file is deleted.

; A0 = name, D0 = length -> D0 = 0, or the error; an existing object is no error (probe25 T3)
plat_mkdir:
        MOVE.L  A6,-(SP)
        BSR     plat_file__cname
        MOVE.L  plat_dosbase,A6
        JSR     -120(A6)                ; CreateDir
        TST.L   D0
        BEQ     .err
        MOVE.L  D0,D1
        JSR     -90(A6)                 ; UnLock
        MOVEQ   #0,D0
        BRA     .out
.err:   JSR     -132(A6)
        CMP.L   #203,D0                 ; object exists
        BNE     .out
        MOVEQ   #0,D0
.out:   MOVE.L  (SP)+,A6
        RTS

; -> A0 = the FileInfoBlock (260 bytes, longword aligned)
plat_file__fib:
        LEA     plat_file__fibm+3,A0
        MOVE.L  A0,D0
        AND.W   #$FFFC,D0
        MOVE.L  D0,A0
        RTS

; D1 = lock -> D0 = 0 if it is a directory, else the error (the lock stays)
plat_file__isdir:
        MOVE.L  D1,-(SP)
        BSR     plat_file__fib
        MOVE.L  A0,D2
        MOVE.L  (SP)+,D1
        MOVE.L  D2,-(SP)
        JSR     -102(A6)                ; Examine
        MOVE.L  (SP)+,A0
        TST.L   D0
        BEQ     .err
        TST.L   4(A0)                   ; fib_DirEntryType > 0: a directory
        BLE     .type
        MOVEQ   #0,D0
        RTS
.err:   JMP     -132(A6)
.type:  MOVE.L  #212,D0                 ; object wrong type
        RTS

; A0 = name, D0 = length -> D0 = 0 (the current directory), or the error. The directory of the
; start comes back at the end (plat_exit_dir).
plat_chdir:
        MOVEM.L D4/A6,-(SP)
        BSR     plat_file__cname
        MOVE.L  plat_dosbase,A6
        MOVEQ   #-2,D2                  ; SHARED_LOCK
        JSR     -84(A6)
        MOVE.L  D0,D4
        BEQ     .ioerr
        MOVE.L  D4,D1
        BSR     plat_file__isdir
        TST.L   D0
        BNE     .unlock
        MOVE.L  D4,D1
        JSR     -126(A6)                ; CurrentDir -> the old lock
        TST.L   plat_file__cdset
        BNE     .free
        MOVE.L  D0,plat_file__cd0       ; the first CHDIR: keep the lock of the start
        MOVE.L  #1,plat_file__cdset
        MOVE.L  #plat_file__cdback,plat_exit_dir
        MOVEQ   #0,D0
        BRA     .out
.free:  MOVE.L  D0,D1
        JSR     -90(A6)
        MOVEQ   #0,D0
        BRA     .out
.unlock:
        MOVE.L  D0,-(SP)
        MOVE.L  D4,D1
        JSR     -90(A6)
        MOVE.L  (SP)+,D0
        BRA     .out
.ioerr: JSR     -132(A6)
.out:   MOVEM.L (SP)+,D4/A6
        RTS

; at the end: back to the directory of the start, our lock given back
plat_file__cdback:
        MOVE.L  A6,-(SP)
        MOVE.L  plat_dosbase,A6
        MOVE.L  plat_file__cd0,D1
        JSR     -126(A6)
        MOVE.L  D0,D1
        JSR     -90(A6)
        CLR.L   plat_file__cdset
        MOVE.L  (SP)+,A6
        RTS

; A0 = path, D0 = length -> D0 = 0 (the listing begins), or the error
plat_dir_open:
        MOVE.L  A6,-(SP)
        BSR     plat_dir__close
        BSR     plat_file__cname
        MOVE.L  plat_dosbase,A6
        MOVEQ   #-2,D2
        JSR     -84(A6)
        MOVE.L  D0,plat_file__dlock
        BEQ     .ioerr
        MOVE.L  D0,D1
        BSR     plat_file__isdir
        TST.L   D0
        BEQ     .out
        MOVE.L  D0,-(SP)
        BSR     plat_dir__close
        MOVE.L  (SP)+,D0
        BRA     .out
.ioerr: JSR     -132(A6)
.out:   MOVE.L  (SP)+,A6
        RTS

plat_dir__close:
        MOVE.L  plat_file__dlock,D1
        BEQ     .out
        CLR.L   plat_file__dlock
        MOVE.L  A6,-(SP)
        MOVE.L  plat_dosbase,A6
        JSR     -90(A6)
        MOVE.L  (SP)+,A6
.out:   RTS

; -> D0 = 0 at the end, else 1 with A0 = name (C string), D1 = its length, D2 = size, D3 = 1 for a
; directory, A1 = date (fib_DateStamp: days since 1.1.1978, minutes, ticks)
plat_dir_next:
        MOVE.L  A6,-(SP)
        MOVE.L  plat_file__dlock,D1
        BEQ     .end
        BSR     plat_file__fib
        MOVE.L  A0,D2
        MOVE.L  plat_dosbase,A6
        JSR     -108(A6)                ; ExNext
        TST.L   D0
        BEQ     .done
        BSR     plat_file__fib
        MOVE.L  A0,A1
        MOVE.L  124(A1),D2              ; fib_Size
        MOVEQ   #0,D3
        TST.L   4(A1)
        BLE     .f
        MOVEQ   #1,D3
        MOVEQ   #0,D2
.f:     LEA     8(A1),A0                ; fib_FileName
        MOVEQ   #-1,D1
.l:     ADDQ.L  #1,D1
        TST.B   0(A0,D1.L)
        BNE     .l
        LEA     132(A1),A1              ; fib_DateStamp
        MOVEQ   #1,D0
        BRA     .out
.done:  BSR     plat_dir__close
.end:   MOVEQ   #0,D0
.out:   MOVE.L  (SP)+,A6
        RTS

;! section bss
plat_file__cd0:
        DS.L    1
plat_file__cdset:
        DS.L    1
plat_file__dlock:
        DS.L    1
plat_file__fibm:
        DS.B    264
plat_file__nb:
        DS.B    256
plat_file__nb2:
        DS.B    256
