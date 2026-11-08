;! module    plat_file
;! provides  plat_fopen plat_fread plat_fwrite plat_fclose plat_fsize plat_fdelete plat_fexists plat_fseek plat_ftell plat_frename
;! provides  plat_mkdir plat_chdir plat_dir_open plat_dir_next
;! requires  plat_alloc
;! group     file-seq
;! clobbers  D0-D3/A0-A1
;! doc       m68k-test: a RAM disk for the file routines (16 files of up to 16 KiB, names up to 32 characters).

; Every routine leaves garbage in D2, D3 and A1 (the convention allows it, AmigaDOS does it): so the
; tests find callers that rely on them.

; Errors as AmigaDOS: 205 object not found, 202 object in use (a file open for writing is exclusive,
; reading is shared), 221 disk full. Names are compared without regard to case.
; Directories (probe25): an entry with used = 2; names are paths from the root ("dir/file"), the
; current directory (CHDIR) is put in front of every name, a leading "/" goes one level up. A
; directory with entries cannot be deleted (216), deleting one without is RMDIR. No dates (FILES
; shows 1.1.1978).
; Entry (64 bytes): +0 used (1 file, 2 directory), +2 name length, +4 name (32), +36 data, +40 size, +44 readers, +46 writer.
; Handle (12 bytes): +0 entry (0: free), +4 position, +8 mode (0 read, 1 write, 2 append, 3 read and
; write: an existing file is kept, a missing one created).

plat_file__FILES EQU 16
plat_file__CAP   EQU 16384

; A0 = name, D0 = length -> A1 = entry with this name, or 0 (keeps D0/A0)
plat_file__find:
        MOVEM.L D0-D3/A0/A2-A3,-(SP)
        CMP.L   #32,D0
        BLS     .l
        MOVEQ   #32,D0
.l:     LEA     plat_file__dir,A1
        MOVEQ   #plat_file__FILES-1,D1
.e:     TST.W   (A1)
        BEQ     .next
        CMP.W   2(A1),D0
        BNE     .next
        MOVE.L  A0,A2                   ; compare without case
        LEA     4(A1),A3
        MOVE.L  D0,D2
        BRA     .cn
.c:     MOVE.B  (A2)+,D3
        BSR     plat_file__upper
        MOVE.B  D3,-(SP)
        MOVE.B  (A3)+,D3
        BSR     plat_file__upper
        CMP.B   (SP)+,D3
        BNE     .next
.cn:    SUBQ.L  #1,D2
        BPL     .c
        BRA     .out                    ; found
.next:  LEA     64(A1),A1
        DBRA    D1,.e
        SUB.L   A1,A1
.out:   MOVEM.L (SP)+,D0-D3/A0/A2-A3
        RTS

plat_file__upper:
        CMP.B   #'a',D3
        BCS     .out
        CMP.B   #'z',D3
        BHI     .out
        SUB.B   #32,D3
.out:   RTS

; A0 = name, D0 = length, D1 = mode -> D0 = handle, or 0 with D1 = error
plat_file__fopen:
        MOVEM.L D4/A2-A3,-(SP)
        MOVE.L  D1,D4
        BSR     plat_file__find
        MOVE.L  A1,A2                   ; A2 = entry or 0
        MOVE.L  A2,D1
        BEQ     .fil
        CMP.W   #2,(A2)                 ; a directory: object wrong type
        BEQ     .typ
.fil:   TST.L   D4
        BNE     .wr
        MOVE.L  A2,D1                   ; read: the file must exist and not be written
        BEQ     .nf
        TST.W   46(A2)
        BNE     .use
        BRA     .handle
.wr:    MOVE.L  A2,D1                   ; write, append: nobody else may have it open
        BEQ     .new
        TST.L   44(A2)
        BNE     .use
        BRA     .handle
.new:   LEA     plat_file__dir,A1       ; a free entry
        MOVEQ   #plat_file__FILES-1,D1
.fe:    TST.W   (A1)
        BEQ     .got
        LEA     64(A1),A1
        DBRA    D1,.fe
        BRA     .full
