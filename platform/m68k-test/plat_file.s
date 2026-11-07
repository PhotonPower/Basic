;! module    plat_file
;! provides  plat_fopen plat_fread plat_fwrite plat_fclose plat_fsize plat_fdelete plat_fexists
;! requires  plat_alloc
;! group     file-seq
;! clobbers  D0-D3/A0-A1
;! doc       m68k-test: a RAM disk for the file routines (16 files of up to 16 KiB, names up to 32 characters).

; Errors as AmigaDOS: 205 object not found, 202 object in use (a file open for writing is exclusive,
; reading is shared), 221 disk full. Names are compared without regard to case.
; Entry (64 bytes): +0 used, +2 name length, +4 name (32), +36 data, +40 size, +44 readers, +46 writer.
; Handle (12 bytes): +0 entry (0: free), +4 position, +8 mode (0 read, 1 write, 2 append).

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
plat_fopen:
        MOVEM.L D4/A2-A3,-(SP)
        MOVE.L  D1,D4
        BSR     plat_file__find
        MOVE.L  A1,A2                   ; A2 = entry or 0
        TST.L   D4
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
.err:   MOVEQ   #0,D0
.out:   MOVEM.L (SP)+,D4/A2-A3
        RTS

; D0 = handle, A0 = buffer, D1 = count -> D0 = bytes read (0 at the end)
plat_fread:
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
plat_fwrite:
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
plat_fclose:
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
plat_fsize:
        MOVE.L  D0,A0
        MOVE.L  (A0),A0
        MOVE.L  40(A0),D0
        RTS

; A0 = name, D0 = length -> D0 = 0, or the error (205, 202)
plat_fdelete:
        BSR     plat_file__find
        MOVE.L  A1,D1
        BEQ     .nf
        TST.L   44(A1)
        BNE     .use
        CLR.W   (A1)
        MOVEQ   #0,D0
        RTS
.nf:    MOVE.L  #205,D0
        RTS
.use:   MOVE.L  #202,D0
        RTS

; A0 = name, D0 = length -> D0 = -1 if the file exists, else 0
plat_fexists:
        BSR     plat_file__find
        MOVE.L  A1,D0
        BEQ     .out
        MOVEQ   #-1,D0
.out:   RTS

;! section bss
plat_file__dir:
        DS.B    64*plat_file__FILES
plat_file__hd:
        DS.B    12*plat_file__FILES