.got:   MOVE.L  A1,A2
        TST.L   36(A2)                  ; data block (kept when the file is deleted)
        BNE     .name
        MOVEM.L D0/A0,-(SP)
        MOVE.L  #plat_file__CAP,D0
        JSR     plat_alloc
        MOVE.L  A0,36(A2)
        MOVEM.L (SP)+,D0/A0
        TST.L   36(A2)
        BEQ     .full
.name:  MOVE.W  #1,(A2)
        CMP.L   #32,D0
        BLS     .nl
        MOVEQ   #32,D0
.nl:    MOVE.W  D0,2(A2)
        LEA     4(A2),A1
        BRA     .cn
.cc:    MOVE.B  (A0)+,(A1)+
.cn:    SUBQ.L  #1,D0
        BPL     .cc
        CLR.L   40(A2)
        CLR.L   44(A2)
.handle:
        LEA     plat_file__hd,A3        ; a free handle
        MOVEQ   #plat_file__FILES-1,D1
.fh:    TST.L   (A3)
        BEQ     .hg
        LEA     12(A3),A3
        DBRA    D1,.fh
        BRA     .full
.hg:    MOVE.L  A2,(A3)
        CLR.L   4(A3)
        MOVE.W  D4,8(A3)
        TST.L   D4
        BNE     .w2
        ADDQ.W  #1,44(A2)               ; one more reader
        BRA     .ok
.w2:    MOVE.W  #1,46(A2)
        CMP.L   #2,D4
        BEQ     .app
        CMP.L   #1,D4
        BNE     .ok
        CLR.L   40(A2)                  ; write: empty
        BRA     .ok
.app:   MOVE.L  40(A2),4(A3)            ; append: at the end
.ok:    MOVE.L  A3,D0
        BRA     .out
.nf:    MOVE.L  #205,D1                 ; object not found
        BRA     .err
.use:   MOVE.L  #202,D1                 ; object in use
        BRA     .err
.full:  MOVE.L  #221,D1                 ; disk full
        BRA     .err
.typ:   MOVE.L  #212,D1                 ; object wrong type
.err:   MOVEQ   #0,D0
.out:   MOVEM.L (SP)+,D4/A2-A3
        RTS

; D0 = handle, A0 = buffer, D1 = count -> D0 = bytes read (0 at the end)
plat_file__fread:
        MOVE.L  A2,-(SP)
        MOVE.L  D0,A2                   ; A2 = handle, A1 = entry
        MOVE.L  (A2),A1
        MOVE.L  40(A1),D3               ; available = size - position
        SUB.L   4(A2),D3
        CMP.L   D1,D3
        BCC     .n
        MOVE.L  D3,D1
.n:     MOVE.L  36(A1),A1               ; from data + position
        ADD.L   4(A2),A1
        ADD.L   D1,4(A2)
        MOVE.L  D1,D0
        BRA     .cn
.c:     MOVE.B  (A1)+,(A0)+
.cn:    SUBQ.L  #1,D1
        BPL     .c
        MOVE.L  (SP)+,A2
        RTS

; D0 = handle, A0 = buffer, D1 = count -> D0 = bytes written (fewer: the file is full)
plat_file__fwrite:
        MOVE.L  A2,-(SP)
        MOVE.L  D0,A2                   ; A2 = handle, A1 = entry
        MOVE.L  (A2),A1
        MOVE.L  #plat_file__CAP,D3      ; room = capacity - position
        SUB.L   4(A2),D3
        CMP.L   D1,D3
        BCC     .n
        MOVE.L  D3,D1
.n:     MOVE.L  4(A2),D2                ; D2 = position
        ADD.L   D1,4(A2)
        MOVE.L  D2,D3                   ; the size grows to the end of the write
        ADD.L   D1,D3
        CMP.L   40(A1),D3
        BLS     .sz
        MOVE.L  D3,40(A1)
.sz:    MOVE.L  36(A1),A1
        ADD.L   D2,A1
        MOVE.L  D1,D0
        BRA     .cn
.c:     MOVE.B  (A0)+,(A1)+
.cn:    SUBQ.L  #1,D1
        BPL     .c
        MOVE.L  (SP)+,A2
        RTS

; D0 = handle
plat_file__fclose:
        MOVE.L  D0,A0
        MOVE.L  (A0),A1
        TST.W   8(A0)
        BNE     .w
        SUBQ.W  #1,44(A1)
        BRA     .free
.w:     CLR.W   46(A1)
.free:  CLR.L   (A0)
        RTS

; D0 = handle -> D0 = length of the file
plat_file__fsize:
        MOVE.L  D0,A0
        MOVE.L  (A0),A0
        MOVE.L  40(A0),D0
        RTS

; D0 = handle, D1 = position (within the file)
plat_file__fseek:
        MOVE.L  D0,A0
        MOVE.L  D1,4(A0)
        MOVEQ   #0,D0
        RTS

; D0 = handle -> D0 = position
plat_file__ftell:
        MOVE.L  D0,A0
        MOVE.L  4(A0),D0
        RTS

; A0 = old name, D0 = length, A1 = new name, D1 = length -> D0 = 0, or the error (205, 203, 202)
plat_file__frename:
        MOVEM.L D4-D5/A2-A3,-(SP)
        MOVE.L  A1,A3                   ; A3/D5 = new name
        MOVE.L  D1,D5
        BSR     plat_file__find
        MOVE.L  A1,A2                   ; A2 = entry of the old name
        MOVE.L  A2,D4
        BEQ     .nf
        MOVE.L  A3,A0
        MOVE.L  D5,D0
        BSR     plat_file__find
        MOVE.L  A1,D4
        BNE     .ex
        TST.L   44(A2)
        BNE     .use
        CMP.L   #32,D5
        BLS     .nl
        MOVEQ   #32,D5
.nl:    MOVE.W  D5,2(A2)
        LEA     4(A2),A1
        BRA     .cn
.cc:    MOVE.B  (A3)+,(A1)+
.cn:    SUBQ.L  #1,D5
        BPL     .cc
        MOVEQ   #0,D0
        BRA     .out
.nf:    MOVE.L  #205,D0                 ; object not found
        BRA     .out
.ex:    MOVE.L  #203,D0                 ; object already exists
        BRA     .out
.use:   MOVE.L  #202,D0                 ; object in use
.out:   MOVEM.L (SP)+,D4-D5/A2-A3
        RTS

; A0 = name, D0 = length -> D0 = 0, or the error (205, 202)
plat_file__fdelete:
        BSR     plat_file__find
        MOVE.L  A1,D1
        BEQ     .nf
        TST.L   44(A1)
        BNE     .use
        CMP.W   #2,(A1)
        BNE     .del
        MOVE.L  A1,-(SP)                ; a directory: empty?
        LEA     4(A1),A0
        MOVEQ   #0,D0
        MOVE.W  2(A1),D0
        MOVEQ   #0,D1
        BSR     plat_file__child
        MOVE.L  (SP)+,A1
        TST.L   D0
        BNE     .full
.del:   CLR.W   (A1)
        MOVEQ   #0,D0
        RTS
.nf:    MOVE.L  #205,D0
        RTS
.use:   MOVE.L  #202,D0
        RTS
.full:  MOVE.L  #216,D0                 ; directory not empty
        RTS

; A0 = path, D0 = its length (0: the root), D1 = first index -> D0 = the first entry inside it
; from there, or 0; D1 = its index. Inside: the name starts with path + "/" (the root: no
; "/" and no ":") and has no further "/".
plat_file__child:
        MOVEM.L D2-D5/A2-A3,-(SP)
        MOVE.L  D0,D4                   ; D4 = length of the path
        MOVE.L  D1,D5                   ; D5 = index
.e:     CMP.L   #plat_file__FILES,D5
        BGE     .none
        MOVE.L  D5,D0
        LSL.L   #6,D0
        LEA     plat_file__dir,A1
        ADD.L   D0,A1
        TST.W   (A1)
        BEQ     .next
        MOVEQ   #0,D2
        MOVE.W  2(A1),D2                ; D2 = length of the name
        LEA     4(A1),A2
        TST.L   D4
        BEQ     .rest
        MOVE.L  D4,D0                   ; path + "/" at the start
        ADDQ.L  #1,D0
        CMP.L   D0,D2
        BLE     .next
        MOVE.L  A0,A3
        MOVE.L  D4,D1
        BRA     .cn
.c:     MOVE.B  (A3)+,D3
        BSR     plat_file__upper
        MOVE.B  D3,-(SP)
        MOVE.B  (A2)+,D3
        BSR     plat_file__upper
        CMP.B   (SP)+,D3
        BNE     .next
.cn:    SUBQ.L  #1,D1
        BPL     .c
        CMP.B   #'/',(A2)+
        BNE     .next
        SUB.L   D4,D2
        SUBQ.L  #1,D2
.rest:  MOVE.L  A2,A3                   ; no "/" (and in the root no ":") in the rest
        MOVE.L  D2,D1
        BRA     .rn
.r:     MOVE.B  (A3)+,D3
        CMP.B   #'/',D3
        BEQ     .next
        TST.L   D4
        BNE     .rn
        CMP.B   #':',D3
        BEQ     .next
.rn:    SUBQ.L  #1,D1
        BPL     .r
        MOVE.L  A1,D0                   ; found
        MOVE.L  D5,D1
        MOVE.L  A2,plat_file__drest
        MOVE.L  D2,plat_file__drlen
        BRA     .out
.next:  ADDQ.L  #1,D5
        BRA     .e
.none:  MOVEQ   #0,D0
.out:   MOVEM.L (SP)+,D2-D5/A2-A3
        RTS

; A0 = name, D0 = length -> A0 = the path from the root (one of two buffers in turn), D0 = its length
plat_file__norm:
        MOVEM.L D2-D3/A2-A3,-(SP)
        MOVE.L  A0,A2                   ; A2/D2 = name
        MOVE.L  D0,D2
        EORI.W  #1,plat_file__nsel      ; the buffer
        LEA     plat_file__nbuf,A3
        TST.W   plat_file__nsel
        BEQ     .b
        LEA     64(A3),A3
.b:     MOVE.L  A2,A0                   ; with a device ("dh0:x") as it is
        MOVE.L  D2,D0
        BRA     .dn
.d:     CMP.B   #':',(A0)+
        BEQ     .asis
.dn:    SUBQ.L  #1,D0
        BPL     .d
        MOVE.L  plat_file__cwdl,D3      ; D3 = length so far: the current directory
        LEA     plat_file__cwd,A0
        MOVE.L  A3,A1
        MOVE.L  D3,D0
        BRA     .wn
.w:     MOVE.B  (A0)+,(A1)+
.wn:    SUBQ.L  #1,D0
        BPL     .w
.up:    TST.L   D2                      ; "/" at the start: one level up
        BEQ     .done
        CMP.B   #'/',(A2)
        BNE     .app
        ADDQ.L  #1,A2
        SUBQ.L  #1,D2
.cut:   TST.L   D3
        BEQ     .up
        SUBQ.L  #1,D3
        CMP.B   #'/',0(A3,D3.L)
        BNE     .cut
        BRA     .up
.app:   TST.L   D3                      ; then "/" and the name
        BEQ     .an
        MOVE.B  #'/',0(A3,D3.L)
        ADDQ.L  #1,D3
.an:    CMP.L   #63,D3
        BGE     .done
        MOVE.B  (A2)+,0(A3,D3.L)
        ADDQ.L  #1,D3
        SUBQ.L  #1,D2
        BNE     .an
        BRA     .done
.asis:  MOVEQ   #0,D3
.ac:    CMP.L   D2,D3
        BGE     .done
        CMP.L   #63,D3
        BGE     .done
        MOVE.B  0(A2,D3.L),0(A3,D3.L)
        ADDQ.L  #1,D3
        BRA     .ac
.done:  MOVE.L  A3,A0
        MOVE.L  D3,D0
        MOVEM.L (SP)+,D2-D3/A2-A3
        RTS

; A0 = name, D0 = length -> D0 = 0, or the error. An existing directory is no error (probe25 T3).
plat_file__mkdir:
        BSR     plat_file__find
        MOVE.L  A1,D1
        BEQ     .new
        CMP.W   #2,(A1)
        BNE     .ex
        MOVEQ   #0,D0
        RTS
.ex:    MOVE.L  #203,D0                 ; object already exists
        RTS
.new:   MOVE.L  D0,D1                   ; the parent must exist: up to the last "/"
.p:     SUBQ.L  #1,D1
        BMI     .root
        CMP.B   #'/',0(A0,D1.L)
        BNE     .p
        MOVEM.L D0/A0,-(SP)
        MOVE.L  D1,D0
        BSR     plat_file__find
        MOVEM.L (SP)+,D0/A0
        MOVE.L  A1,D1
        BEQ     .nf
        CMP.W   #2,(A1)
        BNE     .nf
.root:  LEA     plat_file__dir,A1       ; a free entry
        MOVEQ   #plat_file__FILES-1,D1
.fe:    TST.W   (A1)
        BEQ     .got
        LEA     64(A1),A1
        DBRA    D1,.fe
        MOVE.L  #221,D0                 ; disk full
        RTS
.got:   MOVE.W  #2,(A1)
        CMP.L   #32,D0
        BLS     .nl
        MOVEQ   #32,D0
.nl:    MOVE.W  D0,2(A1)
        CLR.L   40(A1)
        CLR.L   44(A1)
        LEA     4(A1),A1
        BRA     .cn
.cc:    MOVE.B  (A0)+,(A1)+
.cn:    SUBQ.L  #1,D0
        BPL     .cc
        MOVEQ   #0,D0
        RTS
.nf:    MOVE.L  #205,D0
        RTS

; A0 = path, D0 = length -> D0 = 0 (the current directory), or 205
plat_file__chdir:
        TST.L   D0
        BEQ     .set                    ; the root
        BSR     plat_file__find
        MOVE.L  A1,D1
        BEQ     .nf
        CMP.W   #2,(A1)
        BNE     .nf
.set:   CMP.L   #63,D0
        BLS     .l
        MOVEQ   #63,D0
.l:     MOVE.L  D0,plat_file__cwdl
        LEA     plat_file__cwd,A1
        BRA     .cn
.cc:    MOVE.B  (A0)+,(A1)+
.cn:    SUBQ.L  #1,D0
        BPL     .cc
        MOVEQ   #0,D0
        RTS
.nf:    MOVE.L  #205,D0
        RTS

; A0 = path, D0 = length -> D0 = 0 (listing begins), or 205
plat_file__dopen:
        TST.L   D0
        BEQ     .ok
        BSR     plat_file__find
        MOVE.L  A1,D1
        BEQ     .nf
        CMP.W   #2,(A1)
        BNE     .nf
.ok:    CMP.L   #63,D0
        BLS     .l
        MOVEQ   #63,D0
.l:     MOVE.L  D0,plat_file__dpl
        LEA     plat_file__dpath,A1
        BRA     .cn
.cc:    MOVE.B  (A0)+,(A1)+
.cn:    SUBQ.L  #1,D0
        BPL     .cc
        CLR.L   plat_file__dpos
        MOVEQ   #0,D0
        RTS
.nf:    MOVE.L  #205,D0
        RTS

; -> D0 = 0 at the end, else 1 with A0 = name (C string), D1 = its length, D2 = size, D3 = 1 for a
; directory, A1 = date (days since 1.1.1978, minutes, ticks: always 0 here)
plat_file__dnext:
        LEA     plat_file__dpath,A0
        MOVE.L  plat_file__dpl,D0
        MOVE.L  plat_file__dpos,D1
        BSR     plat_file__child
        TST.L   D0
        BEQ     .out
        ADDQ.L  #1,D1
        MOVE.L  D1,plat_file__dpos
        MOVE.L  D0,A1
        MOVEQ   #0,D2
        MOVEQ   #0,D3
        CMP.W   #2,(A1)
        BNE     .f
        MOVEQ   #1,D3
        BRA     .n
.f:     MOVE.L  40(A1),D2
.n:     MOVE.L  plat_file__drest,A0     ; the name as a C string
        LEA     plat_file__dname,A1
        MOVE.L  plat_file__drlen,D1
        MOVE.L  D1,D0
        BRA     .cn
.cc:    MOVE.B  (A0)+,(A1)+
.cn:    SUBQ.L  #1,D0
        BPL     .cc
        CLR.B   (A1)
        LEA     plat_file__dname,A0
        LEA     plat_file__date,A1
        MOVEQ   #1,D0
.out:   RTS

; A0 = name, D0 = length -> D0 = -1 if the file exists, else 0
plat_file__fexists:
        BSR     plat_file__find
        MOVE.L  A1,D0
        BEQ     .out
        MOVEQ   #-1,D0
.out:   RTS


; ---- entry points: the work, then garbage in D2, D3, A1 ------------------------------------------
plat_fopen:
        BSR     plat_file__norm
        BSR     plat_file__fopen
        BRA     plat_file__trash
plat_fread:
        BSR     plat_file__fread
        BRA     plat_file__trash
plat_fwrite:
        BSR     plat_file__fwrite
        BRA     plat_file__trash
plat_fclose:
        BSR     plat_file__fclose
        BRA     plat_file__trash
plat_fsize:
        BSR     plat_file__fsize
        BRA     plat_file__trash
plat_fdelete:
        BSR     plat_file__norm
        BSR     plat_file__fdelete
        BRA     plat_file__trash
plat_fexists:
        BSR     plat_file__norm
        BSR     plat_file__fexists
        BRA     plat_file__trash
plat_fseek:
        BSR     plat_file__fseek
        BRA     plat_file__trash
plat_ftell:
        BSR     plat_file__ftell
        BRA     plat_file__trash
plat_frename:
        MOVEM.L D1/A1,-(SP)             ; the new name (norm uses A1)
        BSR     plat_file__norm
        MOVE.L  (SP)+,D1
        MOVE.L  (SP)+,A1
        MOVEM.L D0/A0,-(SP)
        MOVE.L  A1,A0
        MOVE.L  D1,D0
        BSR     plat_file__norm
        MOVE.L  A0,A1
        MOVE.L  D0,D1
        MOVEM.L (SP)+,D0/A0
        BSR     plat_file__frename
        BRA     plat_file__trash
plat_mkdir:
        BSR     plat_file__norm
        BSR     plat_file__mkdir
        BRA     plat_file__trash
plat_chdir:
        BSR     plat_file__norm
        BSR     plat_file__chdir
        BRA     plat_file__trash
plat_dir_open:
        BSR     plat_file__norm
        BRA     plat_file__dopen
plat_dir_next:
        BRA     plat_file__dnext
plat_file__trash:
        MOVE.L  #$DEADBEEF,D2
        MOVE.L  D2,D3
        MOVE.L  D2,A1
        RTS

;! section bss
plat_file__dir:
        DS.B    64*plat_file__FILES
plat_file__cwdl:
        DS.L    1
plat_file__cwd:
        DS.B    64
plat_file__nsel:
        DS.W    1
plat_file__nbuf:
        DS.B    128
plat_file__dpos:
        DS.L    1
plat_file__dpl:
        DS.L    1
plat_file__dpath:
        DS.B    64
plat_file__drest:
        DS.L    1
plat_file__drlen:
        DS.L    1
plat_file__dname:
        DS.B    66
plat_file__date:
        DS.L    3
plat_file__hd:
        DS.B    12*plat_file__FILES
